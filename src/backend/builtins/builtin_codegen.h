//===--- builtin_codegen.h - Builtin Function CodeGen ------------*- C++ -*-===//
//
// PawLang Compiler - Builtin Functions Code Generation
//
//===----------------------------------------------------------------------===//

#ifndef PAW_BUILTIN_CODEGEN_H
#define PAW_BUILTIN_CODEGEN_H

#include "backend/codegen/codegen_context.h"
#include "middleend/types/type.h"

#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <string>
#include <vector>

namespace pawc {

/// BuiltinCodeGen - Builtin函数代码生成
///
/// 处理18种类型的print/println/to_string重载
class BuiltinCodeGen {
public:
    explicit BuiltinCodeGen(CodeGenContext* context);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 声明所有Runtime函数
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void declareAllRuntimeFunctions();
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 生成Builtin函数调用
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    /// 根据类型生成print调用
    llvm::Value* generatePrint(llvm::Value* value, Type* type);
    
    /// 根据类型生成println调用
    llvm::Value* generatePrintln(llvm::Value* value, Type* type);
    
    /// 根据类型生成to_string调用
    llvm::Value* generateToString(llvm::Value* value, Type* type);
    
    /// 生成len调用 (支持string, array, slice)
    llvm::Value* generateLen(llvm::Value* value, Type* type);
    
    /// 生成panic调用
    llvm::Value* generatePanic(llvm::Value* message);
    
    /// 生成assert调用
    llvm::Value* generateAssert(llvm::Value* condition, llvm::Value* message,
                                const std::string& file, int line);
    
    /// 生成debug_assert调用
    llvm::Value* generateDebugAssert(llvm::Value* condition, llvm::Value* message,
                                     const std::string& file, int line);
    
    /// 生成unreachable调用
    llvm::Value* generateUnreachable(const std::string& file, int line);
    
private:
    CodeGenContext* context_;
    
    // 获取特定类型的runtime函数名
    std::string getRuntimePrintName(Type* type);
    std::string getRuntimeToStringName(Type* type);
    
    // 声明单个runtime函数
    llvm::Function* declareRuntimeFunction(
        const std::string& name,
        llvm::Type* return_type,
        const std::vector<llvm::Type*>& param_types
    );
};

} // namespace pawc

#endif // PAW_BUILTIN_CODEGEN_H
