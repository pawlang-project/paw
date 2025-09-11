use std::fmt;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Ty { Int, Bool, String }

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnOp { Neg, Not }

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BinOp { Add, Sub, Mul, Div, Lt, Le, Gt, Ge, Eq, Ne, And, Or }

#[derive(Clone, Debug)]
pub struct Block { pub stmts: Vec<Stmt>, pub tail: Option<Box<Expr>> }

#[derive(Clone, Debug)]
pub enum Stmt {
    Let { name: String, ty: Ty, init: Expr, is_const: bool },
    Expr(Expr),
    Return(Option<Expr>),
    While { cond: Expr, body: Block },
}

#[derive(Clone, Debug)]
pub enum Expr {
    Int(i64),
    Bool(bool),
    Str(String),
    Var(String),
    Unary { op: UnOp, rhs: Box<Expr> },
    Binary { op: BinOp, lhs: Box<Expr>, rhs: Box<Expr> },
    If { cond: Box<Expr>, then_b: Block, else_b: Block },
    Call { callee: String, args: Vec<Expr> },
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
    Global { name: String, ty: Ty, init: Expr, is_const: bool },
}

#[derive(Clone, Debug)]
pub struct Program { pub items: Vec<Item> }

impl fmt::Display for Ty {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self { Ty::Int=>write!(f,"Int"), Ty::Bool=>write!(f,"Bool"), Ty::String=>write!(f,"String") }
    }
}
