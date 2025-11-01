//===--- memory_manager.h - Memory Management --------------------*- C++ -*-===//
//
// PawLang Compiler - Memory Management (No GC)
//
//===----------------------------------------------------------------------===//

#ifndef PAW_MEMORY_MANAGER_H
#define PAW_MEMORY_MANAGER_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <string>

namespace pawc {

/// MemoryManager - 内存管理器（无GC）
///
/// 策略：
/// - Stack-first: 默认栈分配
/// - RAII: 自动析构
/// - Box<T>: 显式堆分配
class MemoryManager {
public:
    MemoryManager(llvm::IRBuilder<>& builder, llvm::Module* module);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 栈分配（默认）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::AllocaInst* allocateOnStack(llvm::Type* type,
                                      const std::string& name);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 堆分配（显式：Box<T>）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::Value* allocateOnHeap(llvm::Type* type);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 释放内存
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void freeMemory(llvm::Value* ptr);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 析构器生成（RAII）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void generateDestructor(llvm::Value* object, llvm::Type* type);
    
private:
    llvm::IRBuilder<>& builder_;
    llvm::Module* module_;
    
    // Runtime函数
    llvm::Function* malloc_func_ = nullptr;
    llvm::Function* free_func_ = nullptr;
    
    void declareMalloc();
    void declareFree();
};

} // namespace pawc

#endif // PAW_MEMORY_MANAGER_H

