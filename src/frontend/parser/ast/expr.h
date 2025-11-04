//===--- expr.h - Expression AST Nodes ---------------------------*- C++ -*-===//

#ifndef PAW_EXPR_H
#define PAW_EXPR_H

#include "ast_base.h"
#include "frontend/lexer/token.h"
#include <string>

namespace pawc {

/// IntLiteral - 整数字面量
class IntLiteral : public Expr {
public:
    explicit IntLiteral(const std::string& value) : value_(value) {}
    
    const std::string& getValue() const { return value_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string value_;
};

/// FloatLiteral - 浮点字面量
class FloatLiteral : public Expr {
public:
    explicit FloatLiteral(const std::string& value) : value_(value) {}
    
    const std::string& getValue() const { return value_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string value_;
};

/// BoolLiteral - 布尔字面量
class BoolLiteral : public Expr {
public:
    explicit BoolLiteral(bool value) : value_(value) {}
    
    bool getValue() const { return value_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    bool value_;
};

/// CharLiteral - 字符字面量
class CharLiteral : public Expr {
public:
    explicit CharLiteral(char value) : value_(value) {}
    
    char getValue() const { return value_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    char value_;
};

/// StringLiteral - 字符串字面量
class StringLiteral : public Expr {
public:
    explicit StringLiteral(const std::string& value) : value_(value) {}
    
    const std::string& getValue() const { return value_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string value_;
};

/// IdentifierExpr - 标识符表达式
class IdentifierExpr : public Expr {
public:
    explicit IdentifierExpr(const std::string& name) : name_(name) {}
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }  // 用于泛型函数单态化
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
};

/// SelfExpr - self表达式（用于接口方法）
class SelfExpr : public Expr {
public:
    SelfExpr() = default;
    void accept(ASTVisitor* visitor) override;
};

/// BinaryExpr - 二元运算表达式
class BinaryExpr : public Expr {
public:
    BinaryExpr(TokenType op, ExprPtr left, ExprPtr right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {}
    
    TokenType getOperator() const { return op_; }
    Expr* getLeft() const { return left_.get(); }
    Expr* getRight() const { return right_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    TokenType op_;
    ExprPtr left_;
    ExprPtr right_;
};

/// UnaryExpr - 一元运算表达式
class UnaryExpr : public Expr {
public:
    UnaryExpr(TokenType op, ExprPtr operand)
        : op_(op), operand_(std::move(operand)) {}
    
    TokenType getOperator() const { return op_; }
    Expr* getOperand() const { return operand_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    TokenType op_;
    ExprPtr operand_;
};

/// CallExpr - 函数调用
class CallExpr : public Expr {
public:
    CallExpr(ExprPtr callee, std::vector<ExprPtr> args)
        : callee_(std::move(callee)), args_(std::move(args)) {}
    
    Expr* getCallee() const { return callee_.get(); }
    const std::vector<ExprPtr>& getArgs() const { return args_; }
    
    // 🔧 泛型函数支持：类型参数
    const std::vector<Type*>& getTypeArgs() const { return type_args_; }
    void setTypeArgs(const std::vector<Type*>& type_args) { type_args_ = type_args; }
    bool hasTypeArgs() const { return !type_args_.empty(); }
    
    // 🔧 接口方法调用支持
    bool isMethodCall() const { return is_method_call_; }
    void setIsMethodCall(bool is) { is_method_call_ = is; }
    
    Expr* getReceiver() const { return receiver_; }
    void setReceiver(Expr* recv) { receiver_ = recv; }
    
    const std::string& getMethodName() const { return method_name_; }
    void setMethodName(const std::string& name) { method_name_ = name; }
    
    const std::string& getMethodTarget() const { return method_target_; }
    void setMethodTarget(const std::string& target) { method_target_ = target; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr callee_;
    std::vector<ExprPtr> args_;
    std::vector<Type*> type_args_;  // 泛型类型参数（如<i32>）
    
    // 方法调用支持（由TypeChecker设置）
    bool is_method_call_ = false;
    Expr* receiver_ = nullptr;  // 不拥有，只是引用MemberExpr中的object
    std::string method_name_;
    std::string method_target_;  // 完整的目标函数名（Type_method）
};

/// MemberExpr - 成员访问 a.b
class MemberExpr : public Expr {
public:
    MemberExpr(ExprPtr object, const std::string& member)
        : object_(std::move(object)), member_(member) {}
    
    Expr* getObject() const { return object_.get(); }
    const std::string& getMember() const { return member_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr object_;
    std::string member_;
};

/// StaticAccessExpr - 静态访问 Type::Variant 或 Type::method
class StaticAccessExpr : public Expr {
public:
    StaticAccessExpr(const std::string& type_name, const std::string& member)
        : type_name_(type_name), member_(member) {}
    
    const std::string& getTypeName() const { return type_name_; }
    const std::string& getMember() const { return member_; }
    void setTypeName(const std::string& name) { type_name_ = name; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string type_name_;
    std::string member_;
};

/// IndexExpr - 索引访问 a[i]
class IndexExpr : public Expr {
public:
    IndexExpr(ExprPtr object, ExprPtr index)
        : object_(std::move(object)), index_(std::move(index)) {}
    
    Expr* getObject() const { return object_.get(); }
    Expr* getIndex() const { return index_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr object_;
    ExprPtr index_;
};

/// IfExpr - if表达式
class IfExpr : public Expr {
public:
    IfExpr(ExprPtr condition, ExprPtr then_expr, ExprPtr else_expr)
        : condition_(std::move(condition)),
          then_expr_(std::move(then_expr)),
          else_expr_(std::move(else_expr)) {}
    
    Expr* getCondition() const { return condition_.get(); }
    Expr* getThenExpr() const { return then_expr_.get(); }
    Expr* getElseExpr() const { return else_expr_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr condition_;
    ExprPtr then_expr_;
    ExprPtr else_expr_;
};

/// BlockExpr - 块表达式 { ... }
class BlockExpr : public Expr {
public:
    explicit BlockExpr(std::vector<StmtPtr> stmts)
        : stmts_(std::move(stmts)) {}
    
    const std::vector<StmtPtr>& getStmts() const { return stmts_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<StmtPtr> stmts_;
};

/// ArrayLiteral - 数组字面量 [1, 2, 3]
class ArrayLiteral : public Expr {
public:
    explicit ArrayLiteral(std::vector<ExprPtr> elements)
        : elements_(std::move(elements)) {}
    
    const std::vector<ExprPtr>& getElements() const { return elements_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<ExprPtr> elements_;
};

/// TupleExpr - 元组字面量 (1, 2, 3)
class TupleExpr : public Expr {
public:
    explicit TupleExpr(std::vector<ExprPtr> elements)
        : elements_(std::move(elements)) {}
    
    const std::vector<ExprPtr>& getElements() const { return elements_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<ExprPtr> elements_;
};

/// RangeExpr - 范围表达式 0..10, 0..=10
class RangeExpr : public Expr {
public:
    RangeExpr(ExprPtr start, ExprPtr end, bool inclusive)
        : start_(std::move(start)),
          end_(std::move(end)),
          inclusive_(inclusive) {}
    
    Expr* getStart() const { return start_.get(); }
    Expr* getEnd() const { return end_.get(); }
    bool isInclusive() const { return inclusive_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr start_;
    ExprPtr end_;
    bool inclusive_;
};

/// FieldInit - 字段初始化（用于StructLiteral）
struct FieldInit {
    std::string name;
    ExprPtr value;
    
    FieldInit(std::string n, ExprPtr v)
        : name(std::move(n)), value(std::move(v)) {}
};

/// StructLiteral - 结构体字面量 Point { x: 10, y: 20 }
class StructLiteral : public Expr {
public:
    StructLiteral(std::string struct_name, std::vector<FieldInit> fields)
        : struct_name_(std::move(struct_name)), fields_(std::move(fields)) {}
    
    const std::string& getStructName() const { return struct_name_; }
    void setStructName(const std::string& name) { struct_name_ = name; }  // 用于单态化
    const std::vector<FieldInit>& getFields() const { return fields_; }
    std::vector<FieldInit>& getFields() { return fields_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string struct_name_;
    std::vector<FieldInit> fields_;
};

/// MatchArm - match表达式的一个分支 (pattern => expression)
struct MatchArm {
    std::unique_ptr<class Pattern> pattern;
    ExprPtr guard;       // 守卫条件 (可选, if condition)
    ExprPtr expression;
    
    MatchArm(std::unique_ptr<class Pattern> p, ExprPtr e, ExprPtr g = nullptr)
        : pattern(std::move(p)), guard(std::move(g)), expression(std::move(e)) {}
};

/// MatchExpr - match表达式 value is { pattern => expr, ... }
class MatchExpr : public Expr {
public:
    MatchExpr(ExprPtr scrutinee, std::vector<MatchArm> arms)
        : scrutinee_(std::move(scrutinee)), arms_(std::move(arms)) {}
    
    Expr* getScrutinee() const { return scrutinee_.get(); }
    const std::vector<MatchArm>& getArms() const { return arms_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr scrutinee_;  // 被匹配的表达式
    std::vector<MatchArm> arms_;  // 所有分支
};

/// TryExpr - Try表达式 expr? (自动传播Result错误)
class TryExpr : public Expr {
public:
    TryExpr(ExprPtr expr, TokenType op) 
        : expr_(std::move(expr)), operator_(op) {}
    
    Expr* getExpr() const { return expr_.get(); }
    TokenType getOperator() const { return operator_; }
    bool isOptionalTry() const { return operator_ == TokenType::QUESTION; }
    bool isResultTry() const { return operator_ == TokenType::BANG; }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr expr_;
    TokenType operator_;  // QUESTION(?) for Optional, BANG(!) for Result
};

/// NoneLiteral - none字面量 (Optional空值)
class NoneLiteral : public Expr {
public:
    NoneLiteral() = default;
    void accept(ASTVisitor* visitor) override;
};

/// CastExpr - 类型转换表达式 expr as Type
class CastExpr : public Expr {
public:
    CastExpr(ExprPtr expr, Type* target_type)
        : expr_(std::move(expr)), target_type_(target_type) {}
    
    Expr* getExpr() const { return expr_.get(); }
    Type* getTargetType() const { return target_type_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr expr_;
    Type* target_type_;
};

/// ClosureExpr - 闭包表达式 (x: i32, y: i32) -> i32 { body }
class ClosureExpr : public Expr {
public:
    struct Param {
        std::string name;
        Type* type;
        bool is_mutable;
        
        Param(std::string n, Type* t, bool mut = false)
            : name(std::move(n)), type(t), is_mutable(mut) {}
    };
    
    struct CapturedVar {
        std::string name;
        Type* type;
        bool by_reference;  // true: 引用捕获，false: 值捕获
        
        CapturedVar(std::string n, Type* t, bool ref = false)
            : name(std::move(n)), type(t), by_reference(ref) {}
    };
    
    ClosureExpr(std::vector<Param> params, Type* return_type, ExprPtr body)
        : params_(std::move(params)), 
          return_type_(return_type),
          body_(std::move(body)) {}
    
    const std::vector<Param>& getParams() const { return params_; }
    Type* getReturnType() const { return return_type_; }
    void setReturnType(Type* type) { return_type_ = type; }
    
    Expr* getBody() const { return body_.get(); }
    
    // 捕获变量管理
    const std::vector<CapturedVar>& getCapturedVars() const { return captured_vars_; }
    void addCapturedVar(const CapturedVar& var) { captured_vars_.push_back(var); }
    
    // 生成的闭包函数名（CodeGen时设置）
    const std::string& getGeneratedName() const { return generated_name_; }
    void setGeneratedName(const std::string& name) { generated_name_ = name; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<Param> params_;
    Type* return_type_;
    ExprPtr body_;
    std::vector<CapturedVar> captured_vars_;
    std::string generated_name_;  // 生成的函数名
};

} // namespace pawc

#endif // PAW_EXPR_H
