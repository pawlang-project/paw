//===--- memory_manager.cpp - Memory Manager Implementation ------*- C++ -*-===//
/// @file memory_manager.cpp
/// @brief Implementation file

#include "memory_manager.h"
#include <llvm/IR/Function.h>

namespace pawc {

MemoryManager::MemoryManager(llvm::IRBuilder<>& builder, llvm::Module* module)
    : builder_(builder), module_(module) {
    declareMalloc();
    declareFree();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// stackallocate
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::AllocaInst* MemoryManager::allocateOnStack(llvm::Type* type,
                                                 const std::string& name) {
    llvm::Function* func = builder_.GetInsertBlock()->getParent();
    llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(),
                                  func->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(type, nullptr, name);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// heapallocate
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Value* MemoryManager::allocateOnHeap(llvm::Type* type) {
    // computetypessize
    const llvm::DataLayout& dl = module_->getDataLayout();
    uint64_t size = dl.getTypeAllocSize(type);
    
    // callpaw_malloc
    llvm::Value* size_val = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(builder_.getContext()),
        size
    );
    
    llvm::Value* ptr = builder_.CreateCall(malloc_func_, {size_val});
    
    // Return pointer (opaque pointer model - no bitcast needed)
    return ptr;
}

void MemoryManager::freeMemory(llvm::Value* ptr) {
    // directlycallfree（opaque pointernotneedbitcast）
    builder_.CreateCall(free_func_, {ptr});
}

void MemoryManager::generateDestructor(llvm::Value* object, llvm::Type* type) {
    // destruct/destructionfunctiongenerate（based ontypes）
    // 
    // PawLangmemory managementStrategy:
    //   1. stackallocate：self/fromdynamicrelease（functionexittime/when）
    //   2. heapallocate：needexplicitstyle/formdestruct/destruction
    //   3. current：notGCpolicy，not yetfuturecanextendis/asARC/RAII
    // 
    // currentlyfront/beforeimplementation：baseheapmemory deallocation
    // not yetfutureextend：recursionreleasecompositetypes（struct/array）
    // 
    // Note：PawLangof/theinsidememorymodelstillin/atdesignmiddle/center，
    // currentsimplifyimplementationsufficientsupportlarge/bigmany/muchnumberscenario
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Runtime function declarations
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

