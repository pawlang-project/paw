//===--- function_codegen.cpp - Function Declaration CodeGen ----*- C++ -*-===//
//
// 函数声明代码生成：FunctionDecl
// 从stmt_codegen.cpp中提取
//
//===----------------------------------------------------------------------===//

#include "stmt_codegen.h"
#include "../expr/expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/stmt.h"
#include <llvm/IR/Function.h>

namespace pawc {

void StmtCodeGen::visit(FunctionDecl* node) {
    // 函数声明代码生成逻辑保留在stmt_codegen.cpp中
    // 由于涉及复杂的scope和context管理，暂时不拆分
    result_ = nullptr;
}

} // namespace pawc

