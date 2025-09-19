//! Paw AST 定义（保持和现有 parser/typecheck/codegen 完全兼容）
//! - 语义不变：字段/枚举名与原版一致
//! - 补充：更多的派生（Eq/Hash）、Display 实现与便捷构造方法

use std::fmt;

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
                    if i > 0 {
                        write!(f, ", ")?;
                    }
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
pub enum UnOp {
    Neg,
    Not,
}

impl fmt::Display for UnOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            UnOp::Neg => "-",
            UnOp::Not => "!",
        };
        f.write_str(s)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Lt,
    Le,
    Gt,
    Ge,
    Eq,
    Ne,
    And,
    Or,
}

impl fmt::Display for BinOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            BinOp::Add => "+",
            BinOp::Sub => "-",
            BinOp::Mul => "*",
            BinOp::Div => "/",
            BinOp::Lt  => "<",
            BinOp::Le  => "<=",
            BinOp::Gt  => ">",
            BinOp::Ge  => ">=",
            BinOp::Eq  => "==",
            BinOp::Ne  => "!=",
            BinOp::And => "&&",
            BinOp::Or  => "||",
        };
        f.write_str(s)
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
    },
    Assign {
        name: String,
        expr: Expr,
    },
    Expr(Expr),
}

#[derive(Clone, Debug, PartialEq)]
pub enum ForStep {
    Assign {
        name: String,
        expr: Expr,
    },
    Expr(Expr),
}

/* =========================
 *        语句与块
 * ========================= */

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Block {
    pub stmts: Vec<Stmt>,
    pub tail: Option<Box<Expr>>,
}

impl Block {
    #[inline]
    pub fn new() -> Self {
        Self {
            stmts: Vec::new(),
            tail: None,
        }
    }
    #[inline]
    pub fn with_tail(mut self, e: Expr) -> Self {
        self.tail = Some(Box::new(e));
        self
    }
    #[inline]
    pub fn push(&mut self, s: Stmt) {
        self.stmts.push(s);
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Stmt {
    Let {
        name: String,
        ty: Ty,
        init: Expr,
        is_const: bool,
    },
    Assign {
        name: String,
        expr: Expr,
    },
    While {
        cond: Expr,
        body: Block,
    },
    For {
        init: Option<ForInit>,
        cond: Option<Expr>,
        step: Option<ForStep>,
        body: Block,
    },
    Break,
    Continue,
    /// 语句版 if（可无 else；不产生值）
    If {
        cond: Expr,
        then_b: Block,
        else_b: Option<Block>,
    },
    Expr(Expr),
    Return(Option<Expr>),
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
    Int(i32),
    Long(i64),
    Float(f32),
    Double(f64),
    Char(u32),
    Bool(bool),
    Str(String),

    // 变量/运算
    Var(String),
    Unary {
        op: UnOp,
        rhs: Box<Expr>,
    },
    Binary {
        op: BinOp,
        lhs: Box<Expr>,
        rhs: Box<Expr>,
    },

    /// 表达式 if（必须有 else；产生值）
    If {
        cond: Box<Expr>,
        then_b: Block,
        else_b: Block,
    },

    /// match 表达式
    Match {
        scrut: Box<Expr>,
        arms: Vec<(Pattern, Block)>,
        default: Option<Block>,
    },

    /// 函数/方法调用（callee 支持限定名；generics 为显式类型实参）
    Call {
        callee: Callee,
        generics: Vec<Ty>,
        args: Vec<Expr>,
    },

    Block(Block),
}

impl Expr {
    #[inline]
    pub fn block(b: Block) -> Self {
        Expr::Block(b)
    }
    #[inline]
    pub fn var<S: Into<String>>(name: S) -> Self {
        Expr::Var(name.into())
    }
    #[inline]
    pub fn call_name<S: Into<String>>(name: S, args: Vec<Expr>) -> Self {
        Expr::Call {
            callee: Callee::Name(name.into()),
            generics: Vec::new(),
            args,
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
            callee: Callee::Qualified {
                trait_name: trait_name.into(),
                method: method.into(),
            },
            generics,
            args,
        }
    }
}

/* =========================
 *     trait / impl / where
 * ========================= */

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WherePred {
    pub ty: Ty,
    pub bounds: Vec<TraitRef>,
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
}

/// trait 支持多类型形参
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TraitDecl {
    pub name: String,
    pub type_params: Vec<String>, // 例如 ["T"] 或 ["A","B"]
    pub items: Vec<TraitMethodSig>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ImplMethod {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub body: Block,
}

/// impl 存整个 trait 的“实参列表”
/// 例：impl Eq<Int> { ... }  → trait_name="Eq", trait_args=[Int]
///     impl PairEq<Int,Long> → trait_args=[Int, Long]
#[derive(Clone, Debug, PartialEq)]
pub struct ImplDecl {
    pub trait_name: String,
    pub trait_args: Vec<Ty>,
    pub items: Vec<ImplMethod>,
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
}

#[derive(Clone, Debug, PartialEq)]
pub enum Item {
    Fun(FunDecl),
    Global {
        name: String,
        ty: Ty,
        init: Expr,
        is_const: bool,
    },
    Import(String),
    Trait(TraitDecl),
    Impl(ImplDecl),
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Program {
    pub items: Vec<Item>,
}
