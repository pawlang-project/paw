use anyhow::{anyhow, bail, Result};
use std::collections::{HashMap, HashSet};
use crate::ast::*;

/* ========= 多态函数签名 ========= */
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Scheme {
    pub tparams: Vec<String>,
    pub params: Vec<Ty>,
    pub ret: Ty,
    pub where_bounds: Vec<WherePred>,
}

/* ========= trait / impl 环境（用于 where 校验） ========= */
#[derive(Default, Clone)]
pub struct TraitEnv {
    pub decls: HashMap<String, TraitDecl>, // 名 -> 声明（包含形参个数/方法原型）
}

#[derive(Default, Clone)]
pub struct ImplEnv {
    // key: (trait_name, "ArgKey1,ArgKey2,...")
    map: HashMap<(String, String), bool>,
}

fn key_of_ty(t: &Ty) -> String {
    match t {
        Ty::App { name, args } => format!("{}<{}>", name, args.iter().map(key_of_ty).collect::<Vec<_>>().join(",")),
        Ty::Var(v) => format!("Var({})", v),
        _ => format!("{}", t),
    }
}

fn trait_inst_key(tr: &str, args: &[Ty]) -> (String, String) {
    (tr.to_string(), args.iter().map(key_of_ty).collect::<Vec<_>>().join(","))
}

impl ImplEnv {
    fn insert(&mut self, tr: &str, args: &[Ty]) {
        self.map.insert(trait_inst_key(tr, args), true);
    }
    fn has_impl(&self, tr: &str, args: &[Ty]) -> bool {
        self.map.contains_key(&trait_inst_key(tr, args))
    }
}

/* ========= 对外入口：返回 (函数方案, 全局类型, TraitEnv, ImplEnv) ========= */
pub fn typecheck_program(
    p: &Program
) -> Result<(HashMap<String, Scheme>, HashMap<String, Ty>, TraitEnv, ImplEnv)> {
    // 0) 收集 trait / impl
    let mut tenv = TraitEnv::default();
    let mut ienv = ImplEnv::default();

    for it in &p.items {
        match it {
            Item::Trait(td) => {
                if tenv.decls.contains_key(&td.name) {
                    bail!("duplicate trait `{}`", td.name);
                }
                tenv.decls.insert(td.name.clone(), td.clone());
            }
            Item::Impl(id) => {
                // impl 的实参必须全为具体类型
                for ta in &id.trait_args { ensure_no_free_tyvar(ta)?; }
                ienv.insert(&id.trait_name, &id.trait_args);
            }
            _ => {}
        }
    }

    // 1) 收集函数方案（FunDecl + impl 方法映射为普通函数）
    let mut fnscheme = HashMap::<String, Scheme>::new();

    // 普通/extern 函数
    for it in &p.items {
        if let Item::Fun(f) = it {
            if fnscheme.contains_key(&f.name) {
                return Err(anyhow!("duplicate function `{}`", f.name));
            }
            fnscheme.insert(
                f.name.clone(),
                Scheme {
                    tparams: f.type_params.clone(),
                    params: f.params.iter().map(|(_, t)| t.clone()).collect(),
                    ret: f.ret.clone(),
                    where_bounds: f.where_bounds.clone(),
                },
            );
        }
    }
    // impl 方法 -> 具体符号
    for it in &p.items {
        if let Item::Impl(id) = it {
            for m in &id.items {
                let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);
                if fnscheme.contains_key(&sym) {
                    bail!("duplicate impl method symbol `{}`", sym);
                }
                // 方法在 impl 中已是具体类型（不再包含 trait 的形参）
                for (_, t) in &m.params { ensure_no_free_tyvar(t)?; }
                ensure_no_free_tyvar(&m.ret)?;
                fnscheme.insert(sym.clone(), Scheme {
                    tparams: vec![],
                    params: m.params.iter().map(|(_, t)| t.clone()).collect(),
                    ret: m.ret.clone(),
                    where_bounds: vec![],
                });
            }
        }
    }

    // 2) 收集全局变量
    let mut globals = HashMap::<String, Ty>::new();
    let mut globals_const = HashSet::<String>::new();
    for it in &p.items {
        if let Item::Global { name, ty, is_const, .. } = it {
            if globals.contains_key(name) {
                return Err(anyhow!("duplicate global `{}`", name));
            }
            ensure_no_free_tyvar(ty)
                .map_err(|e| anyhow!("global `{}`: {}", name, e))?;
            globals.insert(name.clone(), ty.clone());
            if *is_const { globals_const.insert(name.clone()); }
        }
    }

    // 3) 检查全局初始化
    let mut ck = TyCk::new(&fnscheme, &tenv, &ienv, globals.clone(), globals_const.clone());
    for it in &p.items {
        if let Item::Global { name, ty, init, .. } = it {
            let t = ck.expr(init)?;
            ck.require_assignable(&t, ty)
                .map_err(|e| anyhow!("global `{}` init: {}", name, e))?;
        }
    }

    // 4) 检查函数体（忽略 extern）
    for it in &p.items {
        if let Item::Fun(f) = it {
            if !f.is_extern {
                ck.check_fun(f)?;
            }
        }
    }
    // impl 方法体：按普通函数检查一遍
    for it in &p.items {
        if let Item::Impl(id) = it {
            for m in &id.items {
                let fdecl = FunDecl {
                    name: mangle_impl_method(&id.trait_name, &id.trait_args, &m.name),
                    type_params: vec![],
                    params: m.params.clone(),
                    ret: m.ret.clone(),
                    where_bounds: vec![],
                    body: m.body.clone(),
                    is_extern: false,
                };
                ck.check_fun(&fdecl)?;
            }
        }
    }

    Ok((fnscheme, globals, tenv, ienv))
}

/* ====== 辅助：名搅拌（与 codegen 对应） ====== */
fn mangle_ty(t: &Ty) -> String {
    match t {
        Ty::Int    => "Int".into(),
        Ty::Long   => "Long".into(),
        Ty::Bool   => "Bool".into(),
        Ty::String => "String".into(),
        Ty::Double => "Double".into(),
        Ty::Float  => "Float".into(),
        Ty::Char   => "Char".into(),
        Ty::Void   => "Void".into(),
        Ty::Var(n) => format!("Var({})", n),
        Ty::App { name, args } => {
            if args.is_empty() { name.clone() }
            else {
                let parts: Vec<String> = args.iter().map(mangle_ty).collect();
                format!("{}<{}>", name, parts.join(","))
            }
        }
    }
}
fn mangle_impl_method(trait_name: &str, trait_args: &[Ty], method: &str) -> String {
    if trait_args.is_empty() {
        format!("__impl_{}__{}", trait_name, method)
    } else {
        let parts: Vec<String> = trait_args.iter().map(mangle_ty).collect();
        format!("__impl_{}${}__{}", trait_name, parts.join(","), method)
    }
}

/* ====== 代换 / 统一 ====== */
type Subst = HashMap<String, Ty>;

fn apply_subst(ty: &Ty, s: &Subst) -> Ty {
    match ty {
        Ty::Var(v) => s.get(v).cloned().unwrap_or_else(|| Ty::Var(v.clone())),
        Ty::App { name, args } => Ty::App {
            name: name.clone(),
            args: args.iter().map(|t| apply_subst(t, s)).collect(),
        },
        _ => ty.clone(),
    }
}

fn occurs(v: &str, ty: &Ty) -> bool {
    match ty {
        Ty::Var(v2) => v == v2,
        Ty::App { args, .. } => args.iter().any(|t| occurs(v, t)),
        _ => false,
    }
}

fn unify(lhs: &Ty, rhs: &Ty, s: &mut Subst) -> Result<()> {
    use Ty::*;
    let l = apply_subst(lhs, s);
    let r = apply_subst(rhs, s);
    match (&l, &r) {
        (a, b) if a == b => Ok(()),
        (Var(v), t) | (t, Var(v)) => {
            if occurs(v, t) { bail!("occurs check failed: `{}` in `{}`", v, show_ty(t)); }
            if has_free_tyvar(t) { bail!("cannot bind type variable `{}` to generic `{}`", v, show_ty(t)); }
            s.insert(v.clone(), t.clone());
            Ok(())
        }
        (App { name: ln, args: la }, App { name: rn, args: ra }) => {
            if ln != rn || la.len() != ra.len() { bail!("type constructor mismatch: `{}` vs `{}`", show_ty(&l), show_ty(&r)); }
            for (a, b) in la.iter().zip(ra.iter()) { unify(a, b, s)?; }
            Ok(())
        }
        _ => bail!("type mismatch: `{}` vs `{}`", show_ty(&l), show_ty(&r)),
    }
}

fn has_free_tyvar(t: &Ty) -> bool {
    match t {
        Ty::Var(_) => true,
        Ty::App { args, .. } => args.iter().any(has_free_tyvar),
        _ => false,
    }
}

fn ensure_no_free_tyvar(t: &Ty) -> Result<()> {
    if has_free_tyvar(t) { bail!("free type variable not allowed here: `{}`", show_ty(t)); }
    Ok(())
}

fn show_ty(t: &Ty) -> String {
    use std::fmt::Write;
    let mut s = String::new();
    write!(&mut s, "{t}").ok();
    s
}

/* ====== 类型检查器 ====== */
pub struct TyCk<'a> {
    fnscheme: &'a HashMap<String, Scheme>,
    tenv: &'a TraitEnv,
    ienv: &'a ImplEnv,
    globals: HashMap<String, Ty>,
    globals_const: HashSet<String>,
    scopes: Vec<HashMap<String, Ty>>,
    locals_const: Vec<HashSet<String>>,
    current_ret: Option<Ty>,
    in_loop: usize,

    current_tparams: HashSet<String>,
    current_where: Vec<WherePred>,
}

impl<'a> TyCk<'a> {
    pub fn new(
        fnscheme: &'a HashMap<String, Scheme>,
        tenv: &'a TraitEnv,
        ienv: &'a ImplEnv,
        globals: HashMap<String, Ty>,
        globals_const: HashSet<String>,
    ) -> Self {
        Self {
            fnscheme, tenv, ienv,
            globals, globals_const,
            scopes: vec![], locals_const: vec![],
            current_ret: None, in_loop: 0,
            current_tparams: HashSet::new(),
            current_where: Vec::new(),
        }
    }

    fn push(&mut self) { self.scopes.push(HashMap::new()); self.locals_const.push(HashSet::new()); }
    fn pop (&mut self) { self.scopes.pop(); self.locals_const.pop(); }

    fn lookup(&self, n: &str) -> Option<Ty> {
        for s in self.scopes.iter().rev() { if let Some(t) = s.get(n) { return Some(t.clone()); } }
        self.globals.get(n).cloned()
    }

    fn is_const_name(&self, n:&str)->bool{
        for cset in self.locals_const.iter().rev() { if cset.contains(n) { return true; } }
        self.globals_const.contains(n)
    }

    #[inline] fn is_intish(&self, t: &Ty) -> bool { matches!(t, Ty::Int | Ty::Long | Ty::Char) }
    #[inline] fn is_floatish(&self, t: &Ty) -> bool { matches!(t, Ty::Float | Ty::Double) }
    #[inline] fn as_intish(&self, t: &Ty) -> Ty { if *t == Ty::Char { Ty::Int } else { t.clone() } }

    fn common_numeric(&self, l:&Ty, r:&Ty) -> Result<Ty> {
        use Ty::*;
        let l = self.as_intish(l);
        let r = self.as_intish(r);
        if l==Double || r==Double { return Ok(Double); }
        if l==Float  || r==Float  { return Ok(Float); }
        if l==Long   || r==Long   { return Ok(Long); }
        if l==Int && r==Int { return Ok(Int); }
        Err(anyhow!("numeric types expected, got `{}` and `{}`", l, r))
    }

    fn require_assignable(&self, src:&Ty, dst:&Ty)->Result<()>{
        use Ty::*;
        if src==dst { return Ok(()); }
        ensure_no_free_tyvar(src)?; ensure_no_free_tyvar(dst)?;
        let src_n = self.as_intish(src);
        let dst_n = self.as_intish(dst);

        match (src_n, dst_n) {
            (Int, Long) => Ok(()),
            (Int, Float) | (Long, Float) => Ok(()),
            (Int, Double) | (Long, Double) | (Float, Double) => Ok(()),
            (s, d) => Err(anyhow!("expect `{}`, got `{}`", d, s)),
        }
    }

    fn check_fun(&mut self, f: &FunDecl) -> Result<()> {
        self.current_ret = Some(f.ret.clone());
        self.current_tparams = f.type_params.iter().cloned().collect();
        self.current_where = f.where_bounds.clone();

        self.push();
        for (n, t) in &f.params {
            if self.scopes.last().unwrap().contains_key(n) {
                return Err(anyhow!("dup param `{}`", n));
            }
            self.scopes.last_mut().unwrap().insert(n.clone(), t.clone());
        }
        let _ = self.block(&f.body)?;
        self.pop();

        self.current_ret = None;
        self.current_tparams.clear();
        self.current_where.clear();
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
            Stmt::Let { name, ty, init, is_const } => {
                let t = self.expr(init)?;
                self.require_assignable(&t, ty)
                    .map_err(|e|anyhow!("let `{}`: {}", name, e))?;
                if self.scopes.last().unwrap().contains_key(name) {
                    return Err(anyhow!("dup local `{}`", name));
                }
                self.scopes.last_mut().unwrap().insert(name.clone(), ty.clone());
                if *is_const { self.locals_const.last_mut().unwrap().insert(name.clone()); }
                Ok(())
            }

            Stmt::Assign { name, expr } => {
                let var_ty = self.lookup(name)
                    .ok_or_else(||anyhow!("unknown var `{}`", name))?;
                if self.is_const_name(name) {
                    return Err(anyhow!("cannot assign to const `{}`", name));
                }
                let rhs_ty = self.expr(expr)?;
                self.require_assignable(&rhs_ty, &var_ty)?;
                Ok(())
            }

            Stmt::If { cond, then_b, else_b } => {
                let ct = self.expr(cond)?;
                self.require_assignable(&ct, &Ty::Bool)
                    .map_err(|e|anyhow!("if(cond): {e}"))?;
                let _ = self.block(then_b)?;
                if let Some(eb) = else_b { let _ = self.block(eb)?; }
                Ok(())
            }

            Stmt::While { cond, body } => {
                let t = self.expr(cond)?;
                self.require_assignable(&t, &Ty::Bool)
                    .map_err(|e|anyhow!("while cond: {}", e))?;
                self.in_loop += 1;
                let _ = self.block(body)?;
                self.in_loop -= 1;
                Ok(())
            }

            Stmt::For { init, cond, step, body } => {
                self.push();
                if let Some(fi) = init {
                    match fi {
                        ForInit::Let { name, ty, init, is_const } => {
                            let t = self.expr(init)?;
                            self.require_assignable(&t, ty)?;
                            if self.scopes.last().unwrap().contains_key(name) {
                                return Err(anyhow!("dup local `{}`", name));
                            }
                            self.scopes.last_mut().unwrap().insert(name.clone(), ty.clone());
                            if *is_const { self.locals_const.last_mut().unwrap().insert(name.clone()); }
                        }
                        ForInit::Assign { name, expr } => {
                            let vt = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{}`", name))?;
                            let et = self.expr(expr)?; self.require_assignable(&et, &vt)?;
                        }
                        ForInit::Expr(e) => { let _= self.expr(e)?; }
                    }
                }
                if let Some(c) = cond {
                    let t = self.expr(c)?; self.require_assignable(&t, &Ty::Bool)
                        .map_err(|e|anyhow!("for(cond): {e}"))?;
                }
                if let Some(st) = step {
                    match st {
                        ForStep::Assign { name, expr } => {
                            let vt = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{}`", name))?;
                            let et = self.expr(expr)?; self.require_assignable(&et, &vt)?;
                        }
                        ForStep::Expr(e) => { let _ = self.expr(e)?; }
                    }
                }
                self.in_loop += 1;
                let _ = self.block(body)?;
                self.in_loop -= 1;
                self.pop();
                Ok(())
            }

            Stmt::Break => {
                if self.in_loop == 0 { return Err(anyhow!("`break` outside of loop")); }
                Ok(())
            }
            Stmt::Continue => {
                if self.in_loop == 0 { return Err(anyhow!("`continue` outside of loop")); }
                Ok(())
            }

            Stmt::Expr(e) => { let _ = self.expr(e)?; Ok(()) }
            Stmt::Return(opt) => {
                let ret_ty = self.current_ret
                    .clone().ok_or_else(||anyhow!("return outside function"))?;
                if let Some(e) = opt {
                    let t = self.expr(e)?; self.require_assignable(&t, &ret_ty)?;
                } else if ret_ty != Ty::Void {
                    return Err(anyhow!("function expects `{}`, but return without value", ret_ty));
                }
                Ok(())
            }
        }
    }

    fn expr(&mut self, e:&Expr)->Result<Ty>{
        use BinOp::*; use UnOp::*;
        Ok(match e {
            Expr::Int(_)=>Ty::Int,
            Expr::Long(_)=>Ty::Long,
            Expr::Bool(_)=>Ty::Bool,
            Expr::Str(_)=>Ty::String,
            Expr::Char(_)=>Ty::Char,
            Expr::Float(_)=>Ty::Float,
            Expr::Double(_)=>Ty::Double,
            Expr::Var(n)=> self.lookup(n).ok_or_else(||anyhow!("unknown var `{n}`"))?,

            Expr::Unary{op,rhs} => {
                let t=self.expr(rhs)?;
                match op {
                    Neg=>{
                        if self.is_floatish(&t) || self.is_intish(&t) { self.as_intish(&t) }
                        else { return Err(anyhow!("unary `-` expects numeric, got `{}`", t)); }
                    }
                    Not=>{
                        self.require_assignable(&t,&Ty::Bool)?; Ty::Bool
                    }
                }
            }

            Expr::Binary{op,lhs,rhs}=>{
                let lt=self.expr(lhs)?; let rt=self.expr(rhs)?;
                match op {
                    Add|Sub|Mul|Div => {
                        if has_free_tyvar(&lt) || has_free_tyvar(&rt) {
                            bail!("numeric op requires concrete numeric types (consider adding a trait bound)");
                        }
                        self.common_numeric(&lt, &rt)?
                    }
                    Lt|Le|Gt|Ge => {
                        if has_free_tyvar(&lt) || has_free_tyvar(&rt) { bail!("comparison requires concrete numeric types"); }
                        let _ = self.common_numeric(&lt, &rt)?; Ty::Bool
                    }
                    Eq|Ne => {
                        if (self.is_intish(&lt) || self.is_floatish(&lt)) && (self.is_intish(&rt) || self.is_floatish(&rt)) {
                            if has_free_tyvar(&lt) || has_free_tyvar(&rt) { bail!("equality on numeric requires concrete types"); }
                            let _ = self.common_numeric(&lt, &rt)?; Ty::Bool
                        } else if lt == Ty::Bool && rt == Ty::Bool {
                            Ty::Bool
                        } else {
                            if lt != rt { return Err(anyhow!("comparison requires same types: `{}` vs `{}`", lt, rt)); }
                            if has_free_tyvar(&lt) { bail!("equality for generic type requires trait bound"); }
                            Ty::Bool
                        }
                    }
                    And|Or => {
                        self.require_assignable(&lt,&Ty::Bool)?; self.require_assignable(&rt,&Ty::Bool)?; Ty::Bool
                    }
                }
            }

            Expr::If{cond,then_b,else_b}=>{
                let ct=self.expr(cond)?; self.require_assignable(&ct,&Ty::Bool)
                    .map_err(|e|anyhow!("if cond: {}", e))?;
                let tt=self.block(then_b)?; let et=self.block(else_b)?;
                if tt!=et { return Err(anyhow!("if branches type mismatch: then `{}`, else `{}`", tt, et)); }
                tt
            }

            Expr::Match { scrut, arms, default } => {
                let st = self.expr(scrut)?;
                if has_free_tyvar(&st) { bail!("match scrutinee must be concrete type, got `{}`", st); }
                let mut out_ty: Option<Ty> = None;

                for (pat, blk) in arms {
                    match (pat, &st) {
                        (Pattern::Int(_),  Ty::Int)  => {}
                        (Pattern::Long(_), Ty::Long) => {}
                        (Pattern::Bool(_), Ty::Bool) => {}
                        (Pattern::Char(_), Ty::Char) => {}
                        (Pattern::Wild,   _)         => {}
                        (Pattern::Int(_),  other) => return Err(anyhow!("match pattern Int but scrut is `{}`", other)),
                        (Pattern::Long(_), other) => return Err(anyhow!("match pattern Long but scrut is `{}`", other)),
                        (Pattern::Bool(_), other) => return Err(anyhow!("match pattern Bool but scrut is `{}`", other)),
                        (Pattern::Char(_), other) => return Err(anyhow!("match pattern Char but scrut is `{}`", other)),
                    }
                    let bt = self.block(blk)?;
                    match &mut out_ty {
                        None => out_ty = Some(bt),
                        Some(t) => { if *t != bt { return Err(anyhow!("match arm type mismatch: expect `{t}`, got `{bt}`")); } }
                    }
                }
                if let Some(d) = default {
                    let dt = self.block(d)?;
                    if let Some(t) = &out_ty { if *t != dt { return Err(anyhow!("match default type mismatch")); } }
                    out_ty.get_or_insert(dt);
                }
                out_ty.unwrap_or(Ty::Int)
            }

            // 关键：函数/方法调用
            Expr::Call{callee, generics, args}=>{
                match callee {
                    Callee::Name(name) => {
                        let sch = self.fnscheme.get(name)
                            .ok_or_else(||anyhow!("unknown function `{}`", name))?.clone();

                        let mut subst: Subst = Subst::new();
                        if !sch.tparams.is_empty() {
                            if !generics.is_empty() {
                                if generics.len()!=sch.tparams.len() {
                                    bail!("function `{}` expects {} type args, got {}", name, sch.tparams.len(), generics.len());
                                }
                                for (tp, ta) in sch.tparams.iter().zip(generics.iter()) {
                                    ensure_no_free_tyvar(ta)?; subst.insert(tp.clone(), ta.clone());
                                }
                            } else {
                                if args.len()!=sch.params.len(){ bail!("function `{}` expects {} args, got {}", name, sch.params.len(), args.len()); }
                                for (arg_expr, pty) in args.iter().zip(sch.params.iter()) {
                                    let at = self.expr(arg_expr)?;
                                    if has_free_tyvar(&at) { bail!("cannot infer from generic actual type `{}`", at); }
                                    unify(pty, &at, &mut subst)?;
                                }
                            }
                        } else if sch.params.len()!=args.len() {
                            bail!("function `{}` expects {} args, got {}", name, sch.params.len(), args.len());
                        }

                        let inst_params: Vec<Ty> = sch.params.iter().map(|t| apply_subst(t, &subst)).collect();
                        let inst_ret: Ty = apply_subst(&sch.ret, &subst);

                        self.check_where_bounds(&sch.where_bounds, &subst)
                            .map_err(|e| anyhow!("`{}` where: {}", name, e))?;

                        for (i,(a,pt)) in args.iter().zip(inst_params.iter()).enumerate() {
                            let at=self.expr(a)?; self.require_assignable(&at, pt)
                                .map_err(|e|anyhow!("arg#{i}: {e}"))?;
                        }
                        inst_ret
                    }

                    Callee::Qualified { trait_name, method } => {
                        if generics.is_empty() {
                            bail!("qualified call `{}::{}` needs explicit type args", trait_name, method);
                        }
                        // 生成与 codegen 同名的具体符号，并从 fnscheme 中取签名
                        let sym = mangle_impl_method(trait_name, generics, method);
                        let sch = self.fnscheme.get(&sym)
                            .ok_or_else(|| anyhow!("no such impl method `{}`; missing `impl {0}<...>`?", sym))?
                            .clone();
                        if sch.tparams.len() != 0 {
                            bail!("impl method should be monomorphic here");
                        }
                        if sch.params.len() != args.len() {
                            bail!("function `{}` expects {} args, got {}", sym, sch.params.len(), args.len());
                        }
                        for (i,(a,pt)) in args.iter().zip(sch.params.iter()).enumerate() {
                            let at=self.expr(a)?; self.require_assignable(&at, pt)
                                .map_err(|e|anyhow!("arg#{i}: {e}"))?;
                        }
                        sch.ret
                    }
                }
            }

            Expr::Block(b)=> self.block(b)?,
        })
    }

    fn check_where_bounds(&self, preds: &[WherePred], subst: &Subst)->Result<()>{
        for wp in preds {
            let ty_i = apply_subst(&wp.ty, subst);
            if has_free_tyvar(&ty_i) {
                bail!("unresolved type parameter in where: `{}`", ty_i);
            }
            for b in &wp.bounds {
                // bound 可能写成 Eq<T> / Foo<A,B> / 省略成 Eq（表示绑定在 ty_i 上），此处统一转为完整实参向量
                let mut all_args: Vec<Ty> = if b.args.is_empty() {
                    vec![ty_i.clone()]
                } else {
                    let mut v = Vec::new();
                    for a in &b.args { v.push(apply_subst(a, subst)); }
                    v
                };
                if all_args.iter().any(has_free_tyvar) {
                    bail!("where bound has unresolved generic: {}<...>", b.name);
                }
                if !self.ienv.has_impl(&b.name, &all_args) {
                    bail!("missing `impl {0}<...>` for `{0}<{1}>`", b.name, all_args.iter().map(show_ty).collect::<Vec<_>>().join(", "));
                }
            }
        }
        Ok(())
    }
}
