//===--- expr.cpp - Expression AST Implementation ---------------*- C++ -*-===//

#include "expr.h"
#include "frontend/parser/visitor.h"

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 表达式Visitor接受方法
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void IntLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void FloatLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void BoolLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void CharLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StringLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void NullLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void IdentifierExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void SelfExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void BinaryExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void UnaryExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void CallExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void MemberExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StaticAccessExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void IndexExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void IfExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void BlockExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void ArrayLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void TupleExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StructLiteral::accept(ASTVisitor* visitor) { visitor->visit(this); }
void RangeExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void MatchExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void TryExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }
void ClosureExpr::accept(ASTVisitor* visitor) { visitor->visit(this); }

} // namespace pawc
