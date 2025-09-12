use std::fmt;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Ty {
    Int,
    Bool,
    String,
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
    Int(i64),
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
    Int(i64),
    Bool(bool),
    Str(String),
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
    pub ret: Ty,
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
            Ty::Int => write!(f, "Int"),
            Ty::Bool => write!(f, "Bool"),
            Ty::String => write!(f, "String"),
        }
    }
}
