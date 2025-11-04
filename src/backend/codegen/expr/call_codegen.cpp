//===--- call_codegen.cpp - Call Expression CodeGen -------------*- C++ -*-===//
//
// 函数调用代码生成（严格遵循ARCHITECTURE.md）
// 支持：普通函数、builtin函数、闭包调用
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/generic_types.h"
#include "middleend/types/type_system.h"
#include <iostream>

namespace pawc {

void ExprCodeGen::visit(CallExpr* node) {
    auto& builder = context_->getBuilder();
    
    // === 🔧 Bug Fix: 处理接口方法调用 ===
    if (node->isMethodCall()) {
        std::cerr << "[MethodCall] Calling method: " << node->getMethodTarget() << std::endl;
        
        // 查找方法函数
        llvm::Function* method = context_->getModule()->getFunction(node->getMethodTarget());
        if (!method) {
            std::cerr << "[MethodCall] Method not found!" << std::endl;
            result_ = nullptr;
            return;
        }
        
        std::cerr << "[MethodCall] Method found, generating receiver" << std::endl;
        
        // 生成receiver（self）
        node->getReceiver()->accept(this);
        llvm::Value* receiver = result_;
        
        if (!receiver) {
            std::cerr << "[MethodCall] Receiver is null!" << std::endl;
            result_ = nullptr;
            return;
        }
        
        std::cerr << "[MethodCall] Receiver generated, is pointer: " << receiver->getType()->isPointerTy() << std::endl;
        
        // 🔧 引用类型支持：检查方法的第一个参数是否是引用类型
        // 如果是引用类型，需要传递指针而不是值
        llvm::Value* receiver_arg = receiver;
        
        if (method->arg_size() > 0) {
            llvm::Type* first_param_type = method->getArg(0)->getType();
            
            // 如果第一个参数是指针类型（引用），需要获取receiver的地址
            if (first_param_type->isPointerTy() && !receiver->getType()->isPointerTy()) {
                // receiver是值，但参数需要指针
                // 创建一个临时变量存储receiver，并传递其地址
                auto& builder = context_->getBuilder();
                llvm::AllocaInst* temp = builder.CreateAlloca(receiver->getType(), nullptr, "receiver.tmp");
                builder.CreateStore(receiver, temp);
                receiver_arg = temp;
            } else if (!first_param_type->isPointerTy() && receiver->getType()->isPointerTy()) {
                // receiver是指针，但参数需要值
                // Load值（LLVM 21不透明指针，需要从receiver类型推导）
                auto& builder = context_->getBuilder();
                // 从Paw类型系统获取实际类型
                Type* receiver_paw_type = node->getReceiver()->getType();
                llvm::Type* receiver_llvm_type = context_->getLLVMType(receiver_paw_type);
                receiver_arg = builder.CreateLoad(receiver_llvm_type, receiver, "receiver.val");
            }
        }
        
        // 生成参数列表
        std::vector<llvm::Value*> args;
        
        // 🔧 智能参数传递：只在方法期望receiver时才传递
        // 如果方法有参数（期望receiver），传递receiver作为第一个参数
        if (method->arg_size() > 0) {
            args.push_back(receiver_arg);
        }
        
        // 添加其他参数
        for (const auto& arg : node->getArgs()) {
            arg->accept(this);
            if (result_) {
                args.push_back(result_);
            }
        }
        
        // 调用方法
        result_ = builder.CreateCall(method, args);
        return;
    }
    
    // 🔧 泛型函数调用处理
    if (node->hasTypeArgs()) {
        if (auto* ident = dynamic_cast<IdentifierExpr*>(node->getCallee())) {
            // 泛型函数调用: identity<i32>(42)
            std::cerr << "[DEBUG] 检测到泛型函数调用: " << ident->getName() << std::endl;
            result_ = generateGenericFunctionCall(ident->getName(), 
                                                  node->getTypeArgs(), 
                                                  node->getArgs());
            std::cerr << "[DEBUG] 泛型函数调用完成" << std::endl;
            return;
        }
    }
    
    // 🔧 M7: 检查是否是泛型enum构造
    if (auto* static_access = dynamic_cast<StaticAccessExpr*>(node->getCallee())) {
        // 这是静态访问调用：Option::Some(42)
        Type* callee_type = static_access->getType();
        
        if (callee_type && callee_type->isFunction()) {
            // 这是enum构造器！
            FunctionType* func_type = static_cast<FunctionType*>(callee_type);
            Type* return_type = func_type->getReturnType();
            
            if (return_type && return_type->isEnum()) {
                // 构造enum值
                EnumType* enum_type = static_cast<EnumType*>(return_type);
                
                // 查找variant索引
                std::string variant_name = static_access->getMember();
                int variant_index = -1;
                Type* variant_data_type = nullptr;
                
                const auto& variants = enum_type->getVariants();
                for (size_t i = 0; i < variants.size(); i++) {
                    if (variants[i].first == variant_name) {
                        variant_index = static_cast<int>(i);
                        variant_data_type = variants[i].second;
                        break;
                    }
                }
                
                if (variant_index < 0) {
                    result_ = nullptr;
                    return;
                }
                
                // 获取enum的LLVM类型
                llvm::Type* enum_llvm_type = context_->getLLVMType(enum_type);
                
                // 创建enum struct: {i32 variant_index, data}
                llvm::Value* enum_value = llvm::UndefValue::get(enum_llvm_type);
                
                // 设置variant索引
                enum_value = builder.CreateInsertValue(
                    enum_value,
                    builder.getInt32(variant_index),
                    {0}
                );
                
                // 如果有关联数据，设置数据
                if (variant_data_type && !node->getArgs().empty()) {
                    llvm::Value* arg_value = nullptr;
                    
                    // 🔧 多参数支持：检查是否需要包装为元组
                    if (variant_data_type->isTuple()) {
                        // 多参数：包装为元组
                        TupleType* tuple_type = static_cast<TupleType*>(variant_data_type);
                        std::vector<llvm::Value*> tuple_values;
                        
                        for (size_t i = 0; i < node->getArgs().size(); ++i) {
                            node->getArgs()[i]->accept(this);
                            tuple_values.push_back(result_);
                        }
                        
                        // 构造元组值
                        llvm::Type* tuple_llvm_type = context_->getLLVMType(tuple_type);
                        llvm::Value* tuple_val = llvm::UndefValue::get(tuple_llvm_type);
                        
                        for (size_t i = 0; i < tuple_values.size(); ++i) {
                            tuple_val = builder.CreateInsertValue(tuple_val, tuple_values[i], {static_cast<unsigned>(i)});
                        }
                        
                        arg_value = tuple_val;
                    } else {
                        // 单参数
                        node->getArgs()[0]->accept(this);
                        arg_value = result_;
                    }
                    
                    if (arg_value) {
                        enum_value = builder.CreateInsertValue(
                            enum_value,
                            arg_value,
                            {1}
                        );
                    }
                }
                
                result_ = enum_value;
                return;
            }
        }
    }
    
    // === Step 1: 检查是否是闭包调用 ===
    Type* callee_type = node->getCallee()->getType();
    
    if (callee_type && callee_type->getKind() == Type::Kind::Function) {
        // === 闭包调用！===
        
        // 从FunctionType获取捕获标记（Sema设置）
        auto* func_type_ast = static_cast<FunctionType*>(callee_type);
        bool has_captures = func_type_ast->hasCaptures();
        
        // 生成闭包对象/函数指针
        node->getCallee()->accept(this);
        llvm::Value* closure_value = result_;
        
        if (!closure_value) {
            result_ = nullptr;
            return;
        }
        
        // 生成参数
        std::vector<llvm::Value*> args;
        for (const auto& arg : node->getArgs()) {
            arg->accept(this);
            if (result_) {
                args.push_back(result_);
            }
        }
        
        // === 闭包调用逻辑 ===
        // 构建用户层FunctionType（不包含上下文）
        std::vector<llvm::Type*> param_types_llvm;
        for (Type* param_type : func_type_ast->getParamTypes()) {
            param_types_llvm.push_back(context_->getLLVMType(param_type));
        }
        llvm::Type* return_type_llvm = context_->getLLVMType(func_type_ast->getReturnType());
        
        llvm::Value* callable = nullptr;
        llvm::FunctionType* fn_type = nullptr;
        std::vector<llvm::Value*> final_args;
        
        // Case 1: 直接是llvm::Function*（无捕获闭包立即调用）
        if (llvm::isa<llvm::Function>(closure_value)) {
            callable = closure_value;
            fn_type = llvm::cast<llvm::Function>(closure_value)->getFunctionType();
            final_args = args;
        }
        // Case 2: 是指针（alloca，从变量调用）
        else if (closure_value->getType()->isPointerTy()) {
            // 从alloca load闭包值
            llvm::Value* loaded_closure = builder.CreateLoad(
                llvm::PointerType::getUnqual(builder.getContext()),
                closure_value,
                "loaded_closure"
            );
            
            // 判断是无捕获闭包还是有捕获闭包
            // 策略：
            // - 如果明确知道有捕获（closure_expr存在且has_captures为true），作为结构体处理
            // - 否则（无捕获或无法确定），默认作为函数指针处理
            
            if (has_captures) {
                // Case 2b: 明确有捕获 - 作为闭包结构体指针处理
                llvm::Value* closure_struct_ptr = loaded_closure;
                
                // 从闭包结构体提取函数指针（字段0）
                // 方法：将闭包结构体指针cast为ptr*（指针数组），然后取第一个元素
                llvm::Type* ptr_ptr_type = llvm::PointerType::getUnqual(
                    llvm::PointerType::getUnqual(builder.getContext())
                );
                llvm::Value* ptr_array = builder.CreateBitCast(
                    closure_struct_ptr,
                    ptr_ptr_type,
                    "closure_as_ptr_array"
                );
                
                // Load第一个指针（函数指针）
                llvm::Value* fn_ptr = builder.CreateLoad(
                    llvm::PointerType::getUnqual(builder.getContext()),
                    ptr_array,
                    "closure_fn"
                );
                
                callable = fn_ptr;
                
                // 构建FunctionType（包含上下文参数）
                std::vector<llvm::Type*> param_types_with_ctx;
                param_types_with_ctx.push_back(llvm::PointerType::getUnqual(builder.getContext()));
                for (auto* pt : param_types_llvm) {
                    param_types_with_ctx.push_back(pt);
                }
                fn_type = llvm::FunctionType::get(return_type_llvm, param_types_with_ctx, false);
                
                // 传递闭包结构体指针作为第一个参数
                final_args.push_back(closure_struct_ptr);
                for (auto* arg : args) {
                    final_args.push_back(arg);
                }
            }
            else {
                // Case 2a: 无捕获或无法确定 - 默认作为函数指针处理
                callable = loaded_closure;
                
                // 构建简单的FunctionType（无上下文参数）
                fn_type = llvm::FunctionType::get(return_type_llvm, param_types_llvm, false);
                final_args = args;
            }
        }
        else {
            result_ = nullptr;
            return;
        }
        
        if (!callable || !fn_type) {
            result_ = nullptr;
            return;
        }
        
        // 创建调用
        if (fn_type->getReturnType()->isVoidTy()) {
            // void返回类型不应该有名字
            result_ = builder.CreateCall(fn_type, callable, final_args);
        } else {
            result_ = builder.CreateCall(fn_type, callable, final_args, "closure_call");
        }
        return;
    }
    
    // === Step 2: 普通函数调用（包括builtin）===
    std::string func_name;
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node->getCallee())) {
        func_name = ident->getName();
    } else {
        // 复杂的callee表达式（如方法调用），暂不支持
        result_ = nullptr;
        return;
    }
    
    // 生成参数并收集类型
    std::vector<llvm::Value*> arg_values;
    std::vector<Type*> arg_types;
    
    for (const auto& arg : node->getArgs()) {
        arg->accept(this);
        if (result_) {
            arg_values.push_back(result_);
            
            // 收集参数类型（从AST获取）
            Type* arg_type = arg->getType();
            if (arg_type) {
                arg_types.push_back(arg_type);
            }
        } else {
            // 参数生成失败
            result_ = nullptr;
            return;
        }
    }
    
    // === 特殊处理: ok(value) - Result构造器 ===
    if (func_name == "ok" && arg_values.size() == 1) {
        // ok(value) 返回 Result<T> = { true, value, nullptr }
        // 类型从CallExpr的类型获取（Sema已设置）
        Type* result_type_ast = node->getType();
        
        llvm::Type* result_llvm_type = context_->getLLVMType(result_type_ast);
        
        // 创建Result值: { i1 is_ok, T value, ptr error_message }
        llvm::Value* result_value = llvm::UndefValue::get(result_llvm_type);
        
        // 设置is_ok = true
        result_value = builder.CreateInsertValue(result_value,
            llvm::ConstantInt::get(builder.getInt1Ty(), 1),  // true
            0);
        
        // 设置value
        result_value = builder.CreateInsertValue(result_value, arg_values[0], 1);
        
        // 设置error = nullptr
        result_value = builder.CreateInsertValue(result_value,
            llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(builder.getContext())
            ),
            2);
        
        result_ = result_value;
        return;
    }
    
    // === 特殊处理: err(message) - Result构造器 ===
    if (func_name == "err" && arg_values.size() == 1) {
        // err(message) 返回 Result<T> = { false, undef, message }
        // T从CallExpr的类型推导（Sema已设置）
        Type* result_type_ast = node->getType();
        
        llvm::Type* result_llvm_type = context_->getLLVMType(result_type_ast);
        
        // 创建Result值: { i1 is_ok, T value, ptr error_message }
        llvm::Value* result_value = llvm::UndefValue::get(result_llvm_type);
        
        // 设置is_ok = false
        result_value = builder.CreateInsertValue(result_value,
            llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // false
            0);
        
        // value设置为undef（不使用）
        
        // 设置error = message
        result_value = builder.CreateInsertValue(result_value, arg_values[0], 2);
        
        result_ = result_value;
        return;
    }
    
    // === 特殊处理: some(value) - Optional构造器 ===
    if (func_name == "some" && arg_values.size() == 1) {
        // some(value) 返回 Optional<T> = { true, value }
        // T从value的类型推导（Sema已设置）
        Type* optional_type_ast = node->getType();
        
        llvm::Type* optional_llvm_type = context_->getLLVMType(optional_type_ast);
        
        // 创建Optional值: { i1 has_value, T value }
        llvm::Value* optional_value = llvm::UndefValue::get(optional_llvm_type);
        
        // 设置has_value = true
        optional_value = builder.CreateInsertValue(optional_value,
            llvm::ConstantInt::get(builder.getInt1Ty(), 1),  // true
            0);
        
        // 设置value
        optional_value = builder.CreateInsertValue(optional_value, arg_values[0], 1);
        
        result_ = optional_value;
        return;
    }
    
    // 检查是否是builtin函数（完整支持所有18种类型）
    if (func_name == "println" || func_name == "print" || func_name == "to_string") {
        if (arg_types.size() == 1) {
            // 获取类型签名
            std::string type_suffix = context_->getTypeSignature(arg_types);
            
            if (func_name == "println" || func_name == "print") {
                // 特殊处理f128：需要将fp128转换为struct传递
                if (arg_types[0]->getKind() == Type::Kind::F128) {
                    llvm::Value* fp128_value = arg_values[0];
                    
                    // 将fp128转换为i128（位表示）
                    llvm::Value* bits = builder.CreateBitCast(fp128_value,
                        llvm::Type::getInt128Ty(builder.getContext()));
                    
                    // 提取低64位和高64位
                    llvm::Value* low = builder.CreateTrunc(bits, builder.getInt64Ty());
                    llvm::Value* high = builder.CreateTrunc(
                        builder.CreateLShr(bits, llvm::ConstantInt::get(
                            llvm::Type::getInt128Ty(builder.getContext()), 64)),
                        builder.getInt64Ty()
                    );
                    
                    // 创建struct {i64, i64}
                    llvm::StructType* f128_struct = llvm::StructType::get(builder.getContext(), {
                        builder.getInt64Ty(),
                        builder.getInt64Ty()
                    });
                    
                    llvm::Value* struct_val = llvm::UndefValue::get(f128_struct);
                    struct_val = builder.CreateInsertValue(struct_val, low, {0});
                    struct_val = builder.CreateInsertValue(struct_val, high, {1});
                    
                    // 声明paw_print_f128
                    llvm::Function* print_func = context_->getRuntimeFunction("paw_print_f128");
                    if (!print_func) {
                        auto* func_type = llvm::FunctionType::get(
                            builder.getVoidTy(),
                            {f128_struct},
                            false
                        );
                        print_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            "paw_print_f128",
                            context_->getModule()
                        );
                        context_->registerRuntimeFunction("paw_print_f128", print_func);
                    }
                    
                    builder.CreateCall(print_func, {struct_val});
                } else {
                    // 其他类型：正常处理
                    std::string print_func_name = "paw_print_" + type_suffix;
                    llvm::Function* print_func = context_->getRuntimeFunction(print_func_name);
                    if (!print_func) {
                        auto* func_type = llvm::FunctionType::get(
                            builder.getVoidTy(),
                            {arg_values[0]->getType()},
                            false
                        );
                        print_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            print_func_name,
                            context_->getModule()
                        );
                        context_->registerRuntimeFunction(print_func_name, print_func);
                    }
                    builder.CreateCall(print_func, {arg_values[0]});
                }
                
                // 如果是println，还要输出换行
                if (func_name == "println") {
                    llvm::Function* newline_func = context_->getRuntimeFunction("paw_print_newline");
                    if (!newline_func) {
                        auto* nl_type = llvm::FunctionType::get(builder.getVoidTy(), {}, false);
                        newline_func = llvm::Function::Create(
                            nl_type,
                            llvm::Function::ExternalLinkage,
                            "paw_print_newline",
                            context_->getModule()
                        );
                        context_->registerRuntimeFunction("paw_print_newline", newline_func);
                    }
                    builder.CreateCall(newline_func, {});
                }
                
                result_ = llvm::ConstantInt::get(builder.getInt1Ty(), 0); // void返回
                return;
            } else if (func_name == "to_string") {
                // to_string返回string (ptr)
                std::string to_str_func_name = "paw_" + type_suffix + "_to_string";
                llvm::Function* to_str_func = context_->getRuntimeFunction(to_str_func_name);
                if (!to_str_func) {
                    auto* string_type = llvm::PointerType::getUnqual(builder.getContext());
                    auto* func_type = llvm::FunctionType::get(
                        string_type,
                        {arg_values[0]->getType()},
                        false
                    );
                    to_str_func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        to_str_func_name,
                        context_->getModule()
                    );
                    context_->registerRuntimeFunction(to_str_func_name, to_str_func);
                }
                result_ = builder.CreateCall(to_str_func, {arg_values[0]});
                return;
            }
        }
    } else if (func_name == "len") {
        if (arg_types.size() == 1) {
            std::string type_suffix = context_->getTypeSignature(arg_types);
            
            std::string len_func_name = "paw_" + type_suffix + "_len";
            llvm::Function* len_func = context_->getRuntimeFunction(len_func_name);
            if (!len_func) {
                auto* u64_type = builder.getInt64Ty();
                auto* func_type = llvm::FunctionType::get(
                    u64_type,
                    {arg_values[0]->getType()},
                    false
                );
                len_func = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    len_func_name,
                    context_->getModule()
                );
                context_->registerRuntimeFunction(len_func_name, len_func);
            }
            result_ = builder.CreateCall(len_func, {arg_values[0]});
            return;
        }
    } else if (func_name == "panic") {
        if (arg_types.size() == 1) {
            llvm::Function* panic_func = context_->getRuntimeFunction("paw_panic");
            if (!panic_func) {
                auto* string_type = llvm::PointerType::getUnqual(builder.getContext());
                auto* func_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {string_type},
                    false
                );
                panic_func = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    "paw_panic",
                    context_->getModule()
                );
                context_->registerRuntimeFunction("paw_panic", panic_func);
            }
            builder.CreateCall(panic_func, {arg_values[0]});
            builder.CreateUnreachable();
            result_ = nullptr; // 不会返回
            return;
        }
    } else if (func_name == "assert" || func_name == "debug_assert") {
        if (arg_types.size() == 2) {
            std::string runtime_name = func_name == "assert" ? "paw_assert" : "paw_debug_assert";
            llvm::Function* assert_func = context_->getRuntimeFunction(runtime_name);
            if (!assert_func) {
                auto* bool_type = builder.getInt1Ty();
                auto* string_type = llvm::PointerType::getUnqual(builder.getContext());
                auto* i32_type = builder.getInt32Ty();
                auto* func_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {bool_type, string_type, string_type, i32_type},
                    false
                );
                assert_func = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    runtime_name,
                    context_->getModule()
                );
                context_->registerRuntimeFunction(runtime_name, assert_func);
            }
            auto* file_str = builder.CreateGlobalString("paw_code");
            auto* line_val = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
            builder.CreateCall(assert_func, {arg_values[0], arg_values[1], file_str, line_val});
            result_ = llvm::ConstantInt::get(builder.getInt1Ty(), 0);
            return;
        }
    } else if (func_name == "unreachable") {
        if (arg_types.empty()) {
            llvm::Function* unreachable_func = context_->getRuntimeFunction("paw_unreachable");
            if (!unreachable_func) {
                auto* string_type = llvm::PointerType::getUnqual(builder.getContext());
                auto* i32_type = builder.getInt32Ty();
                auto* func_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {string_type, i32_type},
                    false
                );
                unreachable_func = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    "paw_unreachable",
                    context_->getModule()
                );
                context_->registerRuntimeFunction("paw_unreachable", unreachable_func);
            }
            auto* file_str = builder.CreateGlobalString("paw_code");
            auto* line_val = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
            builder.CreateCall(unreachable_func, {file_str, line_val});
            builder.CreateUnreachable();
            result_ = nullptr;
            return;
        }
    }
    
    // 查找普通函数
    llvm::Function* func = context_->lookupFunctionWithTypes(func_name, arg_types);
    
    if (!func) {
        // 尝试查找非重载函数（用户定义的函数）
        func = context_->lookupFunction(func_name);
    }
    
    if (!func) {
        // 函数未找到
        result_ = nullptr;
        return;
    }
    
    // 🔧 Fix: 确保参数类型与函数签名匹配
    // 对于struct类型，如果类型不匹配，使用指针bitcast转换
    std::vector<llvm::Value*> final_arg_values;
    auto func_arg_it = func->arg_begin();
    
    for (size_t i = 0; i < arg_values.size() && func_arg_it != func->arg_end(); ++i, ++func_arg_it) {
        llvm::Value* arg_val = arg_values[i];
        llvm::Type* expected_type = func_arg_it->getType();
        llvm::Type* actual_type = arg_val->getType();
        
        // 如果类型不匹配，需要转换
        if (actual_type != expected_type) {
            // 对于struct类型：使用指针bitcast（更可靠的方法）
            if (actual_type->isStructTy() && expected_type->isStructTy()) {
                // 检查是否有相同的元素数量和类型
                auto* actual_struct = llvm::cast<llvm::StructType>(actual_type);
                auto* expected_struct = llvm::cast<llvm::StructType>(expected_type);
                
                // 如果元素数量和类型匹配，使用指针转换
                if (actual_struct->getNumElements() == expected_struct->getNumElements()) {
                    // 创建临时alloca存储源值
                    llvm::AllocaInst* temp_alloca = builder.CreateAlloca(actual_type, nullptr, "temp.struct");
                    builder.CreateStore(arg_val, temp_alloca);
                    
                    // Bitcast alloca指针到目标类型的指针
                    llvm::Type* expected_ptr_type = expected_type->getPointerTo();
                    llvm::Value* cast_ptr = builder.CreateBitCast(temp_alloca, expected_ptr_type, "cast.ptr");
                    
                    // Load为期望的类型
                    arg_val = builder.CreateLoad(expected_type, cast_ptr, "converted.struct");
                }
            }
        }
        final_arg_values.push_back(arg_val);
    }
    
    // 创建调用
    // 🔧 Bug Fix: void函数不应该有命名返回值
    if (func->getReturnType()->isVoidTy()) {
        builder.CreateCall(func, final_arg_values);
        result_ = nullptr;  // void函数没有返回值
    } else {
        result_ = builder.CreateCall(func, final_arg_values, "calltmp");
    }
}

// 泛型函数调用CodeGen
llvm::Value* ExprCodeGen::generateGenericFunctionCall(
    const std::string& func_name,
    const std::vector<Type*>& type_args,
    const std::vector<std::unique_ptr<Expr>>& args) {
    
    auto& builder = context_->getBuilder();
    
    // 1. 查找泛型模板
    auto* tmpl = context_->getTypeSystem()->lookupGenericTemplate(func_name);
    if (!tmpl || tmpl->kind != GenericTemplate::FUNCTION) {
        return nullptr;
    }
    
    // 2. 生成实例化函数名
    std::string instance_name = func_name;
    for (const auto* type_arg : type_args) {
        instance_name += "_" + type_arg->toString();
    }
    
    // 3. 检查函数是否已生成
    llvm::Function* llvm_func = context_->getModule()->getFunction(instance_name);
    
    if (!llvm_func) {
        // 4. 生成实例化函数
        llvm_func = generateGenericFunctionInstance(func_name, type_args, tmpl);
        if (!llvm_func) {
            return nullptr;
        }
    }
    
    // 5. 评估参数
    std::vector<llvm::Value*> arg_values;
    for (const auto& arg : args) {
        arg->accept(this);
        if (result_) {
            arg_values.push_back(result_);
        }
    }
    
    // 6. 调用实例化函数
    return builder.CreateCall(llvm_func, arg_values);
}

// 生成泛型函数实例
llvm::Function* ExprCodeGen::generateGenericFunctionInstance(
    const std::string& func_name,
    const std::vector<Type*>& type_args,
    GenericTemplate* tmpl) {
    
    FunctionDecl* func_def = tmpl->func_def;
    
    // 1. 创建类型替换映射
    std::unordered_map<std::string, Type*> type_substitution;
    for (size_t i = 0; i < tmpl->type_params.size() && i < type_args.size(); i++) {
        type_substitution[tmpl->type_params[i]] = type_args[i];
    }
    
    // 2. 替换参数类型和返回类型
    TypeSystem* type_system = context_->getTypeSystem();
    
    std::vector<llvm::Type*> param_types_llvm;
    for (const auto& param : func_def->getParams()) {
        Type* param_type = param.type;
        Type* substituted_type = type_system->substituteType(
            param_type, type_substitution);
        param_types_llvm.push_back(context_->getLLVMType(substituted_type));
    }
    
    Type* return_type = func_def->getReturnType();
    Type* substituted_return_type = type_system->substituteType(
        return_type, type_substitution);
    llvm::Type* return_type_llvm = context_->getLLVMType(substituted_return_type);
    
    // 3. 创建函数类型和函数
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        return_type_llvm, param_types_llvm, false);
    
    std::string instance_name = func_name;
    for (const auto* type_arg : type_args) {
        instance_name += "_" + type_arg->toString();
    }
    
    llvm::Function* llvm_func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        instance_name,
        context_->getModule()
    );
    
    // 4. 生成函数体
    // 保存当前基本块和插入点
    auto& builder = context_->getBuilder();
    llvm::BasicBlock* saved_bb = builder.GetInsertBlock();
    
    // 创建entry块
    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(
        context_->getLLVMContext(), "entry", llvm_func);
    builder.SetInsertPoint(entry_bb);
    
    // 5. 绑定参数
    // 进入新的作用域
    context_->enterScope();
    
    size_t idx = 0;
    for (auto& arg : llvm_func->args()) {
        if (idx < func_def->getParams().size()) {
            const std::string& param_name = func_def->getParams()[idx].name;
            Type* param_type = func_def->getParams()[idx].type;
            Type* substituted_param_type = type_system->substituteType(
                param_type, type_substitution);
            
            // 创建alloca并存储参数
            llvm::Type* param_llvm_type = context_->getLLVMType(substituted_param_type);
            llvm::Value* alloca = builder.CreateAlloca(param_llvm_type, nullptr, param_name);
            builder.CreateStore(&arg, alloca);
            
            // 在作用域中定义变量
            context_->defineVariable(param_name, alloca);
            idx++;
        }
    }
    
    // 6. 生成函数体语句
    if (func_def->getBody()) {
        // 使用StmtCodeGen来生成函数体
        // 需要创建一个临时的类型映射环境来处理泛型参数
        
        // 对于简单情况：如果body只有一个return语句
        // 我们可以内联生成
        auto* block_stmt = dynamic_cast<BlockStmt*>(func_def->getBody());
        if (!block_stmt) {
            context_->exitScope();
            if (saved_bb) builder.SetInsertPoint(saved_bb);
            return llvm_func;
        }
        
        const auto& statements = block_stmt->getStmts();
        
        // 简化：只处理单个return语句
        if (statements.size() == 1) {
            if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(statements[0].get())) {
                if (ret_stmt->getValue()) {
                    // 生成return表达式
                    ret_stmt->getValue()->accept(this);
                    if (result_) {
                        builder.CreateRet(result_);
                    }
                }
            }
        }
    }
    
    // 7. 恢复作用域和插入点
    context_->exitScope();
    
    if (saved_bb) {
        builder.SetInsertPoint(saved_bb);
    }
    
    return llvm_func;
}

} // namespace pawc
