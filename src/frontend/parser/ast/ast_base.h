//===--- ast_base.h - AST Base Classes ---------------------------*- C++ -*-===//
//
// PawLang Compiler - AST Base
//
//===----------------------------------------------------------------------===//

#ifndef PAW_AST_BASE_H
#define PAW_AST_BASE_H

#include "diagnostics/source_location.h"
#include "middleend/types/type.h"
#include <memory>
#include <vector>

namespace pawc {

// Forward declaration
class ASTVisitor;

/// ASTNode - Base class for all AST nodes
class ASTNode {
public:
    virtual ~ASTNode() = default;
    
    SourceLocation getLocation() const { return location_; }
    void setLocation(const SourceLocation& loc) { location_ = loc; }
    
    virtual void accept(ASTVisitor* visitor) = 0;
    
protected:
    SourceLocation location_;
};

/// Expr - Expression base class
class Expr : public ASTNode {
public:
    Type* getType() const { return type_; }
    void setType(Type* type) { type_ = type; }
    
protected:
    Type* type_ = nullptr;
};

/// Stmt - Statement base class
class Stmt : public ASTNode {};

/// Decl - Declaration base class
class Decl : public ASTNode {};

/// Pattern - Pattern base class (used for match)
class Pattern : public ASTNode {};

// Smart pointer aliases
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;
using PatternPtr = std::unique_ptr<Pattern>;

} // namespace pawc

#endif // PAW_AST_BASE_H
