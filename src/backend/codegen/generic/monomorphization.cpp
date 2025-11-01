//===--- monomorphization.cpp - Generic Monomorphization Impl ----*- C++ -*-===//

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
    
    // 检查缓存
    auto it = type_cache_.find(instance_name);
    if (it != type_cache_.end()) {
        return it->second;
    }
    
    // 使用CodeGenContext的类型映射
    llvm::Type* llvm_type = context_->getLLVMType(paw_type);
    
    // 缓存结果
    type_cache_[instance_name] = llvm_type;
    
    return llvm_type;
}

llvm::Function* GenericMonomorphization::getOrCreateMonomorphizedFunction(
    const std::string& instance_name,
    const std::vector<llvm::Type*>& param_types,
    llvm::Type* return_type) {
    
    // 检查缓存
    auto it = function_cache_.find(instance_name);
    if (it != function_cache_.end()) {
        return it->second;
    }
    
    // 创建函数类型
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        return_type,
        param_types,
        false  // not vararg
    );
    
    // 创建函数
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        instance_name,
        context_->getModule()
    );
    
    // 缓存
    function_cache_[instance_name] = func;
    
    return func;
}

void GenericMonomorphization::clearCache() {
    type_cache_.clear();
    function_cache_.clear();
}

} // namespace pawc

