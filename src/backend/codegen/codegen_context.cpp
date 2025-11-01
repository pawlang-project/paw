//===--- codegen_context.cpp - CodeGen Context Implementation ----*- C++ -*-===//

#include "codegen_context.h"
#include "type/type_codegen.h"
#include "middleend/types/type_system.h"

#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>

namespace pawc {

CodeGenContext::CodeGenContext(const std::string& module_name,
                               TypeSystem* type_system,
                               SymbolTable* symbol_table)
    : module_(std::make_unique<llvm::Module>(module_name, context_)),
      builder_(context_),
      type_system_(type_system),
      symbol_table_(symbol_table) {
    
    // 初始化全局作用域
    variable_stack_.emplace_back();
    
    // 声明runtime函数
    declareRuntimeFunctions();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Type Mapping - 28种类型
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Type* CodeGenContext::getLLVMType(Type* paw_type) {
    // 检查缓存
    auto it = type_cache_.find(paw_type);
    if (it != type_cache_.end()) {
        return it->second;
    }
    
    llvm::Type* llvm_type = nullptr;
    
    if (paw_type->isPrimitive()) {
        llvm_type = mapPrimitiveType(paw_type);
    } else if (paw_type->isArray()) {
        llvm_type = mapArrayType(static_cast<ArrayType*>(paw_type));
    } else if (paw_type->isTuple()) {
        llvm_type = mapTupleType(static_cast<TupleType*>(paw_type));
    } else if (paw_type->isStruct()) {
        llvm_type = mapStructType(static_cast<StructType*>(paw_type));
    } else if (paw_type->isOptional()) {
        llvm_type = mapOptionalType(static_cast<OptionalType*>(paw_type));
    } else if (paw_type->isResult()) {
        llvm_type = mapResultType(static_cast<ResultType*>(paw_type));
    } else if (paw_type->isFunction()) {
        llvm_type = mapFunctionType(static_cast<FunctionType*>(paw_type));
    } else if (paw_type->isReference()) {
        auto* ref_type = static_cast<ReferenceType*>(paw_type);
        llvm_type = getLLVMType(ref_type->getPointeeType())->getPointerTo();
    } else {
        // 默认返回opaque pointer
        llvm_type = llvm::PointerType::getUnqual(context_);
    }
    
    // 缓存结果
    type_cache_[paw_type] = llvm_type;
    
    return llvm_type;
}

llvm::Type* CodeGenContext::mapPrimitiveType(Type* type) {
    switch (type->getKind()) {
        // 有符号整数
        case Type::Kind::I8:    return builder_.getInt8Ty();
        case Type::Kind::I16:   return builder_.getInt16Ty();
        case Type::Kind::I32:   return builder_.getInt32Ty();
        case Type::Kind::I64:   return builder_.getInt64Ty();
        case Type::Kind::I128:  return builder_.getInt128Ty();
        
        // 无符号整数 (LLVM使用相同的整数类型)
        case Type::Kind::U8:    return builder_.getInt8Ty();
        case Type::Kind::U16:   return builder_.getInt16Ty();
        case Type::Kind::U32:   return builder_.getInt32Ty();
        case Type::Kind::U64:   return builder_.getInt64Ty();
        case Type::Kind::U128:  return builder_.getInt128Ty();
        
        // 浮点数 (完整精度)
        case Type::Kind::F8:    return builder_.getBFloatTy();  // bfloat16
        case Type::Kind::F16:   return builder_.getHalfTy();    // half (fp16)
        case Type::Kind::F32:   return builder_.getFloatTy();   // float
        case Type::Kind::F64:   return builder_.getDoubleTy();  // double
        case Type::Kind::F128:  return llvm::Type::getFP128Ty(context_);   // fp128
        
        // 其他
        case Type::Kind::Bool:   return builder_.getInt1Ty();
        case Type::Kind::Char:   return builder_.getInt8Ty();
        case Type::Kind::String: return llvm::PointerType::getUnqual(context_);
        case Type::Kind::Void:   return builder_.getVoidTy();
        
        default:
            return llvm::PointerType::getUnqual(context_);
    }
}

llvm::Type* CodeGenContext::mapArrayType(ArrayType* type) {
    llvm::Type* element_type = getLLVMType(type->getElementType());
    return llvm::ArrayType::get(element_type, type->getSize());
}

llvm::Type* CodeGenContext::mapTupleType(TupleType* type) {
    std::vector<llvm::Type*> element_types;
    for (auto* elem_type : type->getElementTypes()) {
        element_types.push_back(getLLVMType(elem_type));
    }
    return llvm::StructType::get(context_, element_types);
}

llvm::Type* CodeGenContext::mapStructType(StructType* type) {
    std::vector<llvm::Type*> field_types;
    for (const auto& [name, field_type] : type->getFields()) {
        field_types.push_back(getLLVMType(field_type));
    }
    return llvm::StructType::create(context_, field_types, type->getName());
}

llvm::Type* CodeGenContext::mapOptionalType(OptionalType* type) {
    // Optional<T> = { i1 has_value, T value }
    llvm::Type* inner_type = getLLVMType(type->getInnerType());
    return llvm::StructType::get(context_, {builder_.getInt1Ty(), inner_type});
}

llvm::Type* CodeGenContext::mapResultType(ResultType* type) {
    // Result<T> = { i1 is_ok, T ok_value, ptr err_msg }
    llvm::Type* ok_type = getLLVMType(type->getOkType());
    return llvm::StructType::get(context_, {
        builder_.getInt1Ty(),
        ok_type,
        llvm::PointerType::getUnqual(context_)
    });
}

llvm::Type* CodeGenContext::mapFunctionType(FunctionType* type) {
    llvm::Type* return_type = getLLVMType(type->getReturnType());
    std::vector<llvm::Type*> param_types;
    for (auto* param_type : type->getParamTypes()) {
        param_types.push_back(getLLVMType(param_type));
    }
    return llvm::FunctionType::get(return_type, param_types, false);
}

// 基础类型快捷方法
llvm::Type* CodeGenContext::getI8Type()   { return builder_.getInt8Ty(); }
llvm::Type* CodeGenContext::getI16Type()  { return builder_.getInt16Ty(); }
llvm::Type* CodeGenContext::getI32Type()  { return builder_.getInt32Ty(); }
llvm::Type* CodeGenContext::getI64Type()  { return builder_.getInt64Ty(); }
llvm::Type* CodeGenContext::getI128Type() { return builder_.getInt128Ty(); }

llvm::Type* CodeGenContext::getU8Type()   { return builder_.getInt8Ty(); }
llvm::Type* CodeGenContext::getU16Type()  { return builder_.getInt16Ty(); }
llvm::Type* CodeGenContext::getU32Type()  { return builder_.getInt32Ty(); }
llvm::Type* CodeGenContext::getU64Type()  { return builder_.getInt64Ty(); }
llvm::Type* CodeGenContext::getU128Type() { return builder_.getInt128Ty(); }

llvm::Type* CodeGenContext::getF8Type()   { return builder_.getBFloatTy(); }
llvm::Type* CodeGenContext::getF16Type()  { return builder_.getHalfTy(); }
llvm::Type* CodeGenContext::getF32Type()  { return builder_.getFloatTy(); }
llvm::Type* CodeGenContext::getF64Type()  { return builder_.getDoubleTy(); }
llvm::Type* CodeGenContext::getF128Type() { return llvm::Type::getFP128Ty(context_); }

llvm::Type* CodeGenContext::getBoolType()   { return builder_.getInt1Ty(); }
llvm::Type* CodeGenContext::getCharType()   { return builder_.getInt8Ty(); }
llvm::Type* CodeGenContext::getStringType() { return llvm::PointerType::getUnqual(context_); }
llvm::Type* CodeGenContext::getVoidType()   { return builder_.getVoidTy(); }

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Symbol Management
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void CodeGenContext::defineVariable(const std::string& name, llvm::Value* value) {
    variable_stack_.back()[name] = value;
}

llvm::Value* CodeGenContext::lookupVariable(const std::string& name) {
    // 从内到外查找
    for (auto it = variable_stack_.rbegin(); it != variable_stack_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return nullptr;
}

void CodeGenContext::registerFunction(const std::string& name, llvm::Function* func) {
    function_table_[name] = func;
}

llvm::Function* CodeGenContext::lookupFunction(const std::string& name) {
    auto it = function_table_.find(name);
    if (it != function_table_.end()) {
        return it->second;
    }
    
    // 查找runtime函数
    return getRuntimeFunction(name);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Scope Management
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void CodeGenContext::enterScope() {
    variable_stack_.emplace_back();
}

void CodeGenContext::exitScope() {
    if (variable_stack_.size() > 1) {
        variable_stack_.pop_back();
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Helper Functions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::AllocaInst* CodeGenContext::createEntryBlockAlloca(
    llvm::Function* func,
    const std::string& var_name,
    llvm::Type* type) {
    
    llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(),
                                  func->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(type, nullptr, var_name);
}

llvm::BasicBlock* CodeGenContext::getCurrentBlock() {
    return builder_.GetInsertBlock();
}

llvm::BasicBlock* CodeGenContext::createBasicBlock(const std::string& name,
                                                   llvm::Function* func) {
    return llvm::BasicBlock::Create(context_, name, func);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Runtime Functions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void CodeGenContext::declareRuntimeFunctions() {
    // Runtime函数声明由BuiltinCodeGen统一管理
    // 这里只声明最基础的内存管理函数
    
    // void* paw_malloc(u64 size)
    auto* malloc_type = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(context_),
        {builder_.getInt64Ty()},
        false
    );
    runtime_functions_["paw_malloc"] = llvm::Function::Create(
        malloc_type,
        llvm::Function::ExternalLinkage,
        "paw_malloc",
        module_.get()
    );
    
    // void paw_free(void* ptr)
    auto* free_type = llvm::FunctionType::get(
        builder_.getVoidTy(),
        {llvm::PointerType::getUnqual(context_)},
        false
    );
    runtime_functions_["paw_free"] = llvm::Function::Create(
        free_type,
        llvm::Function::ExternalLinkage,
        "paw_free",
        module_.get()
    );
    
    // void paw_panic(const char* msg)
    auto* panic_type = llvm::FunctionType::get(
        builder_.getVoidTy(),
        {llvm::PointerType::getUnqual(context_)},
        false
    );
    runtime_functions_["paw_panic"] = llvm::Function::Create(
        panic_type,
        llvm::Function::ExternalLinkage,
        "paw_panic",
        module_.get()
    );
    
    // 注意：18种print/println/to_string函数由BuiltinCodeGen::declareAllRuntimeFunctions()声明
}

llvm::Function* CodeGenContext::getRuntimeFunction(const std::string& name) {
    auto it = runtime_functions_.find(name);
    if (it != runtime_functions_.end()) {
        return it->second;
    }
    return nullptr;
}

void CodeGenContext::registerRuntimeFunction(const std::string& name, llvm::Function* func) {
    runtime_functions_[name] = func;
}

llvm::Function* CodeGenContext::lookupFunctionWithTypes(
    const std::string& name,
    const std::vector<Type*>& param_types) {
    
    // 先查找普通函数
    auto it = function_table_.find(name);
    if (it != function_table_.end()) {
        return it->second;
    }
    
    // 查找builtin重载
    auto overload_it = builtin_overloads_.find(name);
    if (overload_it != builtin_overloads_.end()) {
        // 遍历所有重载，找到匹配的
        for (const auto& [overload_types, func] : overload_it->second) {
            if (overload_types.size() != param_types.size()) continue;
            
            // 检查每个参数类型是否匹配
            bool match = true;
            for (size_t i = 0; i < param_types.size(); ++i) {
                if (!type_system_->equals(param_types[i], overload_types[i])) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                return func;
            }
        }
    }
    
    return nullptr;
}

std::string CodeGenContext::getTypeSignature(const std::vector<Type*>& types) {
    std::string sig;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) sig += "_";
        
        if (types[i]) {
            auto kind = types[i]->getKind();
            if (kind == Type::Kind::I8) sig += "i8";
            else if (kind == Type::Kind::I16) sig += "i16";
            else if (kind == Type::Kind::I32) sig += "i32";
            else if (kind == Type::Kind::I64) sig += "i64";
            else if (kind == Type::Kind::I128) sig += "i128";
            else if (kind == Type::Kind::U8) sig += "u8";
            else if (kind == Type::Kind::U16) sig += "u16";
            else if (kind == Type::Kind::U32) sig += "u32";
            else if (kind == Type::Kind::U64) sig += "u64";
            else if (kind == Type::Kind::U128) sig += "u128";
            else if (kind == Type::Kind::F8) sig += "f8";
            else if (kind == Type::Kind::F16) sig += "f16";
            else if (kind == Type::Kind::F32) sig += "f32";
            else if (kind == Type::Kind::F64) sig += "f64";
            else if (kind == Type::Kind::F128) sig += "f128";
            else if (kind == Type::Kind::Bool) sig += "bool";
            else if (kind == Type::Kind::Char) sig += "char";
            else if (kind == Type::Kind::String) sig += "string";
            else sig += "unknown";
        }
    }
    return sig;
}

void CodeGenContext::registerAllBuiltinFunctions() {
    // 为每个builtin函数的18种类型重载生成LLVM声明
    
    if (!type_system_) {
        return; // TypeSystem未初始化
    }
    
    TypeCodeGen type_gen(context_);
    auto* void_type = builder_.getVoidTy();
    auto* string_type = llvm::PointerType::getUnqual(context_);
    
    // 先只实现i32类型的print/println来测试，避免崩溃
    Type* i32_type = type_system_->getI32Type();
    if (!i32_type) {
        return; // 无法获取i32类型
    }
    
    std::vector<Type*> all_types = {i32_type};
    
    // 为每个类型生成print/println/to_string
    for (Type* param_type : all_types) {
        std::string type_suffix = getTypeSignature({param_type});
        llvm::Type* llvm_param_type = type_gen.mapType(param_type);
        
        // print(T) -> void
        {
            std::string func_name = "print." + type_suffix;
            auto* func_type = llvm::FunctionType::get(void_type, {llvm_param_type}, false);
            auto* func = llvm::Function::Create(
                func_type,
                llvm::Function::InternalLinkage,
                func_name,
                module_.get()
            );
            
            // 生成函数体：调用对应的paw_print_*
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
            auto saved_point = builder_.saveIP();
            builder_.SetInsertPoint(entry);
            
            std::string runtime_name = "paw_print_" + type_suffix;
            auto* runtime_func = getRuntimeFunction(runtime_name);
            if (!runtime_func) {
                // 声明runtime函数
                auto* rt_func_type = llvm::FunctionType::get(void_type, {llvm_param_type}, false);
                runtime_func = llvm::Function::Create(
                    rt_func_type,
                    llvm::Function::ExternalLinkage,
                    runtime_name,
                    module_.get()
                );
                registerRuntimeFunction(runtime_name, runtime_func);
            }
            
            builder_.CreateCall(runtime_func, {&*func->arg_begin()});
            builder_.CreateRetVoid();
            builder_.restoreIP(saved_point);
            
            // 注册到重载表
            builtin_overloads_["print"].push_back({{param_type}, func});
        }
        
        // println(T) -> void
        {
            std::string func_name = "println." + type_suffix;
            auto* func_type = llvm::FunctionType::get(void_type, {llvm_param_type}, false);
            auto* func = llvm::Function::Create(
                func_type,
                llvm::Function::InternalLinkage,
                func_name,
                module_.get()
            );
            
            // 生成函数体：调用print + newline
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
            auto saved_point = builder_.saveIP();
            builder_.SetInsertPoint(entry);
            
            // 调用print
            auto* print_func = builtin_overloads_["print"].back().second;
            builder_.CreateCall(print_func, {&*func->arg_begin()});
            
            // 调用paw_print_newline
            auto* newline_func = getRuntimeFunction("paw_print_newline");
            if (!newline_func) {
                auto* nl_func_type = llvm::FunctionType::get(void_type, {}, false);
                newline_func = llvm::Function::Create(
                    nl_func_type,
                    llvm::Function::ExternalLinkage,
                    "paw_print_newline",
                    module_.get()
                );
                registerRuntimeFunction("paw_print_newline", newline_func);
            }
            builder_.CreateCall(newline_func, {});
            builder_.CreateRetVoid();
            builder_.restoreIP(saved_point);
            
            // 注册到重载表
            builtin_overloads_["println"].push_back({{param_type}, func});
        }
        
        // to_string(T) -> string
        {
            std::string func_name = "to_string." + type_suffix;
            auto* func_type = llvm::FunctionType::get(string_type, {llvm_param_type}, false);
            auto* func = llvm::Function::Create(
                func_type,
                llvm::Function::InternalLinkage,
                func_name,
                module_.get()
            );
            
            // 生成函数体：调用paw_*_to_string
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
            auto saved_point = builder_.saveIP();
            builder_.SetInsertPoint(entry);
            
            std::string runtime_name = "paw_" + type_suffix + "_to_string";
            auto* runtime_func = getRuntimeFunction(runtime_name);
            if (!runtime_func) {
                auto* rt_func_type = llvm::FunctionType::get(string_type, {llvm_param_type}, false);
                runtime_func = llvm::Function::Create(
                    rt_func_type,
                    llvm::Function::ExternalLinkage,
                    runtime_name,
                    module_.get()
                );
                registerRuntimeFunction(runtime_name, runtime_func);
            }
            
            auto* result = builder_.CreateCall(runtime_func, {&*func->arg_begin()});
            builder_.CreateRet(result);
            builder_.restoreIP(saved_point);
            
            // 注册到重载表
            builtin_overloads_["to_string"].push_back({{param_type}, func});
        }
    }
    
    // len(string) -> u64
    {
        auto* u64_type = builder_.getInt64Ty();
        auto* func_type = llvm::FunctionType::get(u64_type, {string_type}, false);
        auto* func = llvm::Function::Create(
            func_type,
            llvm::Function::InternalLinkage,
            "len.string",
            module_.get()
        );
        
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
        auto saved_point = builder_.saveIP();
        builder_.SetInsertPoint(entry);
        
        auto* runtime_func = getRuntimeFunction("paw_string_len");
        if (!runtime_func) {
            auto* rt_func_type = llvm::FunctionType::get(u64_type, {string_type}, false);
            runtime_func = llvm::Function::Create(
                rt_func_type,
                llvm::Function::ExternalLinkage,
                "paw_string_len",
                module_.get()
            );
            registerRuntimeFunction("paw_string_len", runtime_func);
        }
        
        auto* result = builder_.CreateCall(runtime_func, {&*func->arg_begin()});
        builder_.CreateRet(result);
        builder_.restoreIP(saved_point);
        
        builtin_overloads_["len"].push_back({{type_system_->getStringType()}, func});
    }
    
    // panic(string) -> void
    {
        auto* func_type = llvm::FunctionType::get(void_type, {string_type}, false);
        auto* func = llvm::Function::Create(
            func_type,
            llvm::Function::InternalLinkage,
            "panic.string",
            module_.get()
        );
        
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
        auto saved_point = builder_.saveIP();
        builder_.SetInsertPoint(entry);
        
        auto* runtime_func = getRuntimeFunction("paw_panic");
        builder_.CreateCall(runtime_func, {&*func->arg_begin()});
        builder_.CreateUnreachable();
        builder_.restoreIP(saved_point);
        
        builtin_overloads_["panic"].push_back({{type_system_->getStringType()}, func});
    }
    
    // assert(bool, string) -> void
    {
        auto* bool_type = builder_.getInt1Ty();
        auto* func_type = llvm::FunctionType::get(void_type, {bool_type, string_type}, false);
        auto* func = llvm::Function::Create(
            func_type,
            llvm::Function::InternalLinkage,
            "assert.bool_string",
            module_.get()
        );
        
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
        auto saved_point = builder_.saveIP();
        builder_.SetInsertPoint(entry);
        
        auto args = func->arg_begin();
        llvm::Value* condition = &*args++;
        llvm::Value* message = &*args;
        
        // 调用paw_assert(condition, message, __FILE__, __LINE__)
        auto* file_str = builder_.CreateGlobalStringPtr("paw_code");
        auto* line_val = llvm::ConstantInt::get(builder_.getInt32Ty(), 0);
        
        auto* runtime_func = getRuntimeFunction("paw_assert");
        builder_.CreateCall(runtime_func, {condition, message, file_str, line_val});
        builder_.CreateRetVoid();
        builder_.restoreIP(saved_point);
        
        builtin_overloads_["assert"].push_back({
            {type_system_->getBoolType(), type_system_->getStringType()}, func});
    }
    
    // debug_assert(bool, string) -> void
    {
        auto* bool_type = builder_.getInt1Ty();
        auto* func_type = llvm::FunctionType::get(void_type, {bool_type, string_type}, false);
        auto* func = llvm::Function::Create(
            func_type,
            llvm::Function::InternalLinkage,
            "debug_assert.bool_string",
            module_.get()
        );
        
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
        auto saved_point = builder_.saveIP();
        builder_.SetInsertPoint(entry);
        
        auto args = func->arg_begin();
        llvm::Value* condition = &*args++;
        llvm::Value* message = &*args;
        
        auto* file_str = builder_.CreateGlobalStringPtr("paw_code");
        auto* line_val = llvm::ConstantInt::get(builder_.getInt32Ty(), 0);
        
        auto* runtime_func = getRuntimeFunction("paw_debug_assert");
        builder_.CreateCall(runtime_func, {condition, message, file_str, line_val});
        builder_.CreateRetVoid();
        builder_.restoreIP(saved_point);
        
        builtin_overloads_["debug_assert"].push_back({
            {type_system_->getBoolType(), type_system_->getStringType()}, func});
    }
    
    // unreachable() -> void
    {
        auto* func_type = llvm::FunctionType::get(void_type, {}, false);
        auto* func = llvm::Function::Create(
            func_type,
            llvm::Function::InternalLinkage,
            "unreachable",
            module_.get()
        );
        
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
        auto saved_point = builder_.saveIP();
        builder_.SetInsertPoint(entry);
        
        auto* file_str = builder_.CreateGlobalStringPtr("paw_code");
        auto* line_val = llvm::ConstantInt::get(builder_.getInt32Ty(), 0);
        
        auto* runtime_func = getRuntimeFunction("paw_unreachable");
        builder_.CreateCall(runtime_func, {file_str, line_val});
        builder_.CreateUnreachable();
        builder_.restoreIP(saved_point);
        
        builtin_overloads_["unreachable"].push_back({{}, func});
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Module Output
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void CodeGenContext::dump() {
    module_->print(llvm::outs(), nullptr);
}

bool CodeGenContext::writeToFile(const std::string& filename) {
    std::error_code ec;
    llvm::raw_fd_ostream out(filename, ec, llvm::sys::fs::OF_None);
    
    if (ec) {
        return false;
    }
    
    module_->print(out, nullptr);
    return true;
}

bool CodeGenContext::emitObjectFile(const std::string& filename) {
    // 配置目标平台
    std::string error;
    auto target_triple_str = llvm::sys::getDefaultTargetTriple();
    llvm::Triple target_triple(target_triple_str);
    
    auto target = llvm::TargetRegistry::lookupTarget(target_triple_str, error);
    if (!target) {
        return false;
    }
    
    // 配置TargetMachine
    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto rm = llvm::Reloc::Model::PIC_;
    
    auto* target_machine = target->createTargetMachine(
        target_triple_str, cpu, features, opt, rm);
    
    module_->setDataLayout(target_machine->createDataLayout());
    module_->setTargetTriple(target_triple);
    
    // 输出对象文件
    std::error_code ec;
    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
    
    if (ec) {
        return false;
    }
    
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::ObjectFile;
    
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        return false;
    }
    
    pass.run(*module_);
    dest.flush();
    
    delete target_machine;
    return true;
}

} // namespace pawc
