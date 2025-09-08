use std::collections::HashMap;
use anyhow::{bail, Result};

use crate::ast::*;

#[derive(Default)]
pub struct FnSig {
    pub params: Vec<Ty>,
    pub ret: Ty,
}

pub struct TyCk<'a> {
    fns: &'a HashMap<String, FnSig>,
    scopes: Vec<HashMap<String, Ty>>,
}

impl<'a> TyCk<'a> {
    pub fn new(fns: &'a HashMap<String, FnSig>) -> Self {
        Self { fns, scopes: vec![HashMap::new()] }
    }

    fn push(&mut self) { self.scopes.push(HashMap::new()); }
    fn pop(&mut self) { self.scopes.pop(); }
    fn insert(&mut self, n:&str, t:Ty) { self.scopes.last_mut().unwrap().insert(n.to_string(), t); }
    fn lookup(&self, n:&str) -> Option<Ty> {
        for m in self.scopes.iter().rev() {
            if let Some(t) = m.get(n) { return Some(t.clone()); }
        }
        None
    }

    pub fn check_fun(&mut self, f:&FunDecl) -> Result<()> {
        self.push();
        for (name, ty) in &f.params {
            self.insert(name, ty.clone());
        }
        let bt = self.block(&f.body)?;
        // 约定：块尾表达式类型必须与返回类型一致
        if bt != f.ret { bail!("function `{}` returns {:?}, but block tail has {:?}", f.name, f.ret, bt); }
        self.pop();
        Ok(())
    }

    fn block(&mut self, b:&Block) -> Result<Ty> {
        self.push();
        for s in &b.stmts {
            match s {
                Stmt::Let { name, ty, init, .. } => {
                    let et = self.expr(init)?;
                    if &et != ty { bail!("`let {name}: {:?}` but init has {:?}", ty, et); }
                    self.insert(name, ty.clone());
                }
                Stmt::Expr(e) => { let _ = self.expr(e)?; }
                Stmt::Return(opt) => {
                    // 对于 MVP：return 只允许出现在函数尾块；这里先允许并返回其类型
                    if let Some(e) = opt { let t = self.expr(e)?; self.pop(); return Ok(t); }
                    else { self.pop(); return Ok(Ty::Int); }
                }
                Stmt::While { cond, body } => {
                    let ct = self.expr(cond)?;
                    if ct != Ty::Bool { bail!("while condition must be Bool, got {:?}", ct); }
                    let _ = self.block(body)?;
                }
            }
        }
        let t = match &b.tail {
            Some(e) => self.expr(e)?,
            None => Ty::Int, // 约定无尾表达式的块值为 Int（0）
        };
        self.pop();
        Ok(t)
    }

    fn expr(&mut self, e:&Expr) -> Result<Ty> {
        use BinOp::*;
        Ok(match e {
            Expr::Int(_) => Ty::Int,
            Expr::Bool(_) => Ty::Bool,
            Expr::Var(n) => self.lookup(n).ok_or_else(|| anyhow::anyhow!("unknown var `{n}`"))?,
            Expr::Unary{ op, rhs } => {
                let rt = self.expr(rhs)?;
                match (op, rt) {
                    (UnOp::Neg, Ty::Int) => Ty::Int,
                    (UnOp::Not, Ty::Bool) => Ty::Bool,
                    _ => bail!("bad unary operand"),
                }
            }
            Expr::Binary{ op, lhs, rhs } => {
                let lt = self.expr(lhs)?; let rt = self.expr(rhs)?;
                match op {
                    Add|Sub|Mul|Div => { if lt==Ty::Int && rt==Ty::Int { Ty::Int } else { bail!("int binop needs Int") } }
                    Lt|Le|Gt|Ge     => { if lt==Ty::Int && rt==Ty::Int { Ty::Bool } else { bail!("cmp needs Int") } }
                    Eq|Ne           => { if lt==rt { Ty::Bool } else { bail!("==/!= need same types") } }
                    And|Or          => { if lt==Ty::Bool && rt==Ty::Bool { Ty::Bool } else { bail!("&&/|| need Bool") } }
                }
            }
            Expr::If{ cond, then_b, else_b } => {
                if self.expr(cond)? != Ty::Bool { bail!("if condition must be Bool"); }
                let t = self.block(then_b)?; let e = self.block(else_b)?;
                if t != e { bail!("if branches must have same type: {t:?} vs {e:?}") }
                t
            }
            Expr::Call{ callee, args } => {
                let sig = self.fns.get(callee).ok_or_else(|| anyhow::anyhow!("unknown fn `{callee}`"))?;
                if sig.params.len() != args.len() { bail!("fn `{callee}` arity mismatch"); }
                for (i,a) in args.iter().enumerate() {
                    let at = self.expr(a)?;
                    if at != sig.params[i] { bail!("fn `{callee}` arg#{i} type mismatch: expect {:?}, got {:?}", sig.params[i], at); }
                }
                sig.ret.clone()
            }
            Expr::Block(b) => self.block(b)?,
        })
    }
}
