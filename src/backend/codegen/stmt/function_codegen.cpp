//===--- function_codegen.cpp - Function Declaration CodeGen ----*- C++ -*-===//
/// @file function_codegen.cpp
/// @brief Code generation implementation
//
// functiondeclarationcode generation：FunctionDecl
// fromstmt_codegen.cppmiddle/centerextract
//
//===----------------------------------------------------------------------===//

#include "stmt_codegen.h"
#include "../expr/expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/stmt.h"
#include <llvm/IR/Function.h>

namespace pawc {

void StmtCodeGen::visit(FunctionDecl* node) {
    // functiondeclarationcode generationlogickeep/reservein/atstmt_codegen.cppmiddle/center
    // due toinvolve complexityof/thescopeandcontextmanage，temporarilytime/whennotsplit
    results_ = nullptr;
}

} // namespace pawc

