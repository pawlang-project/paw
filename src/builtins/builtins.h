#ifndef PAWC_BUILTINS_H
#define PAWC_BUILTINS_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <unordered_map>

namespace pawc {

// 内置函数管理器
class Builtins {
public:
    Builtins(llvm::LLVMContext& context, llvm::Module& module);
    
    // 声明所有内置函数
    void declareAll();
    
    // 获取内置函数
    llvm::Function* getFunction(const std::string& name);
    
    // 检查是否是内置函数
    bool isBuiltin(const std::string& name) const;
    
private:
    llvm::LLVMContext& context_;
    llvm::Module& module_;
    std::unordered_map<std::string, llvm::Function*> builtins_;
    
    // 声明各个内置函数
    void declarePrintf();   // 声明libc的printf
    void declareStrcat();   // 声明libc的strcat
    void declareStrcpy();   // 声明libc的strcpy
    void declareStrlen();   // 声明libc的strlen
    void declareMalloc();   // 声明libc的malloc
    void declareMemcpy();   // 声明libc的memcpy
    void declarePrint();
    void declarePrintln();
    void declareEprint();
    void declareEprintln();
    
    // 辅助函数：创建函数类型
    llvm::FunctionType* createPrintFunctionType();
};

} // namespace pawc

#endif // PAWC_BUILTINS_H

