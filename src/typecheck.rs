use anyhow::{anyhow, bail, Result};
use std::collections::{HashMap, HashSet};
use crate::ast::*;

/// 多态函数签名（含类型形参与 where 约束）
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Scheme {
    pub tparams: Vec<String>,
    pub params: Vec<Ty>,
    pub ret: Ty,
    pub where_bounds: Vec<WherePred>,
}

/// trait / impl 环境（仅用于存在性与 where 校验）
#[derive(Default)]
pub struct TraitEnv {
    // name -> TraitDecl（目前检查不需要细节，留存以便将来用）
    pub decls: HashMap<String, TraitDecl>,
}
#[derive(Default)]
pub struct ImplEnv {
    // (trait, concrete_type_key) -> true
    map: HashMap<(String, String), bool>,
}

impl ImplEnv {
    fn insert(&mut self, tr: &str, ty: &Ty) {
        self.map.insert((tr.to_string(), key_of_ty(ty)), true);
    }
    fn has_impl(&self, tr: &str, ty: &Ty) -> bool {
        self.map.contains_key(&(tr.to_string(), key_of_ty(ty)))
    }
}

/// 条目级类型检查：收集函数方案 / 全局变量 / trait/impl，并逐函数检查
pub fn typecheck_program(p: &Program)
                         -> Result<(HashMap<String, Scheme>, HashMap<String, Ty>, TraitEnv, ImplEnv)>
{
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
                // 只登记“(trait, 具体类型)”存在（where 校验阶段用）
                ienv.insert(&id.trait_name, &id.for_ty);
            }
            _ => {}
        }
    }

    // 1) 收集函数“方案”（包含 extern）
    let mut fnscheme = HashMap::<String, Scheme>::new();
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

    // 2) 收集全局变量
    let mut globals = HashMap::<String, Ty>::new();
    let mut globals_const = HashSet::<String>::new();
    for it in &p.items {
        if let Item::Global { name, ty, is_const, .. } = it {
            if globals.contains_key(name) {
                return Err(anyhow!("duplicate global `{}`", name));
            }
            // 顶层不允许自由类型变量
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

    // 4) 检查函数（忽略 extern 的函数体）
    for it in &p.items {
        if let Item::Fun(f) = it {
            if !f.is_extern {
                ck.check_fun(f)?;
            }
        }
    }
    Ok((fnscheme, globals, tenv, ienv))
}

/// ====== 类型代换 / 统一 ======

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
            if occurs(v, t) {
                bail!("occurs check failed: `{}` in `{}`", v, show_ty(t));
            }
            // 只允许把类型变量绑定到“具体类型”(无自由变量)；如果你想允许 Var->Var 可以移除此判断
            if has_free_tyvar(t) {
                bail!("cannot bind type variable `{}` to generic `{}`", v, show_ty(t));
            }
            s.insert(v.clone(), t.clone());
            Ok(())
        }
        (App { name: ln, args: la }, App { name: rn, args: ra }) => {
            if ln != rn || la.len() != ra.len() {
                bail!("type constructor mismatch: `{}` vs `{}`", show_ty(&l), show_ty(&r));
            }
            for (a, b) in la.iter().zip(ra.iter()) {
                unify(a, b, s)?;
            }
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

fn key_of_ty(t: &Ty) -> String {
    match t {
        Ty::App { name, args } => format!("{}<{}>", name, args.iter().map(key_of_ty).collect::<Vec<_>>().join(",")),
        Ty::Var(v) => format!("Var({})", v),
        _ => format!("{}", t),
    }
}

/// ====== 类型检查器 ======

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

    // 当前函数的类型形参集合（用于报错/限制）
    current_tparams: HashSet<String>,
    // 当前函数的 where 约束（原样保存；在调用/算术等处并不自动放宽）
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

    fn push(&mut self) {
        self.scopes.push(HashMap::new());
        self.locals_const.push(HashSet::new());
    }
    fn pop(&mut self) {
        self.scopes.pop();
        self.locals_const.pop();
    }

    fn lookup(&self, n: &str) -> Option<Ty> {
        for s in self.scopes.iter().rev() {
            if let Some(t) = s.get(n) { return Some(t.clone()); }
        }
        self.globals.get(n).cloned()
    }

    fn is_const_name(&self, n:&str)->bool{
        for cset in self.locals_const.iter().rev() {
            if cset.contains(n) { return true; }
        }
        self.globals_const.contains(n)
    }

    // ---------- 数值与共同类型/提升规则 ----------

    #[inline] fn is_intish(&self, t: &Ty) -> bool {
        matches!(t, Ty::Int | Ty::Long | Ty::Char)
    }
    #[inline] fn is_floatish(&self, t: &Ty) -> bool {
        matches!(t, Ty::Float | Ty::Double)
    }
    /// Char 参与算术时按 Int 看待
    #[inline] fn as_intish(&self, t: &Ty) -> Ty {
        if *t == Ty::Char { Ty::Int } else { t.clone() }
    }

    /// 两个数值类型的共同类型（浮点优先，再整数）
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

    /// 赋值/参数/返回的“可赋值”关系（窄→宽）
    fn require_assignable(&self, src:&Ty, dst:&Ty)->Result<()>{
        use Ty::*;
        if src==dst { return Ok(()); }
        // 若含自由类型变量，拒绝
        ensure_no_free_tyvar(src)?;
        ensure_no_free_tyvar(dst)?;

        // Char 视为 Int 的语义——只对数值提升生效，原地比较显示仍打印 Char
        let src_n = self.as_intish(src);
        let dst_n = self.as_intish(dst);

        match (src_n, dst_n) {
            // 整数提升
            (Int, Long) => Ok(()),

            // 向浮点提升
            (Int, Float) | (Long, Float) => Ok(()),
            (Int, Double) | (Long, Double) | (Float, Double) => Ok(()),

            // 其余不允许
            (s, d) => Err(anyhow!("expect `{}`, got `{}`", d, s)),
        }
    }

    // ---------- 顶层与语句/表达式 ----------

    fn check_fun(&mut self, f: &FunDecl) -> Result<()> {
        self.current_ret = Some(f.ret.clone());
        self.current_tparams = f.type_params.iter().cloned().collect();
        self.current_where = f.where_bounds.clone();

        self.push();
        for (n, t) in &f.params {
            // 形参可以含类型变量（如 T）
            if self.scopes.last().unwrap().contains_key(n) {
                return Err(anyhow!("dup param `{}`", n));
            }
            self.scopes.last_mut().unwrap().insert(n.clone(), t.clone());
        }
        // 函数返回类型不可含自由类型变量以外的非法位置（这里允许 T/Void 等）
        // 若你要禁止 void + where 组合，可在此加检查

        let _ = self.block(&f.body)?; // 你原先的含义：块值仅用于表达式位置
        self.pop();

        self.current_ret = None;
        self.current_tparams.clear();
        self.current_where.clear();
        Ok(())
    }

    fn block(&mut self, b:&Block)->Result<Ty>{
        self.push();
        for s in &b.stmts { self.stmt(s)?; }
        // 保持你原逻辑：无尾表达式时返回 Int（而非 Void）
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
                } else {
                    if ret_ty != Ty::Void {
                        return Err(anyhow!("function expects `{}`, but return without value", ret_ty));
                    }
                }
                Ok(())
            }
        }
    }

    fn expr(&mut self, e:&Expr)->Result<Ty>{
        use BinOp::*; use UnOp::*;
        Ok(match e {
            // 字面量/变量
            Expr::Int(_)=>Ty::Int,
            Expr::Long(_)=>Ty::Long,
            Expr::Bool(_)=>Ty::Bool,
            Expr::Str(_)=>Ty::String,
            Expr::Char(_)=>Ty::Char,
            Expr::Float(_)=>Ty::Float,
            Expr::Double(_)=>Ty::Double,
            Expr::Var(n)=> self.lookup(n).ok_or_else(||anyhow!("unknown var `{n}`"))?,

            // 一元运算
            Expr::Unary{op,rhs} => {
                let t=self.expr(rhs)?;
                match op {
                    Neg=>{
                        if self.is_floatish(&t) || self.is_intish(&t) {
                            self.as_intish(&t)
                        } else {
                            return Err(anyhow!("unary `-` expects numeric, got `{}`", t));
                        }
                    }
                    Not=>{
                        self.require_assignable(&t,&Ty::Bool)?;
                        Ty::Bool
                    }
                }
            }

            // 二元运算（混合数值）
            Expr::Binary{op,lhs,rhs}=>{
                let lt=self.expr(lhs)?; let rt=self.expr(rhs)?;
                match op {
                    Add|Sub|Mul|Div => {
                        // 若出现类型变量，当前阶段不允许在无约束下参与算术
                        if has_free_tyvar(&lt) || has_free_tyvar(&rt) {
                            bail!("numeric op requires concrete numeric types (consider adding a trait bound)");
                        }
                        self.common_numeric(&lt, &rt)?
                    }
                    Lt|Le|Gt|Ge => {
                        if has_free_tyvar(&lt) || has_free_tyvar(&rt) {
                            bail!("comparison requires concrete numeric types");
                        }
                        let _ = self.common_numeric(&lt, &rt)?; Ty::Bool
                    }
                    Eq|Ne => {
                        if (self.is_intish(&lt) || self.is_floatish(&lt)) &&
                            (self.is_intish(&rt) || self.is_floatish(&rt)) {
                            if has_free_tyvar(&lt) || has_free_tyvar(&rt) {
                                bail!("equality on numeric requires concrete types");
                            }
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
                        self.require_assignable(&lt,&Ty::Bool)?;
                        self.require_assignable(&rt,&Ty::Bool)?;
                        Ty::Bool
                    }
                }
            }

            // 表达式 if
            Expr::If{cond,then_b,else_b}=>{
                let ct=self.expr(cond)?; self.require_assignable(&ct,&Ty::Bool)
                    .map_err(|e|anyhow!("if cond: {}", e))?;
                let tt=self.block(then_b)?; let et=self.block(else_b)?;
                if tt!=et { return Err(anyhow!("if branches type mismatch: then `{}`, else `{}`", tt, et)); }
                tt
            }

            // match 表达式
            Expr::Match { scrut, arms, default } => {
                let st = self.expr(scrut)?;
                if has_free_tyvar(&st) {
                    bail!("match scrutinee must be concrete type, got `{}`", st);
                }
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

            // 调用（支持显式/省略泛型）
            Expr::Call{callee, generics, args}=>{
                let sch = self.fnscheme.get(callee)
                    .ok_or_else(||anyhow!("unknown function `{callee}`"))?.clone();

                // 1) 先根据显式/隐式确定代换 subst
                let mut subst: Subst = Subst::new();
                if !sch.tparams.is_empty() {
                    if !generics.is_empty() {
                        if generics.len()!=sch.tparams.len() {
                            bail!("function `{}` expects {} type args, got {}", callee, sch.tparams.len(), generics.len());
                        }
                        for (tp, ta) in sch.tparams.iter().zip(generics.iter()) {
                            ensure_no_free_tyvar(ta)?;
                            subst.insert(tp.clone(), ta.clone());
                        }
                    } else {
                        // 简单推断：从 args 对 sch.params 做一阶统一（Var 只能绑定具体类型）
                        if args.len()!=sch.params.len(){
                            bail!("function `{}` expects {} args, got {}", callee, sch.params.len(), args.len());
                        }
                        for (arg_expr, pty) in args.iter().zip(sch.params.iter()) {
                            let at = self.expr(arg_expr)?;
                            // 不允许把“含自由类型变量”的实参用于推断
                            if has_free_tyvar(&at) {
                                bail!("cannot infer from generic actual type `{}`", at);
                            }
                            unify(pty, &at, &mut subst)?;
                        }
                    }
                } else {
                    // 非泛型函数：参数数量必须匹配
                    if sch.params.len()!=args.len() {
                        bail!("function `{}` expects {} args, got {}", callee, sch.params.len(), args.len());
                    }
                }

                // 2) 实例化参数/返回类型
                let inst_params: Vec<Ty> = sch.params.iter().map(|t| apply_subst(t, &subst)).collect();
                let inst_ret: Ty = apply_subst(&sch.ret, &subst);

                // 3) 校验 where 约束（套用代换后，要求 impl 存在）
                self.check_where_bounds(&sch.where_bounds, &subst)
                    .map_err(|e| anyhow!("`{}` where: {}", callee, e))?;

                // 4) 逐实参与形参检查
                for (i,(a,pt)) in args.iter().zip(inst_params.iter()).enumerate() {
                    let at=self.expr(a)?;
                    self.require_assignable(&at, pt)
                        .map_err(|e|anyhow!("arg#{i}: {e}"))?;
                }
                inst_ret
            }

            // 块表达式
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
                // 目前 impl 语法是 impl Trait<Concrete>，因此 bound 通常写成 Trait<T>
                // 若 bound.args 为空，按 Trait<ty_i> 解释
                let target_ty = if b.args.is_empty() {
                    ty_i.clone()
                } else if b.args.len() == 1 {
                    let t0 = apply_subst(&b.args[0], subst);
                    if t0 != ty_i {
                        bail!("where bound `{}` applies to `{}`, but target is `{}`", b.name, t0, ty_i);
                    }
                    t0
                } else {
                    bail!("multi-arg trait bound not supported yet: {}<...>", b.name);
                };

                if !self.ienv.has_impl(&b.name, &target_ty) {
                    bail!("missing `impl {0}<{1}>`", b.name, show_ty(&target_ty));
                }
            }
        }
        Ok(())
    }
}
