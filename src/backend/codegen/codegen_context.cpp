//===--- codegen_context.cpp - CodeGen Context Implementation ----*- C++ -*-===//
/// @file codegen_context.cpp
/// @brief LLVM code generation context implementation
///
/// Manages LLVM infrastructure and type conversion for code generation.

#include "codegen_context.h"
#include "type/type_codegen.h"
#include "middleend/types/type_system.h"

#include <iostream>
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

/// Initialize code generation context with LLVM module and builders
CodeGenContext::CodeGenContext(const std::string& module_name,
                               TypeSystem* type_system,
                               SymbolTable* symbol_table)
    : module_(std::make_unique<llvm::Module>(module_name, context_)),
      builder_(context_),
      type_system_(type_system),
      symbol_table_(symbol_table) {
    
    // Initialize global scope
    variable_stack_.emplace_back();
    
    // Declare runtime function
    declareRuntimeFunctions();
}

CodeGenContext::~CodeGenContext() {
    // Bug Fix: Release Module ownership to avoid memory issues during destruction
    // LLVM's internal management of anonymous StructType causes double-free on destruction
    // Release ownership and let the pointer leak, OS will reclaim on process exit
    if (module_) {
        module_.release();  // Release ownership, don't call destructor
    }
    // Other members destructed automatically in default order
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Type Mapping - 28 type kinds
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Type* CodeGenContext::getLLVMType(Type* paw_type) {
    // Check cache first
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
        llvm_type = llvm::PointerType::getUnqual(context_);
    } else if (paw_type->isEnum()) {
        // 🔧 M7: Enum type mapping
        llvm_type = mapEnumType(static_cast<EnumType*>(paw_type));
    } else {
        // Default return opaque pointer
        llvm_type = llvm::PointerType::getUnqual(context_);
    }
    
    // Cache results
    type_cache_[paw_type] = llvm_type;
    
    return llvm_type;
}

llvm::Type* CodeGenContext::mapPrimitiveType(Type* type) {
    switch (type->getKind()) {
        // Signed integers
        case Type::Kind::I8:    return builder_.getInt8Ty();
        case Type::Kind::I16:   return builder_.getInt16Ty();
        case Type::Kind::I32:   return builder_.getInt32Ty();
        case Type::Kind::I64:   return builder_.getInt64Ty();
        case Type::Kind::I128:  return builder_.getInt128Ty();
        
        // Unsigned integers (LLVM uses same integer types)
        case Type::Kind::U8:    return builder_.getInt8Ty();
        case Type::Kind::U16:   return builder_.getInt16Ty();
        case Type::Kind::U32:   return builder_.getInt32Ty();
        case Type::Kind::U64:   return builder_.getInt64Ty();
        case Type::Kind::U128:  return builder_.getInt128Ty();
        
        // Floating-point numbers (full precision)
        case Type::Kind::F8:    return builder_.getBFloatTy();  // bfloat16
        case Type::Kind::F16:   return builder_.getHalfTy();    // half (fp16)
        case Type::Kind::F32:   return builder_.getFloatTy();   // float
        case Type::Kind::F64:   return builder_.getDoubleTy();  // double
        case Type::Kind::F128:  return llvm::Type::getFP128Ty(context_);   // fp128
        
        // Other types
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
    // Array is LLVM built-in type, no naming needed
    return llvm::ArrayType::get(element_type, type->getSize());
}

llvm::Type* CodeGenContext::mapTupleType(TupleType* type) {
    // Tuple uses anonymous type (LLVM internally deduplicates same structure types)
    std::vector<llvm::Type*> element_types;
    for (auto* elem_type : type->getElementTypes()) {
        element_types.push_back(getLLVMType(elem_type));
    }
    return llvm::StructType::get(context_, element_types);
}

llvm::Type* CodeGenContext::mapStructType(StructType* type) {
    // 🔧 Key fix：ensurestruct typein/atinteger/wholeindividual/piececompile/compilationproceduremiddle/centeronlycreate once
    std::string struct_name = type->getName();
    
    // 1. firstFirstthrough/vianamelookup existing LLVM types
    llvm::StructType* existing_type = llvm::StructType::getTypeByName(context_, struct_name);
    if (existing_type) {
        // type already exists，directlyreturn (even if PawLang Type objects differ)
        type_cache_[type] = existing_type;
        return existing_type;
    }
    
    // 2. notexists，needcreate new type
    // Firstcreate opaque type（onlyhasname，nohasbody）
    llvm::StructType* new_type = llvm::StructType::create(context_, struct_name);
    
    // immediatelycache，preventduring recursive definitionduplicatecreate
    type_cache_[type] = new_type;
    
    // 3. generate field types
    std::vector<llvm::Type*> field_types;
    for (const auto& [name, field_type] : type->getFields()) {
        field_types.push_back(getLLVMType(field_type));
    }
    
    // 4. set body (populate fields)
    new_type->setBody(field_types);
    
    return new_type;
}

llvm::Type* CodeGenContext::mapOptionalType(OptionalType* type) {
    // Optional<T> = { i1 has_value, T value }
    // Optional uses anonymous type (LLVM internally deduplicates same structure types)
    Type* inner_paw_type = type->getInnerType();
    llvm::Type* inner_type = getLLVMType(inner_paw_type);
    
    // specialhandle: Optional<void> - void cannot be used as struct field
    if (inner_paw_type->isVoid()) {
        // use i8 as placeholder
        inner_type = builder_.getInt8Ty();
    }
    
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

llvm::Type* CodeGenContext::mapEnumType(EnumType* type) {
    // 🔧 M7: Enum type mappingis/as: { i32 variant_index, data }
    // As simplification, we use i64 as data field (may store pointer or small data)
    
    // lookup largest variant data type
    llvm::Type* data_type = builder_.getInt64Ty();  // defaulti64
    
    for (const auto& variant : type->getVariants()) {
        if (variant.second) {
            llvm::Type* variant_llvm_type = getLLVMType(variant.second);
            // if variant type is larger，useit
            auto& data_layout = module_->getDataLayout();
            if (data_layout.getTypeAllocSize(variant_llvm_type) >
                data_layout.getTypeAllocSize(data_type)) {
                data_type = variant_llvm_type;
            }
        }
    }
    
    return llvm::StructType::get(context_, {
        builder_.getInt32Ty(),  // variant_index
        data_type               // data
    });
}

// base type shortcut methods
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
    // lookup from inner to outer
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
    
    // lookup runtime function
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
    // Runtime function declarations uniformly managed by BuiltinCodeGen
    // hereonlydeclarationmost/lastbasic memory management functions
    
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
    
    // Note：18 print/println/to_string functions declared by BuiltinCodeGen::declareAllRuntimeFunctions()
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
    
    // first lookup normal function
    auto it = function_table_.find(name);
    if (it != function_table_.end()) {
        return it->second;
    }
    
    // lookup builtin overload
    auto overload_it = builtin_overloads_.find(name);
    if (overload_it != builtin_overloads_.end()) {
        // traverse all overloads，foundmatchof/the
        for (const auto& [overload_types, func] : overload_it->second) {
            if (overload_types.size() != param_types.size()) continue;
            
            // check each parameter typeyesnomatch
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
            else if (kind == Type::Kind::Struct) {
                // Struct type: use struct name
                auto* struct_type = static_cast<StructType*>(types[i]);
                sig += struct_type->getName();
            }
            else if (kind == Type::Kind::Optional) {
                auto* optional_type = static_cast<OptionalType*>(types[i]);
                sig += "optional_";
                sig += getTypeSignature({optional_type->getInnerType()});
            }
            else if (kind == Type::Kind::Result) {
                auto* results_type = static_cast<ResultType*>(types[i]);
                sig += "results_";
                sig += getTypeSignature({results_type->getOkType()});
            }
            else sig += "unknown";
        }
    }
    return sig;
}

void CodeGenContext::registerAllBuiltinFunctions() {
    // generate LLVM declaration for each builtin function 18 type overloads
    
    if (!type_system_) {
        return; // TypeSystem not initialized
    }
    
    TypeCodeGen type_gen(context_);
    auto* void_type = builder_.getVoidTy();
    auto* string_type = llvm::PointerType::getUnqual(context_);
    
    // get all 18 basic types
    std::vector<Type*> all_types;
    if (auto* t = type_system_->getI8Type()) all_types.push_back(t);
    if (auto* t = type_system_->getI16Type()) all_types.push_back(t);
    if (auto* t = type_system_->getI32Type()) all_types.push_back(t);
    if (auto* t = type_system_->getI64Type()) all_types.push_back(t);
    if (auto* t = type_system_->getI128Type()) all_types.push_back(t);
    if (auto* t = type_system_->getU8Type()) all_types.push_back(t);
    if (auto* t = type_system_->getU16Type()) all_types.push_back(t);
    if (auto* t = type_system_->getU32Type()) all_types.push_back(t);
    if (auto* t = type_system_->getU64Type()) all_types.push_back(t);
    if (auto* t = type_system_->getU128Type()) all_types.push_back(t);
    if (auto* t = type_system_->getF8Type()) all_types.push_back(t);
    if (auto* t = type_system_->getF16Type()) all_types.push_back(t);
    if (auto* t = type_system_->getF32Type()) all_types.push_back(t);
    if (auto* t = type_system_->getF64Type()) all_types.push_back(t);
    if (auto* t = type_system_->getF128Type()) all_types.push_back(t);
    if (auto* t = type_system_->getBoolType()) all_types.push_back(t);
    if (auto* t = type_system_->getCharType()) all_types.push_back(t);
    if (auto* t = type_system_->getStringType()) all_types.push_back(t);
    
    // generate print/println/to_string for each type
    for (Type* param_type : all_types) {
        std::string type_suffix = getTypeSignature({param_type});
        llvm::Type* llvm_param_type = getLLVMType(param_type);  // use unified type mapping
        
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
            
            // generate function body：call correspondingpaw_print_*
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
            auto saved_point = builder_.saveIP();
            builder_.SetInsertPoint(entry);
            
            std::string runtime_name = "paw_print_" + type_suffix;
            auto* runtime_func = getRuntimeFunction(runtime_name);
            if (!runtime_func) {
                // Declare runtime function
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
            
            // register to overload table
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
            
            // generate function body：call print + newline
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", func);
            auto saved_point = builder_.saveIP();
            builder_.SetInsertPoint(entry);
            
            // call print
            auto* print_func = builtin_overloads_["print"].back().second;
            builder_.CreateCall(print_func, {&*func->arg_begin()});
            
            // call paw_print_newline
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
            
            // register to overload table
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
            
            // generate function body：call paw_*_to_string
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
            
            auto* results = builder_.CreateCall(runtime_func, {&*func->arg_begin()});
            builder_.CreateRet(results);
            builder_.restoreIP(saved_point);
            
            // register to overload table
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
        
        auto* results = builder_.CreateCall(runtime_func, {&*func->arg_begin()});
        builder_.CreateRet(results);
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
        
        // call paw_assert(condition, message, __FILE__, __LINE__)
        auto* file_str = builder_.CreateGlobalString("paw_code");
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
        
        auto* file_str = builder_.CreateGlobalString("paw_code");
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
        
        auto* file_str = builder_.CreateGlobalString("paw_code");
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
    // Configure target platform
    std::string error;
    llvm::Triple target_triple(llvm::sys::getDefaultTargetTriple());
    
    auto target = llvm::TargetRegistry::lookupTarget(target_triple.getTriple(), error);
    if (!target) {
        return false;
    }
    
    // Configure TargetMachine
    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto rm = llvm::Reloc::Model::PIC_;
    
    auto* target_machine = target->createTargetMachine(
        target_triple, cpu, features, opt, rm);
    
    module_->setDataLayout(target_machine->createDataLayout());
    module_->setTargetTriple(target_triple);
    
    // outputobjectfile
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
