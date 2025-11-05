//===--- decl.cpp - Declaration AST Implementation ---------------*- C++ -*-===//
/// @file decl.cpp
/// @brief AST node implementation

#include "decl.h"
#include "frontend/parser/visitor.h"

namespace pawc {

void FunctionDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void StructDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void EnumDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void InterfaceDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }
void SupportDecl::accept(ASTVisitor* visitor) { visitor->visit(this); }

} // namespace pawc

