//===--- stmt.cpp - Statement Implementation ---------------------*- C++ -*-===//

#include "stmt.h"
#include "../visitor.h"

namespace pawc {

void ExprStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void VarDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void DestructuringDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StructDestructuringDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void FunctionDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void ReturnStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void IfStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void LoopStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void WhileStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void BreakStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void ContinueStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void BlockStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void ForStmt::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StructDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void EnumDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void InterfaceDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void SupportDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }

} // namespace pawc

