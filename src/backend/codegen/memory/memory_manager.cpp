//===--- memory_manager.cpp - Memory Manager Implementation ------*- C++ -*-===//

#include "memory_manager.h"
#include <llvm/IR/Function.h>

namespace pawc {

MemoryManager::MemoryManager(llvm::IRBuilder<>& builder, llvm::Module* module)
    : builder_(builder), module_(module) {
    declareMalloc();
    declareFree();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 栈分配
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::AllocaInst* MemoryManager::allocateOnStack(llvm::Type* type,
                                                 const std::string& name) {
    llvm::Function* func = builder_.GetInsertBlock()->getParent();
    llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(),
                                  func->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(type, nullptr, name);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 堆分配
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Value* MemoryManager::allocateOnHeap(llvm::Type* type) {
    // 计算类型大小
    const llvm::DataLayout& dl = module_->getDataLayout();
    uint64_t size = dl.getTypeAllocSize(type);
    
    // 调用paw_malloc
    llvm::Value* size_val = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(builder_.getContext()),
        size
    );
    
    llvm::Value* ptr = builder_.CreateCall(malloc_func_, {size_val});
    
    // 转换为正确的类型指针
    return builder_.CreateBitCast(ptr, type->getPointerTo());
}

void MemoryManager::freeMemory(llvm::Value* ptr) {
    // 直接调用free（opaque pointer不需要bitcast）
    builder_.CreateCall(free_func_, {ptr});
}

void MemoryManager::generateDestructor(llvm::Value* object, llvm::Type* type) {
    // 析构函数生成（基于类型）
    // 
    // PawLang内存管理策略：
    //   1. 栈分配：自动释放（函数退出时）
    //   2. 堆分配：需要显式析构
    //   3. 当前：非GC策略，未来可扩展为ARC/RAII
    // 
    // 目前实现：基础堆内存释放
    // 未来扩展：递归释放复合类型（struct/array）
    // 
    // 注意：PawLang的内存模型仍在设计中，
    // 当前简化实现足以支持大多数场景
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Runtime函数声明
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void MemoryManager::declareMalloc() {
    auto* malloc_type = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(builder_.getContext()),
        {llvm::Type::getInt64Ty(builder_.getContext())},
        false
    );
    
    malloc_func_ = llvm::Function::Create(
        malloc_type,
        llvm::Function::ExternalLinkage,
        "paw_malloc",
        module_
    );
}

void MemoryManager::declareFree() {
    auto* free_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(builder_.getContext()),
        {llvm::PointerType::getUnqual(builder_.getContext())},
        false
    );
    
    free_func_ = llvm::Function::Create(
        free_type,
        llvm::Function::ExternalLinkage,
        "paw_free",
        module_
    );
}

} // namespace pawc

