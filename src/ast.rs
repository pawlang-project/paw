use std::fmt;

/// ---- 类型系统 ----
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Ty {
    Int,     // i32
    Long,    // i64
    Bool,    // 语义布尔（ABI 用 i8；在 codegen 处理 i8↔b1）
    String,  // 运行时字符串句柄/指针
    Double,  // f64
    Float,   // f32
    Char,    // Unicode 标量值：u32（ABI 以 i32 承载）
    Void,    // 仅用作函数返回

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
pub enum BinOp {
    Add, Sub, Mul, Div,
    Lt, Le, Gt, Ge, Eq, Ne,
    And, Or,
}

/// ---- 模式（match） ----
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Pattern {
    Int(i32),     // 与 grammar 一致：Int 改为 i32
    Long(i64),
    Char(u32),
    Bool(bool),
    Wild,         // `_`
}

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
pub struct Block {
    pub stmts: Vec<Stmt>,
    pub tail: Option<Box<Expr>>,
}

#[derive(Clone, Debug)]
pub enum Stmt {
    Let { name: String, ty: Ty, init: Expr, is_const: bool },
    Assign { name: String, expr: Expr },
    While { cond: Expr, body: Block },
    For { init: Option<ForInit>, cond: Option<Expr>, step: Option<ForStep>, body: Block },
    Break,
    Continue,
    /// 语句版 if（可无 else；不产生值）
    If { cond: Expr, then_b: Block, else_b: Option<Block> },
    Expr(Expr),
    Return(Option<Expr>),
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

    /// match 表达式：仅允许 Int/Long/Char/Bool/_ 作为模式
    Match { scrut: Box<Expr>, arms: Vec<(Pattern, Block)>, default: Option<Block> },

    /// 函数调用
    /// - callee：目前语法为标识符调用，保持 String；将来若支持一等函数可改为 Box<Expr>
    /// - generics：调用处的类型实参（来自 `<...>`；省略时为空）
    Call { callee: String, generics: Vec<Ty>, args: Vec<Expr> },

    Block(Block),
}

/// ---- trait/impl/where 支持 ----

/// where 子句中的单个谓词：`<ty> : Bound1 + Bound2 + ...`
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WherePred {
    pub ty: Ty,
    pub bounds: Vec<TraitRef>,
}

/// 约束引用（trait 名 + 可选类型实参）
/// 例：`Eq`、`Iterator<Item=T>`（当前语法是位置参数，如 `Iter<T>`）
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct TraitRef {
    pub name: String,
    pub args: Vec<Ty>, // 语法上允许 0 或多参；常见一元：Eq<T>/Ord<T>
}

/// trait 中的方法签名（无函数体）
#[derive(Clone, Debug)]
pub struct TraitMethodSig {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
}

/// 顶层 trait 声明（目前语法：一元 `<T>`，此处保留成单形参名；未来可扩展成 Vec<String>）
#[derive(Clone, Debug)]
pub struct TraitDecl {
    pub name: String,
    pub type_param: String,          // 如 "T"
    pub items: Vec<TraitMethodSig>,  // 方法签名列表
}

/// impl 中的方法（有函数体）
#[derive(Clone, Debug)]
pub struct ImplMethod {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,
    pub body: Block,
}

/// 顶层 impl（为具体类型实现某 trait）
#[derive(Clone, Debug)]
pub struct ImplDecl {
    pub trait_name: String,  // 例："Eq"
    pub for_ty: Ty,          // 例：Ty::Int 或 Ty::App { name:"Vec", args:[Ty::Var("T")] }
    pub items: Vec<ImplMethod>,
}

/// ---- 函数/项/程序 ----
#[derive(Clone, Debug)]
pub struct FunDecl {
    pub name: String,
    pub type_params: Vec<String>,     // 新增：函数类型形参，如 ["T","U"]
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,                      // 允许 Ty::Void；语义检查阶段保证返回规则
    pub where_bounds: Vec<WherePred>, // 新增：where 子句
    pub body: Block,
    pub is_extern: bool,
}

#[derive(Clone, Debug)]
pub enum Item {
    Fun(FunDecl),
    Global { name: String, ty: Ty, init: Expr, is_const: bool },
    Import(String),

    // 新增：trait/impl 顶层项
    Trait(TraitDecl),
    Impl(ImplDecl),
}

#[derive(Clone, Debug)]
pub struct Program {
    pub items: Vec<Item>,
}
