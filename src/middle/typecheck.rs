use anyhow::{anyhow, bail, Result};
use std::collections::HashMap; // 对外返回类型仍用 std::HashMap 保持兼容
use std::fmt;

use crate::frontend::ast::*;
use crate::backend::mangle::mangle_impl_method;
use crate::utils::fast::{FastMap, FastSet, SmallVec4};

/* ========= 多态函数签名（小向量优化） ========= */
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Scheme {
    pub tparams: SmallVec4<String>,
    pub params:  SmallVec4<Ty>,
    pub ret: Ty,
    pub where_bounds: SmallVec4<WherePred>,
}

/* ========= 强类型 Key ========= */

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct TraitKey(pub String);

impl From<&str> for TraitKey {
    fn from(s: &str) -> Self { TraitKey(s.to_string()) }
}
impl From<String> for TraitKey {
    fn from(s: String) -> Self { TraitKey(s) }
}
impl fmt::Display for TraitKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result { f.write_str(&self.0) }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct ImplKey {
    pub trait_key: TraitKey,
    pub args: Box<[Ty]>,
}
impl ImplKey {
    #[inline]
    pub fn new(trait_name: &str, args: &[Ty]) -> Self {
        ImplKey { trait_key: TraitKey::from(trait_name), args: args.to_vec().into_boxed_slice() }
    }
}
impl fmt::Display for ImplKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.args.is_empty() {
            write!(f, "{}<>", self.trait_key)
        } else {
            write!(f, "{}<", self.trait_key)?;
            for (i, a) in self.args.iter().enumerate() {
                if i > 0 { write!(f, ", ")?; }
                write!(f, "{a}")?;
            }
            write!(f, ">")
        }
    }
}

/* ========= trait / impl 环境 ========= */
#[derive(Default, Clone)]
pub struct TraitEnv {
    pub decls: FastMap<TraitKey, TraitDecl>, // 强类型键 + 快表
}
impl TraitEnv {
    #[inline]
    pub fn arity(&self, name: &str) -> Option<usize> {
        self.decls.get(&TraitKey::from(name)).map(|d| d.type_params.len())
    }
    #[inline]
    pub fn get(&self, name: &str) -> Option<&TraitDecl> {
        self.decls.get(&TraitKey::from(name))
    }
}

#[derive(Default, Clone)]
pub struct ImplEnv {
    map: FastSet<ImplKey>, // 强类型键 + 快集合
}

impl ImplEnv {
    fn insert(&mut self, tr: &str, args: &[Ty]) -> Result<()> {
        let key = ImplKey::new(tr, args);
        if !self.map.insert(key.clone()) {
            bail!("duplicate impl `{}`", key);
        }
        Ok(())
    }
    #[inline]
    pub fn has_impl(&self, tr: &str, args: &[Ty]) -> bool {
        self.map.contains(&ImplKey::new(tr, args))
    }
}

/* ====== 代换 / 统一 ====== */
type Subst = FastMap<String, Ty>;

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
    let _ = write!(&mut s, "{t}");
    s
}

/* ========= 对外入口：返回 (函数方案们, 全局类型, TraitEnv, ImplEnv) ========= */
pub fn typecheck_program(
    p: &Program
) -> Result<(HashMap<String, Vec<Scheme>>, HashMap<String, Ty>, TraitEnv, ImplEnv)> {
    // 0) 收集 trait / impl（并做结构校验）
    let mut tenv = TraitEnv::default();
    let mut ienv = ImplEnv::default();

    // 收集 trait 声明
    for it in &p.items {
        if let Item::Trait(td) = it {
            let key = TraitKey::from(td.name.clone());
            if tenv.decls.contains_key(&key) {
                bail!("duplicate trait `{}`", td.name);
            }
            // 基本自检：方法名重复？
            {
                let mut seen = FastSet::<String>::default();
                for m in &td.items {
                    if !seen.insert(m.name.clone()) {
                        bail!("trait `{}` has duplicate method `{}`", td.name, m.name);
                    }
                }
            }
            tenv.decls.insert(key, td.clone());
        }
    }

    // 校验 impl：trait 存在、元数匹配、方法签名匹配、实参具体；同时注册到 ImplEnv
    for it in &p.items {
        if let Item::Impl(id) = it {
            let td = tenv.get(&id.trait_name)
                .ok_or_else(|| anyhow!("impl refers unknown trait `{}`", id.trait_name))?;

            // 元数匹配
            if td.type_params.len() != id.trait_args.len() {
                bail!(
                    "impl `{}` expects {} type args, got {}",
                    id.trait_name, td.type_params.len(), id.trait_args.len()
                );
            }
            // impl 的实参必须全部是具体类型
            for ta in &id.trait_args { ensure_no_free_tyvar(ta)?; }

            // trait 形参 -> impl 实参 的替换表
            let mut subst: Subst = Subst::default();
            for (tp, ta) in td.type_params.iter().zip(id.trait_args.iter()) {
                subst.insert(tp.clone(), ta.clone());
            }

            // 方法集合相等性检查（不多不少）
            let trait_method_names: FastSet<_> = td.items.iter().map(|m| m.name.clone()).collect();
            let impl_method_names:  FastSet<_> = id.items.iter().map(|m| m.name.clone()).collect();

            for extra in impl_method_names.difference(&trait_method_names) {
                bail!("impl `{}` provides unknown method `{}`", id.trait_name, extra);
            }
            for miss in trait_method_names.difference(&impl_method_names) {
                bail!("impl `{}` missing method `{}`", id.trait_name, miss);
            }

            // 每个方法签名与 trait 一致（经替换后）
            for m in &id.items {
                let decl = td.items.iter().find(|d| d.name == m.name).unwrap();
                let want_params: Vec<Ty> = decl.params.iter().map(|(_, t)| apply_subst(t, &subst)).collect();
                let want_ret: Ty = apply_subst(&decl.ret, &subst);

                for (_, t) in &m.params { ensure_no_free_tyvar(t)?; }
                ensure_no_free_tyvar(&m.ret)?;

                let got_params: Vec<Ty> = m.params.iter().map(|(_, t)| t.clone()).collect();
                if want_params != got_params {
                    bail!(
                        "impl `{}` method `{}` params mismatch: expect ({:?}), got ({:?})",
                        id.trait_name, m.name, want_params, got_params
                    );
                }
                if want_ret != m.ret {
                    bail!(
                        "impl `{}` method `{}` return mismatch: expect `{}`, got `{}`",
                        id.trait_name, m.name, want_ret, m.ret
                    );
                }
            }

            // 记录 impl 实例（供 where 检查 / 合法性校验）
            ienv.insert(&id.trait_name, &id.trait_args)?;
        }
    }

    // 1) 收集函数方案（**允许同名多份**；impl 方法已由 passes 降成自由函数）
    let mut fnscheme = HashMap::<String, Vec<Scheme>>::new();
    for it in &p.items {
        if let Item::Fun(f) = it {
            let entry = fnscheme.entry(f.name.clone()).or_default();
            let sch = Scheme {
                tparams:     f.type_params.iter().cloned().collect(),
                params:      f.params.iter().map(|(_, t)| t.clone()).collect(),
                ret:         f.ret.clone(),
                where_bounds:f.where_bounds.iter().cloned().collect(),
            };
            if !entry.iter().any(|s| s == &sch) {
                entry.push(sch);
            }
        }
    }

    // 2) 收集全局变量
    let mut globals = HashMap::<String, Ty>::new();
    let mut globals_const = FastSet::<String>::default();
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

    Ok((fnscheme, globals, tenv, ienv))
}

fn collect_free_tyvars(t: &Ty, out: &mut FastSet<String>) {
    match t {
        Ty::Var(v) => { out.insert(v.clone()); }
        Ty::App { args, .. } => for a in args { collect_free_tyvars(a, out); }
        _ => {}
    }
}

/* ====== 类型检查器 ====== */
pub struct TyCk<'a> {
    fnscheme: &'a HashMap<String, Vec<Scheme>>,
    // 小优化：按形参数量建立候选索引（快表）
    fnscheme_arity: FastMap<String, FastMap<usize, Vec<Scheme>>>,

    tenv: &'a TraitEnv,
    ienv: &'a ImplEnv,
    globals: HashMap<String, Ty>,      // 对外保持 std
    globals_const: FastSet<String>,    // 快集合
    scopes: Vec<FastMap<String, Ty>>,  // 局部作用域（快表）
    locals_const: Vec<FastSet<String>>,// 局部常量集合
    current_ret: Option<Ty>,
    in_loop: usize,

    current_tparams: FastSet<String>,
    current_where: Vec<WherePred>,
}

impl<'a> TyCk<'a> {
    pub fn new(
        fnscheme: &'a HashMap<String, Vec<Scheme>>,
        tenv: &'a TraitEnv,
        ienv: &'a ImplEnv,
        globals: HashMap<String, Ty>,
        globals_const: FastSet<String>,
    ) -> Self {
        // 构建按形参数量的索引（clone 候选，之后不再持有不可变借用）
        let mut idx: FastMap<String, FastMap<usize, Vec<Scheme>>> = FastMap::default();
        for (name, vecs) in fnscheme {
            let mut sub: FastMap<usize, Vec<Scheme>> = FastMap::default();
            for s in vecs {
                sub.entry(s.params.len()).or_default().push(s.clone());
            }
            idx.insert(name.clone(), sub);
        }

        Self {
            fnscheme,
            fnscheme_arity: idx,
            tenv, ienv,
            globals, globals_const,
            scopes: vec![], locals_const: vec![],
            current_ret: None, in_loop: 0,
            current_tparams: FastSet::default(),
            current_where: Vec::new(),
        }
    }

    fn push(&mut self) { self.scopes.push(FastMap::default()); self.locals_const.push(FastSet::default()); }
    fn pop (&mut self) { self.scopes.pop(); self.locals_const.pop(); }

    fn lookup(&self, n: &str) -> Option<Ty> {
        for s in self.scopes.iter().rev() { if let Some(t) = s.get(n) { return Some(t.clone()); } }
        self.globals.get(n).cloned()
    }

    /// 这些自由类型变量必须全部是当前函数的类型参数
    fn free_vars_all_in_current_tparams(&self, t: &Ty) -> bool {
        let mut s = FastSet::<String>::default();
        collect_free_tyvars(t, &mut s);
        s.into_iter().all(|v| self.current_tparams.contains(&v))
    }

    /// 当前函数的 where 条件是否允许调用 `trait_name<args...>`
    fn where_allows_trait_call(&self, trait_name: &str, targs: &[Ty]) -> Result<bool> {
        let ar = self.tenv.arity(trait_name)
            .ok_or_else(|| anyhow!("unknown trait `{}` in qualified call", trait_name))?;

        for wp in &self.current_where {
            for b in &wp.bounds {
                if b.name != trait_name { continue; }
                if b.args.is_empty() {
                    // 仅一元 trait 可省略参数，表示约束在 wp.ty 上
                    if ar == 1 && targs.len() == 1 && targs[0] == wp.ty {
                        return Ok(true);
                    }
                } else if b.args == targs {
                    return Ok(true);
                }
            }
        }
        Ok(false)
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

            // 函数/方法调用
            Expr::Call{callee, generics, args}=>{
                match callee {
                    Callee::Name(name) => {
                        // 统一走候选选择器（支持重载 + 泛型推断 + where 检查）
                        return self.resolve_fun_call(name, generics, args, "call");
                    }

                    Callee::Qualified { trait_name, method } => {
                        if generics.is_empty() {
                            bail!("qualified call `{}::{}` needs explicit type args", trait_name, method);
                        }

                        // 是否包含当前函数的类型形参（T/U ...）
                        let has_generic_vars = generics.iter().any(has_free_tyvar);

                        // 形参里若有自由类型变量，必须属于当前函数的类型参数集合
                        for ta in generics {
                            if has_free_tyvar(ta) && !self.free_vars_all_in_current_tparams(ta) {
                                bail!("unknown type parameter appears in qualified call: `{}`", show_ty(ta));
                            }
                        }

                        if !has_generic_vars {
                            // —— 全部具体：走“已实例化 impl 方法”的路径 —— //
                            if !self.ienv.has_impl(trait_name, generics) {
                                bail!("no `impl {0}<...>` found matching `{0}<{1}>`",
                                      trait_name,
                                      generics.iter().map(show_ty).collect::<Vec<_>>().join(", "));
                            }
                            let sym = mangle_impl_method(trait_name, generics, method);
                            // 该符号应当是单态的，或只有一个可用定义
                            let schs = self.fnscheme.get(&sym)
                                .ok_or_else(|| anyhow!("no such impl method `{}`; missing `impl {0}<...>`?", sym))?;
                            // 选择：tparams==0 且 params.len()==args.len() 的唯一项
                            let mut pick: Option<Scheme> = None;
                            for s in schs {
                                if !s.tparams.is_empty() { continue; }
                                if s.params.len() != args.len() { continue; }
                                if pick.is_some() {
                                    bail!("ambiguous impl method `{}` (multiple monomorphic candidates)", sym);
                                }
                                pick = Some(s.clone());
                            }
                            let sch = pick.ok_or_else(|| anyhow!("no monomorphic candidate for `{}`", sym))?;

                            // 预先计算参数类型（一次）
                            let mut arg_tys = Vec::with_capacity(args.len());
                            for a in args { arg_tys.push(self.expr(a)?); }

                            // 参数检查
                            for (i,(at, pt)) in arg_tys.iter().zip(sch.params.iter()).enumerate() {
                                self.require_assignable(at, pt)
                                    .map_err(|e|anyhow!("arg#{i}: {e}"))?;
                            }
                            sch.ret
                        } else {
                            // —— 含有类型形参：必须由 where 约束保证存在 impl；按 trait 方法原型做类型检查 —— //
                            // 先把 trait 所需信息 clone 出来，避免后续 self.expr(...) 与借用冲突
                            let (arity, md, tparams): (usize, TraitMethodSig, Vec<String>) = {
                                let td = self.tenv.get(trait_name)
                                    .ok_or_else(|| anyhow!("unknown trait `{}`", trait_name))?;
                                let md_ref = td.items.iter().find(|m| m.name == *method)
                                    .ok_or_else(|| anyhow!("trait `{}` has no method `{}`", trait_name, method))?;
                                (td.type_params.len(), md_ref.clone(), td.type_params.clone())
                            };

                            if arity != generics.len() {
                                bail!(
                                    "trait `{}` expects {} type args, got {}",
                                    trait_name, arity, generics.len()
                                );
                            }

                            if !self.where_allows_trait_call(trait_name, generics)? {
                                bail!(
                                    "cannot call `{0}::<{1}>::{2}` here; missing matching `where` bound",
                                    trait_name,
                                    generics.iter().map(show_ty).collect::<Vec<_>>().join(", "),
                                    method
                                );
                            }

                            // 形参替换：trait 形参 -> 本次提供的实参（其中可含 T/U）
                            let mut subst: Subst = Subst::default();
                            for (tp, ta) in tparams.iter().zip(generics.iter()) {
                                subst.insert(tp.clone(), ta.clone());
                            }
                            let want_params: Vec<Ty> = md.params.iter().map(|(_, t)| apply_subst(t, &subst)).collect();
                            let want_ret: Ty = apply_subst(&md.ret, &subst);

                            if want_params.len() != args.len() {
                                bail!("method `{}` expects {} args, got {}", method, want_params.len(), args.len());
                            }

                            // 预先计算参数类型（一次）
                            let mut arg_tys = Vec::with_capacity(args.len());
                            for a in args { arg_tys.push(self.expr(a)?); }

                            // 参数检查：若涉及类型形参，做“严格相等”；纯具体类型则允许数值提升规则
                            for (i, (at, want)) in arg_tys.iter().zip(want_params.iter()).enumerate() {
                                if has_free_tyvar(at) || has_free_tyvar(want) {
                                    if *at != *want {
                                        bail!("arg#{i}: type mismatch: expect `{}`, got `{}`", show_ty(want), show_ty(at));
                                    }
                                } else {
                                    self.require_assignable(at, want)
                                        .map_err(|e| anyhow!("arg#{i}: {e}"))?;
                                }
                            }

                            // 返回类型也可能仍然带有 T/U；在泛型函数体中这是允许的
                            want_ret
                        }
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
                let ar = self.tenv.arity(&b.name)
                    .ok_or_else(|| anyhow!("unknown trait `{}` in where", b.name))?;

                // 按元数构造完整实参列表
                let all_args: Vec<Ty> = if b.args.is_empty() {
                    if ar != 1 {
                        bail!("trait `{}` expects {} type args in where, got 0", b.name, ar);
                    }
                    vec![ty_i.clone()]
                } else {
                    if b.args.len() != ar {
                        bail!("trait `{}` expects {} type args in where, got {}", b.name, ar, b.args.len());
                    }
                    b.args.iter().map(|a| apply_subst(a, subst)).collect()
                };

                if all_args.iter().any(has_free_tyvar) {
                    bail!("where bound has unresolved generic: {}<...>", b.name);
                }
                if !self.ienv.has_impl(&b.name, &all_args) {
                    bail!("missing `impl {0}<...>` for `{0}<{1}>`",
                          b.name, all_args.iter().map(show_ty).collect::<Vec<_>>().join(", "));
                }
            }
        }
        Ok(())
    }

    /* ============ 重载/泛型 解析核心 ============ */

    #[inline]
    fn is_numeric_widen(&self, src: &Ty, dst: &Ty) -> bool {
        use Ty::*;
        let s = self.as_intish(src);
        let d = self.as_intish(dst);
        matches!(
            (s, d),
            (Int, Long) |
            (Int, Float) | (Long, Float) |
            (Int, Double) | (Long, Double) | (Float, Double)
        )
    }

    /// 根据名字 / 显式类型实参 / 实参表达式，解析“最佳候选”，并返回其实例化后的返回类型
    fn resolve_fun_call(
        &mut self,
        name: &str,
        generics: &[Ty],
        args: &[Expr],
        _call_hint: &str,
    ) -> Result<Ty> {
        // 先算出实参类型，避免借用冲突
        let mut arg_tys = Vec::with_capacity(args.len());
        for a in args { arg_tys.push(self.expr(a)?); }

        // 快速路径：按 arity 取候选，并 clone 成拥有所有权的 Vec
        let cands: Vec<Scheme> = self
            .fnscheme_arity
            .get(name)
            .ok_or_else(|| anyhow!("unknown function `{}`", name))?
            .get(&args.len())
            .ok_or_else(|| anyhow!("no matching overload for `{}` with {} args", name, args.len()))?
            .clone();

        // 评分候选
        let mut viable: Vec<(usize /*score*/, Scheme, Subst, Vec<Ty> /*inst params*/, Ty /*inst ret*/)> =
            Vec::with_capacity(cands.len());

        'CAND: for sch in &cands {
            let mut subst: Subst = Subst::default();

            // 处理类型实参/推断
            if !sch.tparams.is_empty() {
                if !generics.is_empty() {
                    if generics.len() != sch.tparams.len() { continue; }
                    for (tp, ta) in sch.tparams.iter().zip(generics.iter()) {
                        if has_free_tyvar(ta) { continue 'CAND; }
                        subst.insert(tp.clone(), ta.clone());
                    }
                } else {
                    // 从实参类型推断
                    for (at, pty) in arg_tys.iter().zip(sch.params.iter()) {
                        if has_free_tyvar(at) { continue 'CAND; }
                        let mut s_local = subst.clone();
                        if let Err(_) = unify(pty, at, &mut s_local) {
                            continue 'CAND;
                        }
                        subst = s_local;
                    }
                }
            } else if !generics.is_empty() {
                continue;
            }

            // 实例化
            let inst_params: Vec<Ty> = sch.params.iter().map(|t| apply_subst(t, &subst)).collect();
            let inst_ret: Ty = apply_subst(&sch.ret, &subst);

            // where 检查
            if let Err(_) = self.check_where_bounds(&sch.where_bounds, &subst) {
                continue;
            }

            // 参数检查 + 评分
            let mut score: usize = 0;
            for (at, want) in arg_tys.iter().zip(inst_params.iter()) {
                if has_free_tyvar(at) || has_free_tyvar(want) {
                    if *at != *want { continue 'CAND; }
                } else if at == want {
                    // exact：加 0 分
                } else if self.is_numeric_widen(at, want) {
                    score += 1;
                } else {
                    if let Err(_) = self.require_assignable(at, want) {
                        continue 'CAND;
                    } else {
                        score += 2;
                    }
                }
            }

            // 泛型候选稍降级，偏向更具体的定义
            if !sch.tparams.is_empty() { score += 1; }

            viable.push((score, sch.clone(), subst, inst_params, inst_ret));
        }

        if viable.is_empty() {
            if self.fnscheme.contains_key(name) {
                bail!(
                    "no matching overload for `{}`{}",
                    name,
                    if generics.is_empty() {
                        " (try adding explicit type arguments, or ensure a proper trait/impl is in scope)"
                    } else { "" }
                );
            } else {
                bail!("unknown function `{}`", name);
            }
        }

        viable.sort_by_key(|(sc, _, _, _, _)| *sc);
        let best_score = viable[0].0;
        let n_best = viable.iter().take_while(|(sc, ..)| *sc == best_score).count();
        if n_best > 1 {
            bail!("ambiguous call to `{}{}{}: {} candidates tie",
                name,
                if !generics.is_empty() { "<...>" } else { "" },
                if args.is_empty() { "()" } else { "(...)" },
                n_best
            );
        }

        let (_sc, _sch, _subst, _inst_params, inst_ret) = viable.remove(0);
        Ok(inst_ret)
    }
}
