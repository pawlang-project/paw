//! Paw AST 定义（在保持语义/命名尽量一致的前提下，**为主要节点增加 span**）
//! - 语义不变：字段/枚举名与原版一致（仅新增 `span: Span`）
//! - 派生增强：Eq/Hash/Display/便捷构造
//! - 新增：显式类型转换表达式 `Expr::Cast { expr, ty, span }`（已包含）
//! - 迁移建议：原有 `match` 如 `Stmt::Let { name, ty, init, is_const }` 请改为
//!   `Stmt::Let { name, ty, init, is_const, .. }` 以忽略新增的 `span`

use std::fmt;
use crate::frontend::span::Span;

/* =========================
 *        类型系统
 * ========================= */

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Ty {
    Int,
    Long,
    Byte,
    Bool,
    String,
    Double,
    Float,
    Char,
    Void,
    /// 类型变量（如 T / U）
    Var(String),
    /// 类型应用（如 Vec<T> / Map<String, Int>）
    App { name: String, args: Vec<Ty> },
}

impl fmt::Display for Ty {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Ty::Int => write!(f, "Int"),
            Ty::Long => write!(f, "Long"),
            Ty::Byte => write!(f, "Byte"),
            Ty::Bool => write!(f, "Bool"),
            Ty::String => write!(f, "String"),
            Ty::Double => write!(f, "Double"),
            Ty::Float => write!(f, "Float"),
            Ty::Char => write!(f, "Char"),
            Ty::Void => write!(f, "Void"),
            Ty::Var(v) => write!(f, "{v}"),
            Ty::App { name, args } => {
                write!(f, "{name}<")?;
                for (i, a) in args.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "{a}")?;
                }
                write!(f, ">")
            }
        }
    }
}

impl Ty {
    #[inline]
    pub fn is_intish(&self) -> bool {
        matches!(self, Ty::Int | Ty::Long | Ty::Byte | Ty::Char)
    }
    #[inline]
    pub fn is_floatish(&self) -> bool {
        matches!(self, Ty::Float | Ty::Double)
    }
}

/* =========================
 *     一元/二元运算符
 * ========================= */

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum UnOp { Neg, Not }

impl fmt::Display for UnOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self { UnOp::Neg => "-", UnOp::Not => "!" })
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum BinOp {
    Add, Sub, Mul, Div,
    Lt, Le, Gt, Ge, Eq, Ne,
    And, Or,
}

impl fmt::Display for BinOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            BinOp::Add => "+",  BinOp::Sub => "-",  BinOp::Mul => "*",  BinOp::Div => "/",
            BinOp::Lt  => "<",  BinOp::Le  => "<=", BinOp::Gt  => ">",  BinOp::Ge  => ">=",
            BinOp::Eq  => "==", BinOp::Ne  => "!=", BinOp::And => "&&", BinOp::Or  => "||",
        })
    }
}

/* =========================
 *          模式
 * ========================= */

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Pattern {
    Int(i32),
    Long(i64),
    Char(u32),
    Bool(bool),
    Wild,
}

/* =========================
 *      for 初始化/步进
 * ========================= */

#[derive(Clone, Debug, PartialEq)]
pub enum ForInit {
    Let {
        name: String,
        ty: Ty,
        init: Expr,
        is_const: bool,
        span: Span,
    },
    Assign {
        name: String,
        expr: Expr,
        span: Span,
    },
    Expr(Expr, /* span= */ Span),
}

#[derive(Clone, Debug, PartialEq)]
pub enum ForStep {
    Assign {
        name: String,
        expr: Expr,
        span: Span,
    },
    Expr(Expr, /* span= */ Span),
}

/* =========================
 *        语句与块
 * ========================= */

#[derive(Clone, Debug, PartialEq)]
pub struct Block {
    pub stmts: Vec<Stmt>,
    pub tail: Option<Box<Expr>>,
    pub span: Span,
}

impl Default for Block {
    fn default() -> Self { Self { stmts: Vec::new(), tail: None, span: Span::DUMMY } }
}

impl Block {
    #[inline] pub fn new() -> Self { Self::default() }
    #[inline] pub fn with_tail(mut self, e: Expr) -> Self { self.tail = Some(Box::new(e)); self }
    #[inline] pub fn with_span(mut self, span: Span) -> Self { self.span = span; self }
    #[inline] pub fn push(&mut self, s: Stmt) { self.stmts.push(s); }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Stmt {
    Let {
        name: String,
        ty: Ty,
        init: Expr,
        is_const: bool,
        span: Span,
    },
    Assign {
        name: String,
        expr: Expr,
        span: Span,
    },
    While {
        cond: Expr,
        body: Block,
        span: Span,
    },
    For {
        init: Option<ForInit>,
        cond: Option<Expr>,
        step: Option<ForStep>,
        body: Block,
        span: Span,
    },
    Break { span: Span },
    Continue { span: Span },
    /// 语句版 if（可无 else；不产生值）
    If {
        cond: Expr,
        then_b: Block,
        else_b: Option<Block>,
        span: Span,
    },
    Expr {
        expr: Expr,
        span: Span,
    },
    Return {
        expr: Option<Expr>,
        span: Span,
    },
}

/* =========================
 *        调用目标
 * ========================= */

/// - `Name("add")`                    —— 普通自由函数
/// - `Qualified { "Eq", "eq" }`       —— 限定名：Trait::method
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Callee {
    Name(String),
    Qualified { trait_name: String, method: String },
}

impl fmt::Display for Callee {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Callee::Name(n) => write!(f, "{n}"),
            Callee::Qualified { trait_name, method } => write!(f, "{trait_name}::{method}"),
        }
    }
}

/* =========================
 *          表达式
 * ========================= */

#[derive(Clone, Debug, PartialEq)]
pub enum Expr {
    // 字面量
    Int   { value: i32,  span: Span },
    Long  { value: i64,  span: Span },
    Float { value: f32,  span: Span },
    Double{ value: f64,  span: Span },
    Char  { value: u32,  span: Span },
    Bool  { value: bool, span: Span },
    Str   { value: String, span: Span },

    // 变量/运算
    Var {
        name: String,
        span: Span,
    },
    Unary {
        op: UnOp,
        rhs: Box<Expr>,
        span: Span,
    },
    Binary {
        op: BinOp,
        lhs: Box<Expr>,
        rhs: Box<Expr>,
        span: Span,
    },

    /// 显式类型转换（支持链式）：`expr as Ty`
    Cast {
        expr: Box<Expr>,
        ty: Ty,
        span: Span,
    },

    /// 表达式 if（必须有 else；产生值）
    If {
        cond: Box<Expr>,
        then_b: Block,
        else_b: Block,
        span: Span,
    },

    /// match 表达式
    Match {
        scrut: Box<Expr>,
        arms: Vec<(Pattern, Block)>,
        default: Option<Block>,
        span: Span,
    },

    /// 函数/方法调用（callee 支持限定名；generics 为显式类型实参）
    Call {
        callee: Callee,
        generics: Vec<Ty>,
        args: Vec<Expr>,
        span: Span,
    },

    Block {
        block: Block,
        span: Span,
    },
}

impl Expr {
    #[inline]
    pub fn block(b: Block) -> Self {
        Expr::Block { block: b, span: Span::DUMMY }
    }
    #[inline]
    pub fn var<S: Into<String>>(name: S) -> Self {
        Expr::Var { name: name.into(), span: Span::DUMMY }
    }
    #[inline]
    pub fn call_name<S: Into<String>>(name: S, args: Vec<Expr>) -> Self {
        Expr::Call {
            callee: Callee::Name(name.into()),
            generics: Vec::new(),
            args,
            span: Span::DUMMY,
        }
    }
    #[inline]
    pub fn call_qualified<T: Into<String>, M: Into<String>>(
        trait_name: T,
        method: M,
        generics: Vec<Ty>,
        args: Vec<Expr>,
    ) -> Self {
        Expr::Call {
            callee: Callee::Qualified { trait_name: trait_name.into(), method: method.into() },
            generics,
            args,
            span: Span::DUMMY,
        }
    }
    #[inline]
    pub fn cast(expr: Expr, ty: Ty) -> Self {
        Expr::Cast { expr: Box::new(expr), ty, span: Span::DUMMY }
    }
}

/* =========================
 *     trait / impl / where
 * ========================= */

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WherePred {
    pub ty: Ty,
    pub bounds: Vec<TraitRef>,
    pub span: Span,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct TraitRef {
    pub name: String,
    pub args: Vec<Ty>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TraitMethodSig {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub span: Span,
}

/// trait 支持多类型形参
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TraitDecl {
    pub name: String,
    pub type_params: Vec<String>, // 例如 ["T"] 或 ["A","B"]
    pub items: Vec<TraitMethodSig>,
    pub span: Span,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ImplMethod {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub body: Block,
    pub span: Span,
}

/// impl 存整个 trait 的“实参列表”
/// 例：impl Eq<Int> { ... }  → trait_name="Eq", trait_args=[Int]
///     impl PairEq<Int,Long> → trait_args=[Int, Long]
#[derive(Clone, Debug, PartialEq)]
pub struct ImplDecl {
    pub trait_name: String,
    pub trait_args: Vec<Ty>,
    pub items: Vec<ImplMethod>,
    pub span: Span,
}

/* =========================
 *     函数 / 项 / 程序
 * ========================= */

#[derive(Clone, Debug, PartialEq)]
pub struct FunDecl {
    pub name: String,
    pub type_params: Vec<String>, // 函数类型形参
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub where_bounds: Vec<WherePred>, // where 子句
    pub body: Block,
    pub is_extern: bool,
    pub span: Span,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Item {
    Fun(FunDecl, /* span= */ Span),
    Global {
        name: String,
        ty: Ty,
        init: Expr,
        is_const: bool,
        span: Span,
    },
    Import(String, /* span= */ Span),
    Trait(TraitDecl, /* span= */ Span),
    Impl(ImplDecl, /* span= */ Span),
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Program {
    pub items: Vec<Item>,
}
