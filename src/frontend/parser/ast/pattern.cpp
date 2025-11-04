//===--- pattern.cpp - Pattern AST Implementation ---------------*- C++ -*-===//

#include "pattern.h"
#include "frontend/parser/visitor.h"

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 模式Visitor接受方法
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void LiteralPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void WildcardPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void VariablePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void TuplePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void EnumPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StructPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void ArrayPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void SlicePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }

} // namespace pawc
