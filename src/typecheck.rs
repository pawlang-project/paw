use anyhow::{anyhow, Result};
use std::collections::HashMap;
use crate::ast::*;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FnSig { pub params: Vec<Ty>, pub ret: Ty }

pub fn typecheck_program(p: &Program) -> Result<(HashMap<String, FnSig>, HashMap<String, Ty>)> {
    let mut fnsig = HashMap::<String, FnSig>::new();
    for it in &p.items {
        if let Item::Fun(f) = it {
            if fnsig.contains_key(&f.name) { return Err(anyhow!("duplicate function `{}`", f.name)); }
            fnsig.insert(f.name.clone(), FnSig { params: f.params.iter().map(|(_,t)|t.clone()).collect(), ret: f.ret.clone() });
        }
    }
    let mut globals = HashMap::<String, Ty>::new();
    for it in &p.items {
        if let Item::Global { name, ty, .. } = it {
            if globals.contains_key(name) { return Err(anyhow!("duplicate global `{}`", name)); }
            globals.insert(name.clone(), ty.clone());
        }
    }
    let mut ck = TyCk::new(&fnsig, globals.clone());
    for it in &p.items {
        if let Item::Global { name, ty, init, .. } = it {
            let t = ck.expr(init)?;
            ck.require_assignable(&t, ty).map_err(|e|anyhow!("global `{}` init: {}", name, e))?;
        }
    }
    for it in &p.items {
        if let Item::Fun(f) = it {
            if !f.is_extern { ck.check_fun(f)?; }
        }
    }
    Ok((fnsig, globals))
}

pub struct TyCk<'a> {
    fnsig: &'a HashMap<String, FnSig>,
    globals: HashMap<String, Ty>,
    scopes: Vec<HashMap<String, Ty>>,
    current_ret: Option<Ty>,
}

impl<'a> TyCk<'a> {
    fn new(fnsig: &'a HashMap<String, FnSig>, globals: HashMap<String, Ty>) -> Self {
        Self { fnsig, globals, scopes: vec![], current_ret: None }
    }
    fn push(&mut self){ self.scopes.push(HashMap::new()); }
    fn pop(&mut self){ self.scopes.pop(); }
    fn lookup(&self, n:&str)->Option<Ty>{
        for s in self.scopes.iter().rev(){ if let Some(t)=s.get(n){ return Some(t.clone()); } }
        self.globals.get(n).cloned()
    }
    fn require_assignable(&self, src:&Ty, dst:&Ty)->Result<()>{
        if src==dst { Ok(()) } else { Err(anyhow!("expect `{}`, got `{}`", dst, src)) }
    }

    fn check_fun(&mut self, f:&FunDecl)->Result<()>{
        self.current_ret = Some(f.ret.clone());
        self.push();
        for (n,t) in &f.params {
            if self.scopes.last().unwrap().contains_key(n) { return Err(anyhow!("dup param `{}`", n)); }
            self.scopes.last_mut().unwrap().insert(n.clone(), t.clone());
        }
        let _ = self.block(&f.body)?;
        self.pop();
        self.current_ret = None;
        Ok(())
    }

    fn block(&mut self, b:&Block)->Result<Ty>{
        self.push();
        for s in &b.stmts { self.stmt(s)?; }
        let ty = if let Some(e)=&b.tail { self.expr(e)? } else { Ty::Int };
        self.pop();
        Ok(ty)
    }

    fn stmt(&mut self, s:&Stmt)->Result<()>{
        match s {
            Stmt::Let { name, ty, init, .. } => {
                let t = self.expr(init)?;
                self.require_assignable(&t, ty).map_err(|e|anyhow!("let `{}`: {}", name, e))?;
                if self.scopes.last().unwrap().contains_key(name) { return Err(anyhow!("dup local `{}`", name)); }
                self.scopes.last_mut().unwrap().insert(name.clone(), ty.clone());
                Ok(())
            }
            Stmt::Expr(e) => { let _=self.expr(e)?; Ok(()) }
            Stmt::Return(opt) => {
                let ret_ty = self
                    .current_ret
                    .clone()
                    .ok_or_else(|| anyhow!("return outside function"))?;

                if let Some(e) = opt {
                    let t = self.expr(e)?;
                    self.require_assignable(&t, &ret_ty)?;
                }
                Ok(())
            }
            Stmt::While{cond, body} => {
                let t = self.expr(cond)?; self.require_assignable(&t, &Ty::Bool).map_err(|e|anyhow!("while cond: {}", e))?;
                let _ = self.block(body)?; Ok(())
            }
        }
    }

    fn expr(&mut self, e:&Expr)->Result<Ty>{
        use BinOp::*; use UnOp::*;
        Ok(match e {
            Expr::Int(_)=>Ty::Int, Expr::Bool(_)=>Ty::Bool, Expr::Str(_)=>Ty::String,
            Expr::Var(n)=> self.lookup(n).ok_or_else(||anyhow!("unknown var `{n}`"))?,
            Expr::Unary{op,rhs} => {
                let t=self.expr(rhs)?;
                match op { Neg=>{ self.require_assignable(&t,&Ty::Int)?; Ty::Int },
                    Not=>{ self.require_assignable(&t,&Ty::Bool)?; Ty::Bool } }
            }
            Expr::Binary{op,lhs,rhs}=>{
                let lt=self.expr(lhs)?; let rt=self.expr(rhs)?;
                match op {
                    Add|Sub|Mul|Div => { self.require_assignable(&lt,&Ty::Int)?; self.require_assignable(&rt,&Ty::Int)?; Ty::Int }
                    Lt|Le|Gt|Ge|Eq|Ne => { if lt!=rt { return Err(anyhow!("comparison requires same types")); } Ty::Bool }
                    And|Or => { self.require_assignable(&lt,&Ty::Bool)?; self.require_assignable(&rt,&Ty::Bool)?; Ty::Bool }
                }
            }
            Expr::If{cond,then_b,else_b}=>{
                let ct=self.expr(cond)?; self.require_assignable(&ct,&Ty::Bool).map_err(|e|anyhow!("if cond: {}", e))?;
                let tt=self.block(then_b)?; let et=self.block(else_b)?;
                if tt!=et { return Err(anyhow!("if branches type mismatch: then `{}`, else `{}`", tt, et)); }
                tt
            }
            Expr::Call{callee,args}=>{
                let sig=self.fnsig.get(callee).ok_or_else(||anyhow!("unknown function `{callee}`"))?;
                if sig.params.len()!=args.len(){ return Err(anyhow!("function `{}` expects {} args, got {}", callee, sig.params.len(), args.len())); }
                for (i,(a,pt)) in args.iter().zip(sig.params.iter()).enumerate() {
                    let at=self.expr(a)?; self.require_assignable(&at, pt).map_err(|e|anyhow!("arg#{i}: {e}"))?;
                }
                sig.ret.clone()
            }
            Expr::Block(b)=> self.block(b)?,
        })
    }
}
