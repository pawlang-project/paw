#ifndef PAWC_AST_H
#define PAWC_AST_H

#include "pawc/common.h"
#include <memory>
#include <vector>

namespace pawc {

// 前向声明
struct Expr;
struct Stmt;
struct Type;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using TypePtr = std::unique_ptr<Type>;

// ====== 类型 ======

struct Type {
    enum class Kind {
        Primitive,  // i32, f64, bool等
        Array,      // [T]
        Function,   // fn(T1, T2) -> T3
        Named,      // 自定义类型
        Generic,    // 泛型参数 T, U等
        SelfType,   // Self类型（在struct方法中使用）
        Optional    // T? 可选类型（错误处理）
    };
    
    Kind kind;
    SourceLocation location;
    
    virtual ~Type() = default;
    
protected:
    Type(Kind k, const SourceLocation& loc) : kind(k), location(loc) {}
};

struct PrimitiveTypeNode : Type {
    PrimitiveType prim_type;
    
    PrimitiveTypeNode(PrimitiveType pt, const SourceLocation& loc)
        : Type(Kind::Primitive, loc), prim_type(pt) {}
};

struct NamedTypeNode : Type {
    std::string name;
    std::vector<TypePtr> generic_args;  // 泛型参数 Result<i32>
    
    NamedTypeNode(const std::string& n, std::vector<TypePtr> gen_args, const SourceLocation& loc)
        : Type(Kind::Named, loc), name(n), generic_args(std::move(gen_args)) {}
};

struct GenericTypeNode : Type {
    std::string name;  // T, U等
    
    GenericTypeNode(const std::string& n, const SourceLocation& loc)
        : Type(Kind::Generic, loc), name(n) {}
};

// Self类型（在struct方法中使用）
struct SelfTypeNode : Type {
    SelfTypeNode(const SourceLocation& loc)
        : Type(Kind::SelfType, loc) {}
};

// Optional类型: T?（用于错误处理）
struct OptionalTypeNode : Type {
    TypePtr inner_type;
    
    OptionalTypeNode(TypePtr inner, const SourceLocation& loc)
        : Type(Kind::Optional, loc), inner_type(std::move(inner)) {}
};

struct ArrayTypeNode : Type {
    TypePtr element_type;
    int size;  // 数组大小，-1表示不定大小
    
    ArrayTypeNode(TypePtr elem_type, int sz, const SourceLocation& loc)
        : Type(Kind::Array, loc), element_type(std::move(elem_type)), size(sz) {}
};

// ====== 表达式 ======

struct Expr {
    enum class Kind {
        Integer, Float, Boolean, String, Identifier,
        Binary, Unary, Call, Index, Assign,
        MemberAccess,      // obj.field
        StructLiteral,     // Counter { value: 10 }
        EnumVariant,       // Result::Ok(42)
        ArrayLiteral,      // [1, 2, 3]
        Match,             // value is { pattern => expr }
        Is,                // x is Some(y) - 用于条件判断
        Cast,              // x as i32 - 类型转换
        IfExpr,            // if cond { a } else { b } - 内联if表达式
        Try,               // expr? - 错误传播
        Ok,                // ok(value) - 创建成功值
        Err                // err(message) - 创建错误
    };
    
    Kind kind;
    SourceLocation location;
    
    virtual ~Expr() = default;
    
protected:
    Expr(Kind k, const SourceLocation& loc) : kind(k), location(loc) {}
};

struct IntegerExpr : Expr {
    int64_t value;
    
    IntegerExpr(int64_t v, const SourceLocation& loc)
        : Expr(Kind::Integer, loc), value(v) {}
};

struct FloatExpr : Expr {
    double value;
    
    FloatExpr(double v, const SourceLocation& loc)
        : Expr(Kind::Float, loc), value(v) {}
};

struct BooleanExpr : Expr {
    bool value;
    
    BooleanExpr(bool v, const SourceLocation& loc)
        : Expr(Kind::Boolean, loc), value(v) {}
};

struct StringExpr : Expr {
    std::string value;
    
    StringExpr(const std::string& v, const SourceLocation& loc)
        : Expr(Kind::String, loc), value(v) {}
};

struct IdentifierExpr : Expr {
    std::string name;
    
    IdentifierExpr(const std::string& n, const SourceLocation& loc)
        : Expr(Kind::Identifier, loc), name(n) {}
};

struct BinaryExpr : Expr {
    enum class Op {
        Add, Sub, Mul, Div, Mod,
        Eq, Ne, Lt, Le, Gt, Ge,
        And, Or
    };
    
    Op op;
    ExprPtr left;
    ExprPtr right;
    
    BinaryExpr(Op o, ExprPtr l, ExprPtr r, const SourceLocation& loc)
        : Expr(Kind::Binary, loc), op(o), left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExpr : Expr {
    enum class Op { Neg, Not };
    
    Op op;
    ExprPtr operand;
    
    UnaryExpr(Op o, ExprPtr operand, const SourceLocation& loc)
        : Expr(Kind::Unary, loc), op(o), operand(std::move(operand)) {}
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
    std::vector<TypePtr> type_arguments;  // 泛型类型参数 func<i32, string>(...)
    std::string module_prefix;            // 模块前缀 math::add 中的 "math"
    
    CallExpr(ExprPtr c, std::vector<ExprPtr> args, const SourceLocation& loc)
        : Expr(Kind::Call, loc), callee(std::move(c)), arguments(std::move(args)) {}
    
    CallExpr(ExprPtr c, std::vector<TypePtr> type_args, std::vector<ExprPtr> args, const SourceLocation& loc)
        : Expr(Kind::Call, loc), callee(std::move(c)), arguments(std::move(args)), 
          type_arguments(std::move(type_args)) {}
    
    // 带模块前缀的调用 math::add(1, 2)
    CallExpr(const std::string& module, ExprPtr c, std::vector<ExprPtr> args, const SourceLocation& loc)
        : Expr(Kind::Call, loc), callee(std::move(c)), arguments(std::move(args)), 
          module_prefix(module) {}
    
    // 跨模块泛型调用 math::add<i32>(1, 2)
    CallExpr(const std::string& module, ExprPtr c, std::vector<TypePtr> type_args, 
             std::vector<ExprPtr> args, const SourceLocation& loc)
        : Expr(Kind::Call, loc), callee(std::move(c)), arguments(std::move(args)),
          type_arguments(std::move(type_args)), module_prefix(module) {}
};

struct AssignExpr : Expr {
    std::string target;      // 变量名（普通赋值）
    ExprPtr target_expr;     // 成员赋值（obj.field）
    ExprPtr value;
    
    // 普通变量赋值
    AssignExpr(const std::string& t, ExprPtr v, const SourceLocation& loc)
        : Expr(Kind::Assign, loc), target(t), target_expr(nullptr), value(std::move(v)) {}
    
    // 成员赋值
    AssignExpr(ExprPtr tgt_expr, ExprPtr v, const SourceLocation& loc)
        : Expr(Kind::Assign, loc), target(""), target_expr(std::move(tgt_expr)), value(std::move(v)) {}
};

// 类型转换表达式（as操作符）
struct CastExpr : Expr {
    ExprPtr expression;
    TypePtr target_type;
    
    CastExpr(ExprPtr expr, TypePtr type, const SourceLocation& loc)
        : Expr(Kind::Cast, loc), expression(std::move(expr)), target_type(std::move(type)) {}
};

// If表达式: if condition { then_expr } else { else_expr }
struct IfExpr : Expr {
    ExprPtr condition;
    ExprPtr then_expr;
    ExprPtr else_expr;
    
    IfExpr(ExprPtr cond, ExprPtr then_e, ExprPtr else_e, const SourceLocation& loc)
        : Expr(Kind::IfExpr, loc), condition(std::move(cond)), 
          then_expr(std::move(then_e)), else_expr(std::move(else_e)) {}
};

// Try表达式: expr? - 错误传播
struct TryExpr : Expr {
    ExprPtr expression;
    
    TryExpr(ExprPtr expr, const SourceLocation& loc)
        : Expr(Kind::Try, loc), expression(std::move(expr)) {}
};

// Ok表达式: ok(value) - 创建成功值
struct OkExpr : Expr {
    ExprPtr value;
    
    OkExpr(ExprPtr val, const SourceLocation& loc)
        : Expr(Kind::Ok, loc), value(std::move(val)) {}
};

// Err表达式: err(message) - 创建错误
struct ErrExpr : Expr {
    ExprPtr message;
    
    ErrExpr(ExprPtr msg, const SourceLocation& loc)
        : Expr(Kind::Err, loc), message(std::move(msg)) {}
};

struct IndexExpr : Expr {
    ExprPtr array;
    ExprPtr index;
    
    IndexExpr(ExprPtr arr, ExprPtr idx, const SourceLocation& loc)
        : Expr(Kind::Index, loc), array(std::move(arr)), index(std::move(idx)) {}
};

struct ArrayLiteralExpr : Expr {
    std::vector<ExprPtr> elements;
    
    ArrayLiteralExpr(std::vector<ExprPtr> elems, const SourceLocation& loc)
        : Expr(Kind::ArrayLiteral, loc), elements(std::move(elems)) {}
};

struct MemberAccessExpr : Expr {
    ExprPtr object;
    std::string member;
    
    MemberAccessExpr(ExprPtr obj, const std::string& mem, const SourceLocation& loc)
        : Expr(Kind::MemberAccess, loc), object(std::move(obj)), member(mem) {}
};

struct FieldInit {
    std::string name;
    ExprPtr value;
};

struct StructLiteralExpr : Expr {
    std::string type_name;
    std::vector<FieldInit> fields;
    
    StructLiteralExpr(const std::string& name, std::vector<FieldInit> flds, const SourceLocation& loc)
        : Expr(Kind::StructLiteral, loc), type_name(name), fields(std::move(flds)) {}
};

struct EnumVariantExpr : Expr {
    std::string enum_name;
    std::string variant_name;
    std::vector<ExprPtr> values;  // 关联值
    
    EnumVariantExpr(const std::string& en, const std::string& vn, 
                    std::vector<ExprPtr> vals, const SourceLocation& loc)
        : Expr(Kind::EnumVariant, loc), enum_name(en), variant_name(vn), 
          values(std::move(vals)) {}
};

struct Pattern;
using PatternPtr = std::unique_ptr<Pattern>;

struct MatchArm {
    PatternPtr pattern;
    ExprPtr expression;
};

struct MatchExpr : Expr {
    ExprPtr value;
    std::vector<MatchArm> arms;
    
    MatchExpr(ExprPtr val, std::vector<MatchArm> a, const SourceLocation& loc)
        : Expr(Kind::Match, loc), value(std::move(val)), arms(std::move(a)) {}
};

// Is表达式: x is Some(y) - 返回bool，用于条件判断
struct IsExpr : Expr {
    ExprPtr value;
    PatternPtr pattern;
    
    IsExpr(ExprPtr val, PatternPtr pat, const SourceLocation& loc)
        : Expr(Kind::Is, loc), value(std::move(val)), pattern(std::move(pat)) {}
};

// ====== 模式 ======

struct Pattern {
    enum class Kind {
        Wildcard,       // _
        Identifier,     // x
        Literal,        // 42, "hello"
        EnumVariant,    // Some(x), Ok(value)
        Struct          // Point { x, y }
    };
    
    Kind kind;
    SourceLocation location;
    
    virtual ~Pattern() = default;
    
protected:
    Pattern(Kind k, const SourceLocation& loc) : kind(k), location(loc) {}
};

struct WildcardPattern : Pattern {
    WildcardPattern(const SourceLocation& loc)
        : Pattern(Kind::Wildcard, loc) {}
};

struct IdentifierPattern : Pattern {
    std::string name;
    
    IdentifierPattern(const std::string& n, const SourceLocation& loc)
        : Pattern(Kind::Identifier, loc), name(n) {}
};

struct EnumVariantPattern : Pattern {
    std::string enum_name;
    std::string variant_name;
    std::vector<PatternPtr> bindings;  // 绑定的变量
    
    EnumVariantPattern(const std::string& en, const std::string& vn,
                      std::vector<PatternPtr> binds, const SourceLocation& loc)
        : Pattern(Kind::EnumVariant, loc), enum_name(en), variant_name(vn),
          bindings(std::move(binds)) {}
};

// ====== 语句 ======

struct Stmt {
    enum class Kind {
        Expression, Let, Return, If, Loop, Block, Function,
        Struct, Enum, TypeAlias, Impl, Break, Continue, Import, Extern
    };
    
    Kind kind;
    SourceLocation location;
    
    virtual ~Stmt() = default;
    
protected:
    Stmt(Kind k, const SourceLocation& loc) : kind(k), location(loc) {}
};

struct ExprStmt : Stmt {
    ExprPtr expression;
    
    ExprStmt(ExprPtr expr, const SourceLocation& loc)
        : Stmt(Kind::Expression, loc), expression(std::move(expr)) {}
};

struct LetStmt : Stmt {
    std::string name;
    bool is_mutable;
    TypePtr type;  // 可选
    ExprPtr initializer;  // 可选
    
    LetStmt(const std::string& n, bool mut, TypePtr t, ExprPtr init, const SourceLocation& loc)
        : Stmt(Kind::Let, loc), name(n), is_mutable(mut), 
          type(std::move(t)), initializer(std::move(init)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value;  // 可选
    
    ReturnStmt(ExprPtr v, const SourceLocation& loc)
        : Stmt(Kind::Return, loc), value(std::move(v)) {}
};

struct BreakStmt : Stmt {
    BreakStmt(const SourceLocation& loc)
        : Stmt(Kind::Break, loc) {}
};

struct ContinueStmt : Stmt {
    ContinueStmt(const SourceLocation& loc)
        : Stmt(Kind::Continue, loc) {}
};

struct ImportStmt : Stmt {
    std::string module_path;  // "math" or "utils/helper"
    
    ImportStmt(const std::string& path, const SourceLocation& loc)
        : Stmt(Kind::Import, loc), module_path(path) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
    
    BlockStmt(std::vector<StmtPtr> stmts, const SourceLocation& loc)
        : Stmt(Kind::Block, loc), statements(std::move(stmts)) {}
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch;  // 可选
    
    IfStmt(ExprPtr cond, StmtPtr then_b, StmtPtr else_b, const SourceLocation& loc)
        : Stmt(Kind::If, loc), condition(std::move(cond)), 
          then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
};

struct LoopStmt : Stmt {
    enum class LoopKind {
        Condition,      // loop condition {}
        Infinite,       // loop {}
        Iterator,       // loop item in array {}
        Range           // loop x in 0..100 {}
    };
    
    LoopKind loop_kind;
    ExprPtr condition;              // 条件循环的条件
    std::string iterator_var;       // 迭代器变量名
    ExprPtr iterable;              // 被迭代的数组
    ExprPtr range_start;           // 范围起始
    ExprPtr range_end;             // 范围结束
    StmtPtr body;
    
    // 条件循环/无限循环
    LoopStmt(ExprPtr cond, StmtPtr b, const SourceLocation& loc)
        : Stmt(Kind::Loop, loc), 
          loop_kind(cond ? LoopKind::Condition : LoopKind::Infinite),
          condition(std::move(cond)), body(std::move(b)) {}
    
    // 迭代器循环
    LoopStmt(std::string var, ExprPtr iter, StmtPtr b, const SourceLocation& loc)
        : Stmt(Kind::Loop, loc),
          loop_kind(LoopKind::Iterator),
          iterator_var(std::move(var)),
          iterable(std::move(iter)),
          body(std::move(b)) {}
    
    // 范围循环
    LoopStmt(std::string var, ExprPtr start, ExprPtr end, StmtPtr b, const SourceLocation& loc)
        : Stmt(Kind::Loop, loc),
          loop_kind(LoopKind::Range),
          iterator_var(std::move(var)),
          range_start(std::move(start)),
          range_end(std::move(end)),
          body(std::move(b)) {}
};

struct Parameter {
    std::string name;
    TypePtr type;  // 如果是self，type为nullptr
    bool is_self;
    bool is_mut_self;
    SourceLocation location;
    
    Parameter() : is_self(false), is_mut_self(false) {}
};

// Extern声明（放在Parameter之后）
struct ExternStmt : Stmt {
    std::string name;
    std::vector<Parameter> parameters;
    TypePtr return_type;
    
    ExternStmt(const std::string& n, std::vector<Parameter> params, 
               TypePtr ret_type, const SourceLocation& loc)
        : Stmt(Kind::Extern, loc), name(n), parameters(std::move(params)), 
          return_type(std::move(ret_type)) {}
};

struct GenericParam {
    std::string name;  // T, U等
    SourceLocation location;
};

struct FunctionStmt : Stmt {
    std::string name;
    std::vector<GenericParam> generic_params;  // 泛型参数 <T, U>
    std::vector<Parameter> parameters;
    TypePtr return_type;
    StmtPtr body;
    bool is_public;
    bool is_method;  // 第一个参数是self
    
    FunctionStmt(const std::string& n, std::vector<GenericParam> gen_params,
                 std::vector<Parameter> params, 
                 TypePtr ret_type, StmtPtr b, bool pub, const SourceLocation& loc)
        : Stmt(Kind::Function, loc), name(n), generic_params(std::move(gen_params)),
          parameters(std::move(params)), return_type(std::move(ret_type)), 
          body(std::move(b)), is_public(pub), is_method(false) {
        // 检查是否是方法（第一个参数是self）
        if (!parameters.empty() && parameters[0].is_self) {
            is_method = true;
        }
    }
};

// Struct字段或方法
struct StructMember {
    enum class Kind { Field, Method };
    Kind kind;
};

struct StructField : StructMember {
    std::string name;
    TypePtr type;
    SourceLocation location;
    
    StructField() { kind = Kind::Field; }
};

struct StructStmt : Stmt {
    std::string name;
    std::vector<GenericParam> generic_params;
    std::vector<StructField> fields;
    std::vector<std::unique_ptr<FunctionStmt>> methods;  // struct内的方法
    bool is_public;
    
    StructStmt(const std::string& n, std::vector<GenericParam> gen_params,
               std::vector<StructField> flds, 
               std::vector<std::unique_ptr<FunctionStmt>> meths,
               bool pub, const SourceLocation& loc)
        : Stmt(Kind::Struct, loc), name(n), generic_params(std::move(gen_params)),
          fields(std::move(flds)), methods(std::move(meths)), is_public(pub) {}
};

// Enum变体
struct EnumVariant {
    std::string name;
    std::vector<TypePtr> associated_types;  // 关联类型
    SourceLocation location;
};

struct EnumStmt : Stmt {
    std::string name;
    std::vector<GenericParam> generic_params;
    std::vector<EnumVariant> variants;
    bool is_public;
    
    EnumStmt(const std::string& n, std::vector<GenericParam> gen_params,
             std::vector<EnumVariant> vars, bool pub, const SourceLocation& loc)
        : Stmt(Kind::Enum, loc), name(n), generic_params(std::move(gen_params)),
          variants(std::move(vars)), is_public(pub) {}
};

// Type别名: type Result = enum { ... }
struct TypeAliasStmt : Stmt {
    std::string name;
    std::vector<GenericParam> generic_params;
    StmtPtr definition;  // StructStmt 或 EnumStmt
    
    TypeAliasStmt(const std::string& n, std::vector<GenericParam> gen_params,
                  StmtPtr def, const SourceLocation& loc)
        : Stmt(Kind::TypeAlias, loc), name(n), generic_params(std::move(gen_params)),
          definition(std::move(def)) {}
};

// Impl块（方法实现）
struct ImplStmt : Stmt {
    std::string type_name;
    std::vector<GenericParam> generic_params;
    std::vector<std::unique_ptr<FunctionStmt>> methods;
    
    ImplStmt(const std::string& tn, std::vector<GenericParam> gen_params,
             std::vector<std::unique_ptr<FunctionStmt>> meths, const SourceLocation& loc)
        : Stmt(Kind::Impl, loc), type_name(tn), generic_params(std::move(gen_params)),
          methods(std::move(meths)) {}
};

// ====== 程序 ======

struct Program {
    std::vector<StmtPtr> statements;
    std::vector<CompilerError> errors;
};

} // namespace pawc

#endif // PAWC_AST_H

