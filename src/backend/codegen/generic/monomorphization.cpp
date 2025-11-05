//===--- monomorphization.cpp - Generic Monomorphization Impl ----*- C++ -*-===//
/// @file monomorphization.cpp
/// @brief Implementation file

#include "monomorphization.h"
#include "backend/codegen/codegen_context.h"
#include "middleend/types/type.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>

namespace pawc {

GenericMonomorphization::GenericMonomorphization(CodeGenContext* context)
    : context_(context) {}

llvm::Type* GenericMonomorphization::mapMonomorphizedType(
    Type* paw_type, 
    const std::string& instance_name) {
    
    // Check cache first
    auto it = type_cache_.find(instance_name);
    if (it != type_cache_.end()) {
        return it->second;
    }
    
    // useCodeGenContextof/thetypesmap
    llvm::Type* llvm_type = context_->getLLVMType(paw_type);
    
    // Cache results
    type_cache_[instance_name] = llvm_type;
    
    return llvm_type;
}

llvm::Function* GenericMonomorphization::getOrCreateMonomorphizedFunction(
    const std::string& instance_name,
    const std::vector<llvm::Type*>& param_types,
    llvm::Type* return_type) {
    
    // Check cache first
    auto it = function_cache_.find(instance_name);
    if (it != function_cache_.end()) {
        return it->second;
    }
    
    // createfunctiontypes
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        return_type,
        param_types,
        false  // not vararg
    );
    
    // createfunction
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        instance_name,
        context_->getModule()
    );
    
    // cache
    function_cache_[instance_name] = func;
    
    return func;
}

void GenericMonomorphization::clearCache() {
    type_cache_.clear();
    function_cache_.clear();
}

} // namespace pawc

