//===--- pattern.cpp - Pattern AST Implementation ---------------*- C++ -*-===//
/// @file pattern.cpp
/// @brief AST node implementation

#include "pattern.h"
#include "frontend/parser/visitor.h"

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pattern visitor accept method
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void LiteralPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void WildcardPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void VariablePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void TuplePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void EnumPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StructPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void ArrayPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void SlicePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void RangePattern::accept(ASTVisitor* visitor) { visitor->visit(this); }
void OrPattern::accept(ASTVisitor* visitor) { visitor->visit(this); }

} // namespace pawc
