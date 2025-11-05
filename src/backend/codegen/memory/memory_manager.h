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

/// MemoryManager - insidememorymanager（noGC）
///
/// Strategy:
/// - Stack-first: defaultstackallocate
/// - RAII: self/fromdynamicdestruct/destruction
/// - Box<T>: explicitstyle/formheapallocate
class MemoryManager {
public:
    MemoryManager(llvm::IRBuilder<>& builder, llvm::Module* module);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // stackallocate（default）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::AllocaInst* allocateOnStack(llvm::Type* type,
                                      const std::string& name);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // heapallocate（explicitstyle/form：Box<T>）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::Value* allocateOnHeap(llvm::Type* type);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // releaseinsidememory
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void freeMemory(llvm::Value* ptr);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // destruct/destructionlinker/ergenerate（RAII）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void generateDestructor(llvm::Value* object, llvm::Type* type);
    
private:
    llvm::IRBuilder<>& builder_;
    llvm::Module* module_;
    
    // Runtimefunction
    llvm::Function* malloc_func_ = nullptr;
    llvm::Function* free_func_ = nullptr;
    
    void declareMalloc();
    void declareFree();
};

} // namespace pawc

#endif // PAW_MEMORY_MANAGER_H

