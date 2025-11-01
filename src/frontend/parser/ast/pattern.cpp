//===--- pattern.cpp - Pattern AST Implementations ----------------*- C++ -*-===//

#include "pattern.h"
#include "frontend/parser/visitor.h"

namespace pawc {

void LiteralPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void WildcardPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void VariablePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void TuplePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void EnumPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }

} // namespace pawc

