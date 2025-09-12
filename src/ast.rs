use std::fmt;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Ty {
    Int,     // i32
    Long,    // i64
    Bool,    // 语义布尔（ABI 用 i8；在 codegen 处理 i8↔b1）
    String,  // 运行时字符串句柄/指针
    Double,  // f64
    Float,   // f32
    Char,    // Unicode 标量值：u32（ABI 以 i32 承载）
    Void,    // 仅用作函数返回
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnOp {
    Neg,
    Not,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
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

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Pattern {
    Int(i32),     // 原 i64 改为 i32
    Long(i64),
    Char(u32),
    Bool(bool),
    Wild, // `_`
}

#[derive(Clone, Debug)]
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

#[derive(Clone, Debug)]
pub enum ForStep {
    Assign { name: String, expr: Expr },
    Expr(Expr),
}

#[derive(Clone, Debug)]
pub struct Block {
    pub stmts: Vec<Stmt>,
    pub tail: Option<Box<Expr>>,
}

#[derive(Clone, Debug)]
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

#[derive(Clone, Debug)]
pub enum Expr {
    // —— 字面量 —— //
    Int(i32),       // 原 i64 改为 i32
    Long(i64),
    Float(f32),
    Double(f64),
    Char(u32),
    Bool(bool),
    Str(String),

    // —— 变量/运算 —— //
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

    /// match 表达式：仅允许 Int/Long/Char/Bool/_ 作为模式
    Match {
        scrut: Box<Expr>,
        arms: Vec<(Pattern, Block)>,
        default: Option<Block>,
    },

    Call {
        callee: String,
        args: Vec<Expr>,
    },

    Block(Block),
}

#[derive(Clone, Debug)]
pub struct FunDecl {
    pub name: String,
    pub params: Vec<(String, Ty)>,
    pub ret: Ty,   // 允许 Ty::Void；语义检查阶段保证返回规则
    pub body: Block,
    pub is_extern: bool,
}

#[derive(Clone, Debug)]
pub enum Item {
    Fun(FunDecl),
    Global {
        name: String,
        ty: Ty,
        init: Expr,
        is_const: bool,
    },
    Import(String),
}

#[derive(Clone, Debug)]
pub struct Program {
    pub items: Vec<Item>,
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
        }
    }
}
