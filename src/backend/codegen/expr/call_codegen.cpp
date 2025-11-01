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
#include <iostream>

namespace pawc {

void ExprCodeGen::visit(CallExpr* node) {
    auto& builder = context_->getBuilder();
    
    // === Step 1: 检查是否是闭包调用 ===
    Type* callee_type = node->getCallee()->getType();
    
    if (callee_type && callee_type->getKind() == Type::Kind::Function) {
        // === 闭包调用！===
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
        // closure_value可能是：
        // 1. llvm::Function* - 无捕获的闭包（直接函数指针）
        // 2. 指向闭包结构体的指针变量 - 需要load+提取函数指针+传递上下文
        
        llvm::Value* callable = nullptr;
        llvm::FunctionType* fn_type = nullptr;
        std::vector<llvm::Value*> final_args;
        
        // Case 1: 如果是llvm::Function*，直接使用
        if (llvm::isa<llvm::Function>(closure_value)) {
            callable = closure_value;
            fn_type = llvm::cast<llvm::Function>(closure_value)->getFunctionType();
            final_args = args;  // 不需要额外的上下文参数
        }
        // Case 2: 如果是指针（闭包结构体）
        else if (closure_value->getType()->isPointerTy()) {
            // 获取AST类型信息
            auto* func_type_ast = static_cast<FunctionType*>(callee_type);
            TypeCodeGen type_gen(builder.getContext());
            
            // 构建用户层FunctionType（不包含上下文）
            std::vector<llvm::Type*> param_types_llvm;
            for (Type* param_type : func_type_ast->getParamTypes()) {
                param_types_llvm.push_back(type_gen.mapType(param_type));
            }
            llvm::Type* return_type_llvm = type_gen.mapType(func_type_ast->getReturnType());
            
            // 关键：closure_value是指向闭包结构体指针的指针（alloca返回的）
            // 需要先load获取实际的闭包结构体指针
            llvm::Value* closure_struct_ptr = builder.CreateLoad(
                llvm::PointerType::getUnqual(builder.getContext()),
                closure_value,
                "closure_struct"
            );
            
            // 从闭包结构体提取函数指针（字段0）
            llvm::Value* fn_ptr_field_addr = builder.CreateConstInBoundsGEP1_32(
                llvm::PointerType::getUnqual(builder.getContext()),
                closure_struct_ptr,
                0,
                "fn_ptr_field"
            );
            
            // Load函数指针
            llvm::Value* fn_ptr = builder.CreateLoad(
                llvm::PointerType::getUnqual(builder.getContext()),
                fn_ptr_field_addr,
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
            // 未知类型
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
        
        TypeCodeGen type_gen(context_->getLLVMContext());
        llvm::Type* result_llvm_type = type_gen.mapType(result_type_ast);
        
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
        
        TypeCodeGen type_gen(context_->getLLVMContext());
        llvm::Type* result_llvm_type = type_gen.mapType(result_type_ast);
        
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
    
    // 创建调用
    result_ = builder.CreateCall(func, arg_values, "calltmp");
}

} // namespace pawc

