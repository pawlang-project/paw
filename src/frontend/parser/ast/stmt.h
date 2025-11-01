//===--- stmt.h - Statement AST Nodes ----------------------------*- C++ -*-===//

#ifndef PAW_STMT_H
#define PAW_STMT_H

#include "ast_base.h"
#include "expr.h"
#include <string>

namespace pawc {

// ============================================================================
// 泛型参数和约束
// ============================================================================

/// GenericParam - 泛型参数 (T, U, V)
struct GenericParam {
    std::string name;  // 参数名称，如 "T"
    
    explicit GenericParam(const std::string& n) : name(n) {}
    
    bool operator==(const GenericParam& other) const {
        return name == other.name;
    }
};

/// WhereClause - where约束子句
struct WhereClause {
    std::string type_param;       // 类型参数，如 "T"
    std::string interface_name;   // 接口名称，如 "Display"
    
    WhereClause(const std::string& tp, const std::string& iface)
        : type_param(tp), interface_name(iface) {}
    
    bool operator==(const WhereClause& other) const {
        return type_param == other.type_param && 
               interface_name == other.interface_name;
    }
};

// ============================================================================
// Statement AST Nodes
// ============================================================================

/// ExprStmt - 表达式语句
class ExprStmt : public Stmt {
public:
    explicit ExprStmt(ExprPtr expr) : expr_(std::move(expr)) {}
    
    Expr* getExpr() const { return expr_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr expr_;
};

/// VarDecl - 变量声明 let x: T = value
class VarDecl : public Stmt {
public:
    VarDecl(const std::string& name, Type* type, bool is_mutable, ExprPtr init)
        : name_(name), type_(type), is_mutable_(is_mutable), init_(std::move(init)) {}
    
    const std::string& getName() const { return name_; }
    Type* getType() const { return type_; }
    void setType(Type* type) { type_ = type; }
    bool isMutable() const { return is_mutable_; }
    Expr* getInit() const { return init_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    Type* type_;
    bool is_mutable_;
    ExprPtr init_;
};

/// FunctionDecl - 函数声明
class FunctionDecl : public Stmt {
public:
    struct Param {
        std::string name;
        Type* type;
        bool is_mutable;
    };
    
    FunctionDecl(const std::string& name, 
                 std::vector<GenericParam> generic_params,
                 std::vector<Param> params,
                 Type* return_type, 
                 StmtPtr body,
                 std::vector<WhereClause> where_clauses = {})
        : name_(name), 
          generic_params_(std::move(generic_params)),
          params_(std::move(params)),
          return_type_(return_type), 
          body_(std::move(body)),
          where_clauses_(std::move(where_clauses)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<Param>& getParams() const { return params_; }
    Type* getReturnType() const { return return_type_; }
    Stmt* getBody() const { return body_.get(); }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    
    bool isGeneric() const { return !generic_params_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<GenericParam> generic_params_;  // 泛型参数 <T, U>
    std::vector<Param> params_;
    Type* return_type_;
    StmtPtr body_;
    std::vector<WhereClause> where_clauses_;   // where约束
};

/// ReturnStmt - return语句
class ReturnStmt : public Stmt {
public:
    explicit ReturnStmt(ExprPtr value) : value_(std::move(value)) {}
    
    Expr* getValue() const { return value_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr value_;
};

/// IfStmt - if语句
class IfStmt : public Stmt {
public:
    IfStmt(ExprPtr condition, StmtPtr then_stmt, StmtPtr else_stmt)
        : condition_(std::move(condition)),
          then_stmt_(std::move(then_stmt)),
          else_stmt_(std::move(else_stmt)) {}
    
    Expr* getCondition() const { return condition_.get(); }
    Stmt* getThenStmt() const { return then_stmt_.get(); }
    Stmt* getElseStmt() const { return else_stmt_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr condition_;
    StmtPtr then_stmt_;
    StmtPtr else_stmt_;
};

/// LoopStmt - loop语句（无限循环）
class LoopStmt : public Stmt {
public:
    explicit LoopStmt(StmtPtr body) : body_(std::move(body)) {}
    
    Stmt* getBody() const { return body_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    StmtPtr body_;
};

/// WhileStmt - loop + if + break组合（模拟while）
class WhileStmt : public Stmt {
public:
    WhileStmt(ExprPtr condition, StmtPtr body)
        : condition_(std::move(condition)), body_(std::move(body)) {}
    
    Expr* getCondition() const { return condition_.get(); }
    Stmt* getBody() const { return body_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr condition_;
    StmtPtr body_;
};

/// BreakStmt - break语句
class BreakStmt : public Stmt {
public:
    BreakStmt() = default;
    void accept(ASTVisitor* visitor) override;
};

/// ContinueStmt - continue语句
class ContinueStmt : public Stmt {
public:
    ContinueStmt() = default;
    void accept(ASTVisitor* visitor) override;
};

/// BlockStmt - 块语句
class BlockStmt : public Stmt {
public:
    explicit BlockStmt(std::vector<StmtPtr> stmts)
        : stmts_(std::move(stmts)) {}
    
    const std::vector<StmtPtr>& getStmts() const { return stmts_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<StmtPtr> stmts_;
};

/// ForStmt - for..in循环 for i in 0..10 { ... }
/// 注意：在PawLang中实际使用 loop i in 0..10 { ... } 语法
class ForStmt : public Stmt {
public:
    ForStmt(std::string var_name, ExprPtr iterator, StmtPtr body)
        : var_name_(std::move(var_name)),
          iterator_(std::move(iterator)),
          body_(std::move(body)) {}
    
    const std::string& getVarName() const { return var_name_; }
    Expr* getIterator() const { return iterator_.get(); }
    Stmt* getBody() const { return body_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string var_name_;
    ExprPtr iterator_;
    StmtPtr body_;
};

/// StructDecl - 结构体定义 type Point = struct { x: i32, y: i32 }
class StructDecl : public Stmt {
public:
    using Field = std::pair<std::string, Type*>;
    
    StructDecl(std::string name, 
               std::vector<GenericParam> generic_params,
               std::vector<Field> fields)
        : name_(std::move(name)), 
          generic_params_(std::move(generic_params)),
          fields_(std::move(fields)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<Field>& getFields() const { return fields_; }
    bool isGeneric() const { return !generic_params_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<GenericParam> generic_params_;  // 泛型参数 <T, U>
    std::vector<Field> fields_;
};

/// EnumVariant - 枚举变体
struct EnumVariant {
    std::string name;
    Type* data_type;  // nullable，如果变体无数据
    
    EnumVariant(std::string n, Type* t = nullptr)
        : name(std::move(n)), data_type(t) {}
};

/// EnumDecl - 枚举定义 type Status = enum { Active, Inactive }
class EnumDecl : public Stmt {
public:
    EnumDecl(std::string name, 
             std::vector<GenericParam> generic_params,
             std::vector<EnumVariant> variants)
        : name_(std::move(name)), 
          generic_params_(std::move(generic_params)),
          variants_(std::move(variants)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<EnumVariant>& getVariants() const { return variants_; }
    bool isGeneric() const { return !generic_params_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<GenericParam> generic_params_;  // 泛型参数 <T, U>
    std::vector<EnumVariant> variants_;
};

/// InterfaceMethod - 接口方法声明
struct InterfaceMethod {
    std::string name;
    std::vector<FunctionDecl::Param> params;
    Type* return_type;
    
    InterfaceMethod(std::string n, std::vector<FunctionDecl::Param> p, Type* r)
        : name(std::move(n)), params(std::move(p)), return_type(r) {}
};

/// InterfaceDecl - 接口定义 type Display = interface { fn show(); }
class InterfaceDecl : public Stmt {
public:
    InterfaceDecl(std::string name, 
                  std::vector<GenericParam> generic_params,
                  std::vector<InterfaceMethod> methods)
        : name_(std::move(name)), 
          generic_params_(std::move(generic_params)),
          methods_(std::move(methods)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<InterfaceMethod>& getMethods() const { return methods_; }
    bool isGeneric() const { return !generic_params_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<GenericParam> generic_params_;  // 泛型参数 <T, U>
    std::vector<InterfaceMethod> methods_;
};

/// SupportDecl - 接口实现 support Point with Display { methods }
class SupportDecl : public Stmt {
public:
    SupportDecl(std::string type_name, 
                std::vector<GenericParam> type_generic_params,
                std::string interface_name,
                std::vector<GenericParam> interface_generic_params,
                std::vector<std::unique_ptr<FunctionDecl>> methods,
                std::vector<WhereClause> where_clauses = {})
        : type_name_(std::move(type_name)),
          type_generic_params_(std::move(type_generic_params)),
          interface_name_(std::move(interface_name)),
          interface_generic_params_(std::move(interface_generic_params)),
          methods_(std::move(methods)),
          where_clauses_(std::move(where_clauses)) {}
    
    const std::string& getTypeName() const { return type_name_; }
    const std::vector<GenericParam>& getTypeGenericParams() const { return type_generic_params_; }
    const std::string& getInterfaceName() const { return interface_name_; }
    const std::vector<GenericParam>& getInterfaceGenericParams() const { return interface_generic_params_; }
    const std::vector<std::unique_ptr<FunctionDecl>>& getMethods() const { return methods_; }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string type_name_;
    std::vector<GenericParam> type_generic_params_;         // 类型的泛型参数
    std::string interface_name_;
    std::vector<GenericParam> interface_generic_params_;    // 接口的泛型参数 (如果适用)
    std::vector<std::unique_ptr<FunctionDecl>> methods_;
    std::vector<WhereClause> where_clauses_;               // where约束
};

} // namespace pawc

#endif // PAW_STMT_H
