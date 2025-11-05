//===--- call_codegen.cpp - Call Expression CodeGen -------------*- C++ -*-===//
/// @file call_codegen.cpp
/// @brief Code generation implementation
//
// Function call code generation (strictly follows ARCHITECTURE.md)
// Supports: regular functions, builtin functions, closure calls
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
    
    // === 🔧 Bug Fix: process interface method call ===
    if (node->isMethodCall()) {
        std::cerr << "[MethodCall] Calling method: " << node->getMethodTarget() << std::endl;
        
        // Look up method function
        llvm::Function* method = context_->getModule()->getFunction(node->getMethodTarget());
        if (!method) {
            std::cerr << "[MethodCall] Method not found!" << std::endl;
            results_ = nullptr;
            return;
        }
        
        std::cerr << "[MethodCall] Method found, generating receiver" << std::endl;
        
        // Generate receiver (self)
        node->getReceiver()->accept(this);
        llvm::Value* receiver = results_;
        
        if (!receiver) {
            std::cerr << "[MethodCall] Receiver is null!" << std::endl;
            results_ = nullptr;
            return;
        }
        
        std::cerr << "[MethodCall] Receiver generated, is pointer: " << receiver->getType()->isPointerTy() << std::endl;
        
        // 🔧 Reference type support: check if method's first parameter is reference type
        // If it's a reference type, need to pass pointer instead of value
        llvm::Value* receiver_arg = receiver;
        
        if (method->arg_size() > 0) {
            llvm::Type* first_param_type = method->getArg(0)->getType();
            
            // If first parameter is pointer type (reference), need to get receiver's address
            if (first_param_type->isPointerTy() && !receiver->getType()->isPointerTy()) {
                // Receiver is value, but parameter needs pointer
                // Create a temporary variable to store receiver and pass its address
                auto& builder = context_->getBuilder();
                llvm::AllocaInst* temp = builder.CreateAlloca(receiver->getType(), nullptr, "receiver.tmp");
                builder.CreateStore(receiver, temp);
                receiver_arg = temp;
            } else if (!first_param_type->isPointerTy() && receiver->getType()->isPointerTy()) {
                // Receiver is pointer, but parameter needs value
                // Load value (LLVM 21 opaque pointer, need to infer from receiver type)
                auto& builder = context_->getBuilder();
                // Get actual type from Paw type system
                Type* receiver_paw_type = node->getReceiver()->getType();
                llvm::Type* receiver_llvm_type = context_->getLLVMType(receiver_paw_type);
                receiver_arg = builder.CreateLoad(receiver_llvm_type, receiver, "receiver.val");
            }
        }
        
        // Generate parameter list
        std::vector<llvm::Value*> args;
        
        // 🔧 Smart parameter passing: only pass when method expects receiver
        // If method has parameters (expects receiver), pass receiver as first parameter
        if (method->arg_size() > 0) {
            args.push_back(receiver_arg);
        }
        
        // Add other parameters
        for (const auto& arg : node->getArgs()) {
            arg->accept(this);
            if (results_) {
                args.push_back(results_);
            }
        }
        
        // Call method
        results_ = builder.CreateCall(method, args);
        return;
    }
    
    // 🔧 Generic function call process
    if (node->hasTypeArgs()) {
        if (auto* ident = dynamic_cast<IdentifierExpr*>(node->getCallee())) {
            // Generic function call: identity<i32>(42)
            std::cerr << "[DEBUG] Detected generic function call: " << ident->getName() << std::endl;
            results_ = generateGenericFunctionCall(ident->getName(), 
                                                  node->getTypeArgs(), 
                                                  node->getArgs());
            std::cerr << "[DEBUG] Generic function call complete" << std::endl;
            return;
        }
    }
    
    // 🔧 M7: check if it's a generic enum constructor
    if (auto* static_access = dynamic_cast<StaticAccessExpr*>(node->getCallee())) {
        // This is a static access call：Option::Some(42)
        Type* callee_type = static_access->getType();
        
        if (callee_type && callee_type->isFunction()) {
            // This is an enum constructor！
            FunctionType* func_type = static_cast<FunctionType*>(callee_type);
            Type* return_type = func_type->getReturnType();
            
            if (return_type && return_type->isEnum()) {
                // Construct enum value
                EnumType* enum_type = static_cast<EnumType*>(return_type);
                
                // lookupvariantindex
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
                    results_ = nullptr;
                    return;
                }
                
                // Get enum's LLVM type
                llvm::Type* enum_llvm_type = context_->getLLVMType(enum_type);
                
                // createenum struct: {i32 variant_index, data}
                llvm::Value* enum_value = llvm::UndefValue::get(enum_llvm_type);
                
                // setvariantindex
                enum_value = builder.CreateInsertValue(
                    enum_value,
                    builder.getInt32(variant_index),
                    {0}
                );
                
                // If there's associated data, set the data
                if (variant_data_type && !node->getArgs().empty()) {
                    llvm::Value* arg_value = nullptr;
                    
                    // 🔧 Multi-parameter support: check if tuple wrapping is needed
                    if (variant_data_type->isTuple()) {
                        // Multi-parameter: wrap as tuple
                        TupleType* tuple_type = static_cast<TupleType*>(variant_data_type);
                        std::vector<llvm::Value*> tuple_values;
                        
                        for (size_t i = 0; i < node->getArgs().size(); ++i) {
                            node->getArgs()[i]->accept(this);
                            tuple_values.push_back(results_);
                        }
                        
                        // Construct tuple value
                        llvm::Type* tuple_llvm_type = context_->getLLVMType(tuple_type);
                        llvm::Value* tuple_val = llvm::UndefValue::get(tuple_llvm_type);
                        
                        for (size_t i = 0; i < tuple_values.size(); ++i) {
                            tuple_val = builder.CreateInsertValue(tuple_val, tuple_values[i], {static_cast<unsigned>(i)});
                        }
                        
                        arg_value = tuple_val;
                    } else {
                        // Single parameter
                        node->getArgs()[0]->accept(this);
                        arg_value = results_;
                    }
                    
                    if (arg_value) {
                        enum_value = builder.CreateInsertValue(
                            enum_value,
                            arg_value,
                            {1}
                        );
                    }
                }
                
                results_ = enum_value;
                return;
            }
        }
    }
    
    // === Step 1: check if it's a closure call ===
    Type* callee_type = node->getCallee()->getType();
    
    if (callee_type && callee_type->getKind() == Type::Kind::Function) {
        // === Closure call！===
        
        // Get capture flag from FunctionType (set by Sema)
        auto* func_type_ast = static_cast<FunctionType*>(callee_type);
        bool has_captures = func_type_ast->hasCaptures();
        
        // Generate closure object/function pointer
        node->getCallee()->accept(this);
        llvm::Value* closure_value = results_;
        
        if (!closure_value) {
            results_ = nullptr;
            return;
        }
        
        // generationparameter
        std::vector<llvm::Value*> args;
        for (const auto& arg : node->getArgs()) {
            arg->accept(this);
            if (results_) {
                args.push_back(results_);
            }
        }
        
        // === Closure call logic ===
        // Build user-level FunctionType (without context)
        std::vector<llvm::Type*> param_types_llvm;
        for (Type* param_type : func_type_ast->getParamTypes()) {
            param_types_llvm.push_back(context_->getLLVMType(param_type));
        }
        llvm::Type* return_type_llvm = context_->getLLVMType(func_type_ast->getReturnType());
        
        llvm::Value* callable = nullptr;
        llvm::FunctionType* fn_type = nullptr;
        std::vector<llvm::Value*> final_args;
        
        // Case 1: directly an llvm::Function* (immediate call of capture-less closure)
        if (llvm::isa<llvm::Function>(closure_value)) {
            callable = closure_value;
            fn_type = llvm::cast<llvm::Function>(closure_value)->getFunctionType();
            final_args = args;
        }
        // Case 2: is a pointer (alloca, call from variable)
        else if (closure_value->getType()->isPointerTy()) {
            // Load closure value from alloca
            llvm::Value* loaded_closure = builder.CreateLoad(
                llvm::PointerType::getUnqual(builder.getContext()),
                closure_value,
                "loaded_closure"
            );
            
            // Determine if it's a capture-less or capturing closure
            // Strategy:
            // - If clearly has captures（closure_expr exists and has_captures is true），process as struct
            // - Otherwise (no captures or uncertain)，default to function pointer process
            
            if (has_captures) {
                // Case 2b: clearly has captures - process as closure struct pointer
                llvm::Value* closure_struct_ptr = loaded_closure;
                
                // Extract function pointer from closure struct（field 0）
                // Method: use opaque pointer model
                llvm::Value* ptr_array = closure_struct_ptr;
                
                // Load first pointer (function pointer)
                llvm::Value* fn_ptr = builder.CreateLoad(
                    llvm::PointerType::getUnqual(builder.getContext()),
                    ptr_array,
                    "closure_fn"
                );
                
                callable = fn_ptr;
                
                // Build FunctionType (including context parameter)
                std::vector<llvm::Type*> param_types_with_ctx;
                param_types_with_ctx.push_back(llvm::PointerType::getUnqual(builder.getContext()));
                for (auto* pt : param_types_llvm) {
                    param_types_with_ctx.push_back(pt);
                }
                fn_type = llvm::FunctionType::get(return_type_llvm, param_types_with_ctx, false);
                
                // Pass closure struct pointer as first parameter
                final_args.push_back(closure_struct_ptr);
                for (auto* arg : args) {
                    final_args.push_back(arg);
                }
            }
            else {
                // Case 2a: nocaptureornoway/cannotcertain/sure - default to function pointer process
                callable = loaded_closure;
                
                // Build simple FunctionType (no context parameter)
                fn_type = llvm::FunctionType::get(return_type_llvm, param_types_llvm, false);
                final_args = args;
            }
        }
        else {
            results_ = nullptr;
            return;
        }
        
        if (!callable || !fn_type) {
            results_ = nullptr;
            return;
        }
        
        // createcall
        if (fn_type->getReturnType()->isVoidTy()) {
            // Void return type should not have a name
            results_ = builder.CreateCall(fn_type, callable, final_args);
        } else {
            results_ = builder.CreateCall(fn_type, callable, final_args, "closure_call");
        }
        return;
    }
    
    // === Step 2: normal/regularfunctioncall（packageincludingbuiltin）===
    std::string func_name;
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node->getCallee())) {
        func_name = ident->getName();
    } else {
        // Complex callee expression (like method call), not yet supported
        results_ = nullptr;
        return;
    }
    
    // Generate parameters and collect types
    std::vector<llvm::Value*> arg_values;
    std::vector<Type*> arg_types;
    
    for (const auto& arg : node->getArgs()) {
        arg->accept(this);
        if (results_) {
            arg_values.push_back(results_);
            
            // Collect parameter types (get from AST)
            Type* arg_type = arg->getType();
            if (arg_type) {
                arg_types.push_back(arg_type);
            }
        } else {
            // parametergeneratefailure
            results_ = nullptr;
            return;
        }
    }
    
    // === Special handling: ok(value) - Result constructor ===
    if (func_name == "ok" && arg_values.size() == 1) {
        // ok(value) return Result<T> = { true, value, nullptr }
        // Type is obtained from CallExpr's type (already set by Sema)
        Type* results_type_ast = node->getType();
        
        llvm::Type* results_llvm_type = context_->getLLVMType(results_type_ast);
        
        // createResultvalue: { i1 is_ok, T value, ptr error_message }
        llvm::Value* results_value = llvm::UndefValue::get(results_llvm_type);
        
        // setis_ok = true
        results_value = builder.CreateInsertValue(results_value,
            llvm::ConstantInt::get(builder.getInt1Ty(), 1),  // true
            0);
        
        // setvalue
        results_value = builder.CreateInsertValue(results_value, arg_values[0], 1);
        
        // seterror = nullptr
        results_value = builder.CreateInsertValue(results_value,
            llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(builder.getContext())
            ),
            2);
        
        results_ = results_value;
        return;
    }
    
    // === Special handling: err(message) - Result constructor ===
    if (func_name == "err" && arg_values.size() == 1) {
        // err(message) return Result<T> = { false, undef, message }
        // T is inferred from CallExpr's type (already set by Sema)
        Type* results_type_ast = node->getType();
        
        llvm::Type* results_llvm_type = context_->getLLVMType(results_type_ast);
        
        // createResultvalue: { i1 is_ok, T value, ptr error_message }
        llvm::Value* results_value = llvm::UndefValue::get(results_llvm_type);
        
        // setis_ok = false
        results_value = builder.CreateInsertValue(results_value,
            llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // false
            0);
        
        // Set value as undef (not used)
        
        // seterror = message
        results_value = builder.CreateInsertValue(results_value, arg_values[0], 2);
        
        results_ = results_value;
        return;
    }
    
    // === Special handling: some(value) - Optional constructor ===
    if (func_name == "some" && arg_values.size() == 1) {
        // some(value) return Optional<T> = { true, value }
        // T is inferred from value's type (already set by Sema)
        Type* optional_type_ast = node->getType();
        
        llvm::Type* optional_llvm_type = context_->getLLVMType(optional_type_ast);
        
        // createOptionalvalue: { i1 has_value, T value }
        llvm::Value* optional_value = llvm::UndefValue::get(optional_llvm_type);
        
        // sethas_value = true
        optional_value = builder.CreateInsertValue(optional_value,
            llvm::ConstantInt::get(builder.getInt1Ty(), 1),  // true
            0);
        
        // setvalue
        optional_value = builder.CreateInsertValue(optional_value, arg_values[0], 1);
        
        results_ = optional_value;
        return;
    }
    
    // Check if it's a builtin function (full support for all 18 types)
    if (func_name == "println" || func_name == "print" || func_name == "to_string") {
        if (arg_types.size() == 1) {
            // Get type signature
            std::string type_suffix = context_->getTypeSignature(arg_types);
            
            if (func_name == "println" || func_name == "print") {
                // Special handling for f128: need to convert fp128 to struct for passing
                if (arg_types[0]->getKind() == Type::Kind::F128) {
                    llvm::Value* fp128_value = arg_values[0];
                    
                    // Convert fp128 to i128 (bit representation)
                    llvm::Value* bits = builder.CreateBitCast(fp128_value,
                        llvm::Type::getInt128Ty(builder.getContext()));
                    
                    // Extract low 64 bits and high 64 bits
                    llvm::Value* low = builder.CreateTrunc(bits, builder.getInt64Ty());
                    llvm::Value* high = builder.CreateTrunc(
                        builder.CreateLShr(bits, llvm::ConstantInt::get(
                            llvm::Type::getInt128Ty(builder.getContext()), 64)),
                        builder.getInt64Ty()
                    );
                    
                    // createstruct {i64, i64}
                    llvm::StructType* f128_struct = llvm::StructType::get(builder.getContext(), {
                        builder.getInt64Ty(),
                        builder.getInt64Ty()
                    });
                    
                    llvm::Value* struct_val = llvm::UndefValue::get(f128_struct);
                    struct_val = builder.CreateInsertValue(struct_val, low, {0});
                    struct_val = builder.CreateInsertValue(struct_val, high, {1});
                    
                    // declarepaw_print_f128
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
                    // Other types: normal process
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
                
                // If it's println, also output newline
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
                
                results_ = llvm::ConstantInt::get(builder.getInt1Ty(), 0); // voidreturn
                return;
            } else if (func_name == "to_string") {
                // to_stringreturnstring (ptr)
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
                results_ = builder.CreateCall(to_str_func, {arg_values[0]});
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
            results_ = builder.CreateCall(len_func, {arg_values[0]});
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
            results_ = nullptr; // Won't return
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
            results_ = llvm::ConstantInt::get(builder.getInt1Ty(), 0);
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
            results_ = nullptr;
            return;
        }
    }
    
    // Look up regular function
    llvm::Function* func = context_->lookupFunctionWithTypes(func_name, arg_types);
    
    if (!func) {
        // Try to look up non-overloaded function (user-defined function)
        func = context_->lookupFunction(func_name);
    }
    
    if (!func) {
        // Function not found
        results_ = nullptr;
        return;
    }
    
    // 🔧 Fix: ensure parameter types match function signature
    // For struct types, if types don't match, use pointer bitcast conversion
    std::vector<llvm::Value*> final_arg_values;
    auto func_arg_it = func->arg_begin();
    
    for (size_t i = 0; i < arg_values.size() && func_arg_it != func->arg_end(); ++i, ++func_arg_it) {
        llvm::Value* arg_val = arg_values[i];
        llvm::Type* expected_type = func_arg_it->getType();
        llvm::Type* actual_type = arg_val->getType();
        
        // If types don't match, need to convert
        if (actual_type != expected_type) {
            // For struct types: use pointer bitcast (more reliable method)
            if (actual_type->isStructTy() && expected_type->isStructTy()) {
                // Check if they have same number of elements and types
                auto* actual_struct = llvm::cast<llvm::StructType>(actual_type);
                auto* expected_struct = llvm::cast<llvm::StructType>(expected_type);
                
                // If element count and types match, use pointer conversion
                if (actual_struct->getNumElements() == expected_struct->getNumElements()) {
                    // Create temporary alloca to store source value
                    llvm::AllocaInst* temp_alloca = builder.CreateAlloca(actual_type, nullptr, "temp.struct");
                    builder.CreateStore(arg_val, temp_alloca);
                    
                    // Bitcast alloca pointer to target type's pointer (opaque pointer model)
                    llvm::Value* cast_ptr = temp_alloca;
                    
                    // Load as expected type
                    arg_val = builder.CreateLoad(expected_type, cast_ptr, "converted.struct");
                }
            }
        }
        final_arg_values.push_back(arg_val);
    }
    
    // createcall
    // 🔧 Bug Fix: void function should not have named return value
    if (func->getReturnType()->isVoidTy()) {
        builder.CreateCall(func, final_arg_values);
        results_ = nullptr;  // Void function has no return value
    } else {
        results_ = builder.CreateCall(func, final_arg_values, "calltmp");
    }
}

// genericfunctioncallCodeGen
llvm::Value* ExprCodeGen::generateGenericFunctionCall(
    const std::string& func_name,
    const std::vector<Type*>& type_args,
    const std::vector<std::unique_ptr<Expr>>& args) {
    
    auto& builder = context_->getBuilder();
    
    // 1. lookupgenerictemplate
    auto* tmpl = context_->getTypeSystem()->lookupGenericTemplate(func_name);
    if (!tmpl || tmpl->kind != GenericTemplate::FUNCTION) {
        return nullptr;
    }
    
    // 2. generateinstantiationfunction name
    std::string instance_name = func_name;
    for (const auto* type_arg : type_args) {
        instance_name += "_" + type_arg->toString();
    }
    
    // 3. checkfunctionyesnoalreadygenerate
    llvm::Function* llvm_func = context_->getModule()->getFunction(instance_name);
    
    if (!llvm_func) {
        // 4. generateinstantiationfunction
        llvm_func = generateGenericFunctionInstance(func_name, type_args, tmpl);
        if (!llvm_func) {
            return nullptr;
        }
    }
    
    // 5. evaluateparameter
    std::vector<llvm::Value*> arg_values;
    for (const auto& arg : args) {
        arg->accept(this);
        if (results_) {
            arg_values.push_back(results_);
        }
    }
    
    // 6. callinstantiationfunction
    return builder.CreateCall(llvm_func, arg_values);
}

// generationgenericfunctioninstance
llvm::Function* ExprCodeGen::generateGenericFunctionInstance(
    const std::string& func_name,
    const std::vector<Type*>& type_args,
    GenericTemplate* tmpl) {
    
    FunctionDecl* func_def = tmpl->func_def;
    
    // 1. createtypessubstitutionmap
    std::unordered_map<std::string, Type*> type_substitution;
    for (size_t i = 0; i < tmpl->type_params.size() && i < type_args.size(); i++) {
        type_substitution[tmpl->type_params[i]] = type_args[i];
    }
    
    // 2. substitutionparametertypesandreturntypes
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
    
    // 3. createfunctiontypesandfunction
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
    
    // 4. Generate function body
    // Save current basic block and insertion point
    auto& builder = context_->getBuilder();
    llvm::BasicBlock* saved_bb = builder.GetInsertBlock();
    
    // createentryblock
    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(
        context_->getLLVMContext(), "entry", llvm_func);
    builder.SetInsertPoint(entry_bb);
    
    // 5. bindparameter
    // enternewof/thescope
    context_->enterScope();
    
    size_t idx = 0;
    for (auto& arg : llvm_func->args()) {
        if (idx < func_def->getParams().size()) {
            const std::string& param_name = func_def->getParams()[idx].name;
            Type* param_type = func_def->getParams()[idx].type;
            Type* substituted_param_type = type_system->substituteType(
                param_type, type_substitution);
            
            // createallocaandstorageparameter
            llvm::Type* param_llvm_type = context_->getLLVMType(substituted_param_type);
            llvm::Value* alloca = builder.CreateAlloca(param_llvm_type, nullptr, param_name);
            builder.CreateStore(&arg, alloca);
            
            // in/atscopemiddle/centerdefinitionvariable
            context_->defineVariable(param_name, alloca);
            idx++;
        }
    }
    
    // 6. generatefunctionbody/structstatement
    if (func_def->getBody()) {
        // useStmtCodeGenfuturegeneratefunctionbody/struct
        // needcreateone/aindividual/pieceTemporaryof/thetypesmapenvironmentfutureprocessgenericparameter
        
        // right/correctat/insimplesinglecase/situation：ifbodyonlyhasone/aindividual/piecereturnstatement
        // wemayinlinegenerate
        auto* block_stmt = dynamic_cast<BlockStmt*>(func_def->getBody());
        if (!block_stmt) {
            context_->exitScope();
            if (saved_bb) builder.SetInsertPoint(saved_bb);
            return llvm_func;
        }
        
        const auto& statements = block_stmt->getStmts();
        
        // simplify：onlyprocesssingleindividual/piecereturnstatement
        if (statements.size() == 1) {
            if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(statements[0].get())) {
                if (ret_stmt->getValue()) {
                    // generationreturnexpression
                    ret_stmt->getValue()->accept(this);
                    if (results_) {
                        builder.CreateRet(results_);
                    }
                }
            }
        }
    }
    
    // 7. Restore scope and insertion point
    context_->exitScope();
    
    if (saved_bb) {
        builder.SetInsertPoint(saved_bb);
    }
    
    return llvm_func;
}

} // namespace pawc
