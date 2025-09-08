use anyhow::{bail, Result};
use std::collections::HashMap;

use crate::ast::*;

#[derive(Clone, Debug)]
pub struct FnSig {
    pub params: Vec<Ty>,
    pub ret: Ty,
}

/// fns: 函数签名；globals: 顶层 let/const 的类型；scopes: 局部作用域栈
pub struct TyCk<'a> {
    fns: &'a HashMap<String, FnSig>,
    globals: HashMap<String, Ty>,
    scopes: Vec<HashMap<String, Ty>>,
}

impl<'a> TyCk<'a> {
    pub fn new(fns: &'a HashMap<String, FnSig>, globals: HashMap<String, Ty>) -> Self {
        Self { fns, globals, scopes: vec![HashMap::new()] }
    }

    fn push_scope(&mut self) { self.scopes.push(HashMap::new()); }
    fn pop_scope(&mut self)  { self.scopes.pop(); }

    fn define_local(&mut self, name: &str, ty: Ty) {
        self.scopes.last_mut().unwrap().insert(name.to_string(), ty);
    }

    /// 先查局部作用域，再查全局（关键）
    fn lookup_var(&self, name: &str) -> Option<Ty> {
        for s in self.scopes.iter().rev() {
            if let Some(t) = s.get(name) { return Some(t.clone()); }
        }
        self.globals.get(name).cloned()
    }

    pub fn check_fun(&mut self, f: &FunDecl) -> Result<()> {
        self.push_scope();
        for (n, t) in &f.params { self.define_local(n, t.clone()); }
        let body_ty = self.block(&f.body)?;
        self.ensure_ty(&body_ty, &f.ret, "function return type mismatch")?;
        self.pop_scope();
        Ok(())
    }

    pub fn block(&mut self, b: &Block) -> Result<Ty> {
        self.push_scope();
        for s in &b.stmts { self.stmt(s)?; }
        let ty = if let Some(tail) = &b.tail { self.expr(tail)? } else { Ty::Int };
        self.pop_scope();
        Ok(ty)
    }

    pub fn stmt(&mut self, s: &Stmt) -> Result<()> {
        match s {
            Stmt::Let { name, ty, init, .. } => {
                let t = self.expr(init)?;
                self.ensure_ty(&t, ty, &format!("let `{name}` type mismatch"))?;
                self.define_local(name, ty.clone());
                Ok(())
            }
            Stmt::Expr(e) => { let _ = self.expr(e)?; Ok(()) }
            Stmt::Return(opt) => { if let Some(e) = opt { let _ = self.expr(e)?; } Ok(()) }
            Stmt::While { cond, body } => {
                let tc = self.expr(cond)?;
                self.ensure_bool(&tc, "while condition must be Bool")?;
                let _ = self.block(body)?;
                Ok(())
            }
        }
    }

    pub fn expr(&mut self, e: &Expr) -> Result<Ty> {
        use BinOp::*;
        Ok(match e {
            Expr::Int(_)  => Ty::Int,
            Expr::Bool(_) => Ty::Bool,
            Expr::Var(n)  => self.lookup_var(n)
                .ok_or_else(|| anyhow::anyhow!(format!("unknown var `{n}`")))?,
            Expr::Unary { op, rhs } => {
                let t = self.expr(rhs)?;
                match op {
                    UnOp::Neg => { self.ensure_int(&t, "unary `-` expects Int")?; Ty::Int }
                    UnOp::Not => { self.ensure_bool(&t, "unary `!` expects Bool")?; Ty::Bool }
                }
            }
            Expr::Binary { op, lhs, rhs } => {
                let lt = self.expr(lhs)?; let rt = self.expr(rhs)?;
                match op {
                    Add|Sub|Mul|Div => { self.ensure_int(&lt,"arith expects Int")?;
                        self.ensure_int(&rt,"arith expects Int")?; Ty::Int }
                    Lt|Le|Gt|Ge    => { self.ensure_int(&lt,"cmp expects Int")?;
                        self.ensure_int(&rt,"cmp expects Int")?; Ty::Bool }
                    Eq|Ne          => { self.ensure_same(&lt,&rt,"==/!= require same types")?; Ty::Bool }
                    And|Or         => { self.ensure_bool(&lt,"&&/|| expect Bool")?;
                        self.ensure_bool(&rt,"&&/|| expect Bool")?; Ty::Bool }
                }
            }
            Expr::If { cond, then_b, else_b } => {
                let ct = self.expr(cond)?; self.ensure_bool(&ct, "`if` cond must be Bool")?;
                let tt = self.block(then_b)?; let et = self.block(else_b)?;
                self.ensure_same(&tt, &et, "`if` branches must have same type")?;
                tt
            }
            Expr::Call { callee, args } => {
                let sig = self.fns.get(callee)
                    .ok_or_else(|| anyhow::anyhow!(format!("unknown function `{callee}`")))?;
                if sig.params.len() != args.len() {
                    bail!("function `{callee}` expects {} args, got {}", sig.params.len(), args.len());
                }
                for (i, (pt, ae)) in sig.params.iter().zip(args.iter()).enumerate() {
                    let at = self.expr(ae)?;
                    self.ensure_ty(&at, pt, &format!("arg #{i} type mismatch in call `{callee}`"))?;
                }
                sig.ret.clone()
            }
            Expr::Block(b) => self.block(b)?,
        })
    }

    fn ensure_int(&self, t: &Ty, msg: &str) -> Result<()> {
        if matches!(t, Ty::Int) { Ok(()) } else { bail!("{msg}: got {t:?}") }
    }
    fn ensure_bool(&self, t: &Ty, msg: &str) -> Result<()> {
        if matches!(t, Ty::Bool){ Ok(()) } else { bail!("{msg}: got {t:?}") }
    }
    fn ensure_same(&self, a: &Ty, b: &Ty, msg: &str) -> Result<()> {
        if a==b { Ok(()) } else { bail!("{msg}: {a:?} vs {b:?}") }
    }
    fn ensure_ty(&self, got: &Ty, expect: &Ty, msg: &str) -> Result<()> {
        if got==expect { Ok(()) } else { bail!("{msg}: expect {expect:?}, got {got:?}") }
    }
}
