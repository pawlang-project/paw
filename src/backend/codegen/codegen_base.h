//===--- codegen_base.h - Code Generation Base Class -------------*- C++ -*-===//
//
// PawLang Compiler - CodeGen Base
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_BASE_H
#define PAW_CODEGEN_BASE_H

#include "codegen_context.h"
#include <llvm/IR/Value.h>

namespace pawc {

class ASTNode;  // 前向声明

/// CodeGenBase - 所有CodeGen类的基类
class CodeGenBase {
public:
    explicit CodeGenBase(CodeGenContext* context) : context_(context) {}
    virtual ~CodeGenBase() = default;
    
    virtual llvm::Value* generate(ASTNode* node) = 0;
    
protected:
    CodeGenContext* context_;
};

} // namespace pawc

#endif // PAW_CODEGEN_BASE_H
