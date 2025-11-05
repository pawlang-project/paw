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

/// BuiltinCodeGen - Builtinfunctioncode generation
///
/// process18types/kindstypesof/theprint/println/to_stringoverload
class BuiltinCodeGen {
public:
    explicit BuiltinCodeGen(CodeGenContext* context);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // declareAllRuntimefunction
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void declareAllRuntimeFunctions();
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // generationBuiltinfunctioncall
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    /// according totypesgenerateprintcall
    llvm::Value* generatePrint(llvm::Value* value, Type* type);
    
    /// according totypesgenerateprintlncall
    llvm::Value* generatePrintln(llvm::Value* value, Type* type);
    
    /// according totypesgenerateto_stringcall
    llvm::Value* generateToString(llvm::Value* value, Type* type);
    
    /// generationlencall (supportstring, array, slice)
    llvm::Value* generateLen(llvm::Value* value, Type* type);
    
    /// generationpaniccall
    llvm::Value* generatePanic(llvm::Value* message);
    
    /// generationassertcall
    llvm::Value* generateAssert(llvm::Value* condition, llvm::Value* message,
                                const std::string& file, int line);
    
    /// generationdebug_assertcall
    llvm::Value* generateDebugAssert(llvm::Value* condition, llvm::Value* message,
                                     const std::string& file, int line);
    
    /// generationunreachablecall
    llvm::Value* generateUnreachable(const std::string& file, int line);
    
private:
    CodeGenContext* context_;
    
    // getspecifictypesof/theruntimefunction name
    std::string getRuntimePrintName(Type* type);
    std::string getRuntimeToStringName(Type* type);
    
    // declaresingleindividual/pieceruntimefunction
    llvm::Function* declareRuntimeFunction(
        const std::string& name,
        llvm::Type* return_type,
        const std::vector<llvm::Type*>& param_types
    );
};

} // namespace pawc

#endif // PAW_BUILTIN_CODEGEN_H
