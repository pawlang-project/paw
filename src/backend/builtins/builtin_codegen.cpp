//===--- builtin_codegen.cpp - Builtin CodeGen Implementation ----*- C++ -*-===//
/// @file builtin_codegen.cpp
/// @brief Implementation file

#include "builtin_codegen.h"
#include "backend/codegen/type/type_codegen.h"
#include <llvm/IR/Constants.h>

namespace pawc {

BuiltinCodeGen::BuiltinCodeGen(CodeGenContext* context) : context_(context) {}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// declareAllRuntimefunction
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void BuiltinCodeGen::declareAllRuntimeFunctions() {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    auto* i1_type = builder.getInt1Ty();
    auto* i32_type = builder.getInt32Ty();
    auto* void_type = builder.getVoidTy();
    auto* ptr_type = llvm::PointerType::getUnqual(ctx);
    
    // printfunction（18types/kindstypes）- alreadybyCodeGenContextdeclaration
    // panic - alreadybyCodeGenContextdeclaration
    
    // assert(bool, string, string, i32) -> void
    declareRuntimeFunction("paw_assert", void_type,
        {i1_type, ptr_type, ptr_type, i32_type});
    
    // debug_assert(bool, string, string, i32) -> void
    declareRuntimeFunction("paw_debug_assert", void_type,
        {i1_type, ptr_type, ptr_type, i32_type});
    
    // unreachable(string, i32) -> void
    declareRuntimeFunction("paw_unreachable", void_type,
        {ptr_type, i32_type});
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// generationcall
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Value* BuiltinCodeGen::generatePanic(llvm::Value* message) {
    auto* panic_fn = context_->getRuntimeFunction("paw_panic");
    if (!panic_fn) return nullptr;
    
    return context_->getBuilder().CreateCall(panic_fn, {message});
}

llvm::Value* BuiltinCodeGen::generateAssert(llvm::Value* condition, llvm::Value* message,
                                             const std::string& file, int line) {
    auto& builder = context_->getBuilder();
    
    // Create filename character string constant
    auto* file_str = builder.CreateGlobalString(file);
    auto* line_val = llvm::ConstantInt::get(builder.getInt32Ty(), line);
    
    // call paw_assert
    auto* assert_fn = context_->getRuntimeFunction("paw_assert");
    if (!assert_fn) return nullptr;
    
    return builder.CreateCall(assert_fn, {condition, message, file_str, line_val});
}

llvm::Value* BuiltinCodeGen::generateDebugAssert(llvm::Value* condition, llvm::Value* message,
                                                  const std::string& file, int line) {
    auto& builder = context_->getBuilder();
    
    // Create filename character string constant
    auto* file_str = builder.CreateGlobalString(file);
    auto* line_val = llvm::ConstantInt::get(builder.getInt32Ty(), line);
    
    // Call paw_debug_assert
    auto* debug_assert_fn = context_->getRuntimeFunction("paw_debug_assert");
    if (!debug_assert_fn) return nullptr;
    
    return builder.CreateCall(debug_assert_fn, {condition, message, file_str, line_val});
}

llvm::Value* BuiltinCodeGen::generateUnreachable(const std::string& file, int line) {
    auto& builder = context_->getBuilder();
    
    // Create filename character string constant
    auto* file_str = builder.CreateGlobalString(file);
    auto* line_val = llvm::ConstantInt::get(builder.getInt32Ty(), line);
    
    // Call paw_unreachable
    auto* unreachable_fn = context_->getRuntimeFunction("paw_unreachable");
    if (!unreachable_fn) return nullptr;
    
    return builder.CreateCall(unreachable_fn, {file_str, line_val});
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// helper function
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Function* BuiltinCodeGen::declareRuntimeFunction(
    const std::string& name,
    llvm::Type* return_type,
    const std::vector<llvm::Type*>& param_types) {
    
    auto* func_type = llvm::FunctionType::get(return_type, param_types, false);
    auto* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        name,
        context_->getModule()
    );
    
    // registertocontext
    context_->registerRuntimeFunction(name, func);
    
    return func;
}

std::string BuiltinCodeGen::getRuntimePrintName(Type* type) {
    if (!type) return "";
    
    auto kind = type->getKind();
    
    // Signed integers
    if (kind == Type::Kind::I8) return "paw_print_i8";
    if (kind == Type::Kind::I16) return "paw_print_i16";
    if (kind == Type::Kind::I32) return "paw_print_i32";
    if (kind == Type::Kind::I64) return "paw_print_i64";
    if (kind == Type::Kind::I128) return "paw_print_i128";
    
    // Unsigned integers
    if (kind == Type::Kind::U8) return "paw_print_u8";
    if (kind == Type::Kind::U16) return "paw_print_u16";
    if (kind == Type::Kind::U32) return "paw_print_u32";
    if (kind == Type::Kind::U64) return "paw_print_u64";
    if (kind == Type::Kind::U128) return "paw_print_u128";
    
    // Floating-point numbers
    if (kind == Type::Kind::F8) return "paw_print_f8";
    if (kind == Type::Kind::F16) return "paw_print_f16";
    if (kind == Type::Kind::F32) return "paw_print_f32";
    if (kind == Type::Kind::F64) return "paw_print_f64";
    if (kind == Type::Kind::F128) return "paw_print_f128";
    
    // Other typestypes
    if (kind == Type::Kind::Bool) return "paw_print_bool";
    if (kind == Type::Kind::Char) return "paw_print_char";
    if (kind == Type::Kind::String) return "paw_print_string";
    
    return "";
}

std::string BuiltinCodeGen::getRuntimeToStringName(Type* type) {
    if (!type) return "";
    
    auto kind = type->getKind();
    
    // Signed integers
    if (kind == Type::Kind::I8) return "paw_i8_to_string";
    if (kind == Type::Kind::I16) return "paw_i16_to_string";
    if (kind == Type::Kind::I32) return "paw_i32_to_string";
    if (kind == Type::Kind::I64) return "paw_i64_to_string";
    if (kind == Type::Kind::I128) return "paw_i128_to_string";
    
    // Unsigned integers
    if (kind == Type::Kind::U8) return "paw_u8_to_string";
    if (kind == Type::Kind::U16) return "paw_u16_to_string";
    if (kind == Type::Kind::U32) return "paw_u32_to_string";
    if (kind == Type::Kind::U64) return "paw_u64_to_string";
    if (kind == Type::Kind::U128) return "paw_u128_to_string";
    
    // Floating-point numbers
    if (kind == Type::Kind::F8) return "paw_f8_to_string";
    if (kind == Type::Kind::F16) return "paw_f16_to_string";
    if (kind == Type::Kind::F32) return "paw_f32_to_string";
    if (kind == Type::Kind::F64) return "paw_f64_to_string";
    if (kind == Type::Kind::F128) return "paw_f128_to_string";
    
    // Other typestypes
    if (kind == Type::Kind::Bool) return "paw_bool_to_string";
    if (kind == Type::Kind::Char) return "paw_char_to_string";
    if (kind == Type::Kind::String) return "paw_string_to_string";
    
    return "";
}

llvm::Value* BuiltinCodeGen::generatePrint(llvm::Value* value, Type* type) {
    if (!value || !type) return nullptr;
    
    std::string func_name = getRuntimePrintName(type);
    if (func_name.empty()) return nullptr;
    
    // getordeclarationruntimefunction
    auto* print_fn = context_->getRuntimeFunction(func_name);
    if (!print_fn) {
        // self/fromdynamicdeclarationfunction
        TypeCodeGen type_gen(context_->getLLVMContext());
        auto* llvm_param_type = type_gen.mapType(type);
        auto* void_type = context_->getBuilder().getVoidTy();
        
        auto* func_type = llvm::FunctionType::get(void_type, {llvm_param_type}, false);
        print_fn = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            func_name,
            context_->getModule()
        );
        context_->registerRuntimeFunction(func_name, print_fn);
    }
    
    return context_->getBuilder().CreateCall(print_fn, {value});
}

llvm::Value* BuiltinCodeGen::generatePrintln(llvm::Value* value, Type* type) {
    if (!value || !type) return nullptr;
    
    // println = print + newline
    auto* print_results = generatePrint(value, type);
    
    // call paw_print_newline
    auto* newline_fn = context_->getRuntimeFunction("paw_print_newline");
    if (!newline_fn) {
        auto* void_type = context_->getBuilder().getVoidTy();
        auto* func_type = llvm::FunctionType::get(void_type, {}, false);
        newline_fn = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            "paw_print_newline",
            context_->getModule()
        );
        context_->registerRuntimeFunction("paw_print_newline", newline_fn);
    }
    
    context_->getBuilder().CreateCall(newline_fn, {});
    return print_results;
}

llvm::Value* BuiltinCodeGen::generateToString(llvm::Value* value, Type* type) {
    if (!value || !type) return nullptr;
    
    std::string func_name = getRuntimeToStringName(type);
    if (func_name.empty()) return nullptr;
    
    // getordeclarationruntimefunction
    auto* to_string_fn = context_->getRuntimeFunction(func_name);
    if (!to_string_fn) {
        // self/fromdynamicdeclarationfunction
        TypeCodeGen type_gen(context_->getLLVMContext());
        auto* llvm_param_type = type_gen.mapType(type);
        auto* string_type = llvm::PointerType::getUnqual(context_->getLLVMContext());
        
        auto* func_type = llvm::FunctionType::get(string_type, {llvm_param_type}, false);
        to_string_fn = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            func_name,
            context_->getModule()
        );
        context_->registerRuntimeFunction(func_name, to_string_fn);
    }
    
    return context_->getBuilder().CreateCall(to_string_fn, {value});
}

llvm::Value* BuiltinCodeGen::generateLen(llvm::Value* value, Type* type) {
    auto& builder = context_->getBuilder();
    
    if (!type) return nullptr;
    
    // 1. String: callpaw_string_len
    if (type->isString()) {
        auto* len_fn = context_->getRuntimeFunction("paw_string_len");
        if (!len_fn) {
            // declarefunction
            auto* size_t_type = builder.getInt64Ty();
            auto* ptr_type = llvm::PointerType::getUnqual(context_->getLLVMContext());
            auto* func_type = llvm::FunctionType::get(size_t_type, {ptr_type}, false);
            len_fn = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                "paw_string_len",
                context_->getModule()
            );
            context_->registerRuntimeFunction("paw_string_len", len_fn);
        }
        return builder.CreateCall(len_fn, {value});
    }
    
    // 2. Array: compile timeconstant
    if (type->isArray()) {
        auto* array_type = static_cast<ArrayType*>(type);
        size_t size = array_type->getSize();
        return llvm::ConstantInt::get(builder.getInt64Ty(), size);
    }
    
    // 3. Slice: fromstructbodyextractlenfield
    if (type->getKind() == Type::Kind::Slice) {
        // Slicestruct: { ptr: *T, len: u64 }
        llvm::Type* value_type = value->getType();
        
        // case/situation1: valueyespointertypes（pointing toslicestructbody/struct）
        if (value_type->isPointerTy()) {
            // Slicestruct: { ptr, i64 }
            // mapslicetypesgetstructbody/structtypes
            TypeCodeGen type_gen(context_->getLLVMContext());
            llvm::Type* slice_struct_type = type_gen.mapType(type);
            
            // useStructGEPvisitlenfield（field 1）
            llvm::Value* len_ptr = builder.CreateStructGEP(
                slice_struct_type,
                value,
                1,  // lenfieldindex
                "slice.len.ptr"
            );
            
            return builder.CreateLoad(builder.getInt64Ty(), len_ptr, "slice.len");
        }
        
        // case/situation2: valueyesstructbody/structvaluetypes
        else if (value_type->isStructTy()) {
            // directlyuseExtractValueextractfield 1
            return builder.CreateExtractValue(value, 1, "slice.len");
        }
        
        // case/situation3: typesmismatch，returnnullptr
        else {
            return nullptr;
        }
    }
    
    return nullptr;
}

} // namespace pawc
