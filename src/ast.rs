use std::fmt;


/// ---- 类型系统 ----
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Ty {
    Int, Long, Bool, String, Double, Float, Char, Void,
    /// 类型变量（如 T / U）
    Var(String),
    /// 类型应用（如 Vec<T> / Map<String, Int>）
    App { name: String, args: Vec<Ty> },
}

impl fmt::Display for Ty {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Ty::Int    => write!(f, "Int"),
            Ty::Long   => write!(f, "Long"),
            Ty::Bool   => write!(f, "Bool"),
            Ty::String => write!(f, "String"),
            Ty::Double => write!(f, "Double"),
            Ty::Float  => write!(f, "Float"),
            Ty::Char   => write!(f, "Char"),
            Ty::Void   => write!(f, "Void"),
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

/// ---- 一元/二元运算 ----
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnOp { Neg, Not }

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BinOp { Add, Sub, Mul, Div, Lt, Le, Gt, Ge, Eq, Ne, And, Or }

/// ---- 模式（match） ----
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Pattern { Int(i32), Long(i64), Char(u32), Bool(bool), Wild }

/// ---- for 初始化/步进 ----
#[derive(Clone, Debug)]
pub enum ForInit {
    Let { name: String, ty: Ty, init: Expr, is_const: bool },
    Assign { name: String, expr: Expr },
    Expr(Expr),
}
#[derive(Clone, Debug)]
pub enum ForStep {
    Assign { name: String, expr: Expr },
    Expr(Expr),
}

/// ---- 语句/块 ----
#[derive(Clone, Debug)]
pub struct Block { pub stmts: Vec<Stmt>, pub tail: Option<Box<Expr>> }

#[derive(Clone, Debug)]
pub enum Stmt {
    Let { name: String, ty: Ty, init: Expr, is_const: bool },
    Assign { name: String, expr: Expr },
    While { cond: Expr, body: Block },
    For { init: Option<ForInit>, cond: Option<Expr>, step: Option<ForStep>, body: Block },
    Break, Continue,
    /// 语句版 if（可无 else；不产生值）
    If { cond: Expr, then_b: Block, else_b: Option<Block> },
    Expr(Expr),
    Return(Option<Expr>),
}

/// ---- 调用目标（新增） ----
/// - Name("add")                —— 普通自由函数
/// - Qualified { "Eq", "eq" }   —— 限定名：Trait::method
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

/// ---- 表达式 ----
#[derive(Clone, Debug)]
pub enum Expr {
    // 字面量
    Int(i32), Long(i64), Float(f32), Double(f64), Char(u32), Bool(bool), Str(String),
    // 变量/运算
    Var(String),
    Unary { op: UnOp, rhs: Box<Expr> },
    Binary { op: BinOp, lhs: Box<Expr>, rhs: Box<Expr> },
    /// 表达式 if（必须有 else；产生值）
    If { cond: Box<Expr>, then_b: Block, else_b: Block },
    /// match 表达式
    Match { scrut: Box<Expr>, arms: Vec<(Pattern, Block)>, default: Option<Block> },

    /// 函数/方法调用（修改：callee 支持限定名；generics 为显式类型实参）
    Call { callee: Callee, generics: Vec<Ty>, args: Vec<Expr> },

    Block(Block),
}

/// ---- trait/impl/where ----
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WherePred { pub ty: Ty, pub bounds: Vec<TraitRef> }

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct TraitRef { pub name: String, pub args: Vec<Ty> }

#[derive(Clone, Debug)]
pub struct TraitMethodSig {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
}

/// 修改：trait 支持多类型形参
#[derive(Clone, Debug)]
pub struct TraitDecl {
    pub name: String,
    pub type_params: Vec<String>,      // 例如 ["T"] 或 ["A","B"]
    pub items: Vec<TraitMethodSig>,
}

#[derive(Clone, Debug)]
pub struct ImplMethod {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub body: Block,
}

/// 修改：impl 存整个 trait 的“实参列表”
/// 例：impl Eq<Int> { ... }  → trait_name="Eq", trait_args=[Int]
///     impl PairEq<Int,Long> → trait_args=[Int, Long]
#[derive(Clone, Debug)]
pub struct ImplDecl {
    pub trait_name: String,
    pub trait_args: Vec<Ty>,
    pub items: Vec<ImplMethod>,
}

/// ---- 函数/项/程序 ----
#[derive(Clone, Debug)]
pub struct FunDecl {
    pub name: String,
    pub type_params: Vec<String>,     // 函数类型形参
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub where_bounds: Vec<WherePred>, // where 子句
    pub body: Block,
    pub is_extern: bool,
}

#[derive(Clone, Debug)]
pub enum Item {
    Fun(FunDecl),
    Global { name: String, ty: Ty, init: Expr, is_const: bool },
    Import(String),
    Trait(TraitDecl),
    Impl(ImplDecl),
}

#[derive(Clone, Debug)]
pub struct Program { pub items: Vec<Item> }
