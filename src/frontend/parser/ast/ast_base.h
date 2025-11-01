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

// 前向声明
class ASTVisitor;

/// ASTNode - 所有AST节点的基类
class ASTNode {
public:
    virtual ~ASTNode() = default;
    
    SourceLocation getLocation() const { return location_; }
    void setLocation(const SourceLocation& loc) { location_ = loc; }
    
    virtual void accept(ASTVisitor* visitor) = 0;
    
protected:
    SourceLocation location_;
};

/// Expr - 表达式基类
class Expr : public ASTNode {
public:
    Type* getType() const { return type_; }
    void setType(Type* type) { type_ = type; }
    
protected:
    Type* type_ = nullptr;
};

/// Stmt - 语句基类
class Stmt : public ASTNode {};

/// Decl - 声明基类
class Decl : public ASTNode {};

/// Pattern - 模式基类（用于match）
class Pattern : public ASTNode {};

// 智能指针别名
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;
using PatternPtr = std::unique_ptr<Pattern>;

} // namespace pawc

#endif // PAW_AST_BASE_H
