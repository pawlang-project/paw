//===--- control_flow_codegen.cpp - Control Flow CodeGen --------*- C++ -*-===//
/// @file control_flow_codegen.cpp
/// @brief Code generation implementation
//
// control flowcode generation：IfStmt, LoopStmt, WhileStmt, BreakStmt, ContinueStmtetc
// fromstmt_codegen.cppmiddle/centerextract
//
//===----------------------------------------------------------------------===//

#include "stmt_codegen.h"
#include "../expr/expr_codegen.h"
#include "frontend/parser/ast/stmt.h"
#include <llvm/IR/BasicBlock.h>

namespace pawc {

// control flowstatementcode generationlogickeep/reservein/atstmt_codegen.cppmiddle/center
// due toinvolve complexityof/theBasicBlockmanage，temporarilytime/whennotsplit

void StmtCodeGen::visit(IfStmt* node) {
    results_ = nullptr;
}

void StmtCodeGen::visit(LoopStmt* node) {
    results_ = nullptr;
}

void StmtCodeGen::visit(WhileStmt* node) {
    results_ = nullptr;
}

void StmtCodeGen::visit(BreakStmt* node) {
    results_ = nullptr;
}

void StmtCodeGen::visit(ContinueStmt* node) {
    results_ = nullptr;
}

void StmtCodeGen::visit(ForStmt* node) {
    results_ = nullptr;
}

void StmtCodeGen::visit(ReturnStmt* node) {
    results_ = nullptr;
}

} // namespace pawc

