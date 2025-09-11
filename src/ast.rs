use std::fmt;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Ty { Int, Bool, String }

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnOp { Neg, Not }

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BinOp { Add, Sub, Mul, Div, Lt, Le, Gt, Ge, Eq, Ne, And, Or }

#[derive(Clone, Debug)]
pub struct Block {
    pub stmts: Vec<Stmt>,
    pub tail: Option<Box<Expr>>,
}

#[derive(Clone, Debug)]
pub enum Stmt {
    // let/const 声明
    Let { name: String, ty: Ty, init: Expr, is_const: bool },

    Assign { name: String, expr: Expr },           // NEW
    Break,                                          // NEW
    Continue,                                       // NEW

    // 普通表达式语句
    Expr(Expr),

    // return [expr] ;
    Return(Option<Expr>),

    // while (cond) { body }
    While { cond: Expr, body: Block },
    For {                                           // NEW（仅做 parse 时承载，反糖后不再出现）
        init: Option<ForInit>,
        cond: Option<Expr>,
        step: Option<ForStep>,
        body: Block,
    },
}

#[derive(Clone, Debug)]
pub enum ForInit {                                  // NEW
    Let { name: String, ty: Ty, init: Expr, is_const: bool },
    Assign { name: String, expr: Expr },
    Expr(Expr),
}

#[derive(Clone, Debug)]
pub enum ForStep {                                  // NEW
    Assign { name: String, expr: Expr },
    Expr(Expr),
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
    Match {                                         // NEW（仅做 parse 时承载，反糖后不再出现）
        scrut: Box<Expr>,
        arms: Vec<(Pattern, Block)>,   // (字面量, block)
        default: Option<Block>,
    },
}

#[derive(Clone, Debug)]
pub enum Pattern {                                  // NEW
    Int(i64),
    Bool(bool),
    Wild, // 不会出现在 arms 里，这里仅为了将来扩展；当前 _ 进 default
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
