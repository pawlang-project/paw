// src/typecheck.rs
use anyhow::{anyhow, bail, Result};
use std::collections::HashMap; // 对外返回类型仍用 std::HashMap 保持兼容
use std::fmt;

use crate::backend::mangle::mangle_impl_method;
use crate::diag::DiagSink;
use crate::frontend::ast::*;
use crate::frontend::span::Span;
use crate::utils::fast::{FastMap, FastSet, SmallVec4};

/* ===================== 辅助宏：记录诊断再返回/不中断 ===================== */

macro_rules! tc_bail {
    ($self:ident, $code:expr, $span:expr, $($arg:tt)*) => {{
        $self.diags.error($code, $self.file_id, $span, format!($($arg)*));
        bail!(format!($($arg)*))
    }}; }
macro_rules! tc_err {
    ($self:ident, $code:expr, $span:expr, $($arg:tt)*) => {{
// 不中断，仅记录
        $self.diags.error($code, $self.file_id, $span, format!($($arg)*));
    }}; }

/* ===================== 多态函数签名（小向量优化） ===================== */

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Scheme {
    pub tparams: SmallVec4<String>,
    pub params: SmallVec4<Ty>,
    pub ret: Ty,
    pub where_bounds: SmallVec4<WherePred>,
}

/* ===================== 强类型键 ===================== */

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
    pub fn new(tr: &str, args: &[Ty]) -> Self {
        ImplKey { trait_key: TraitKey::from(tr), args: args.to_vec().into_boxed_slice() }
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

/* ===================== trait / impl 环境 ===================== */

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

/* ===================== 代换 / 统一 ===================== */

type Subst = FastMap<String, Ty>;

fn apply_subst(ty: &Ty, s: &Subst) -> Ty {
    match ty {
        Ty::Var(v) => s.get(v).cloned().unwrap_or_else(|| Ty::Var(v.clone())),
        Ty::App { name, args } => Ty::App { name: name.clone(), args: args.iter().map(|t| apply_subst(t, s)).collect() },
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
    // CHG: 放宽绑定，允许把类型形参绑定到包含自由类型变量的类型（只做 occurs-check）
    use Ty::*;
    let l = apply_subst(lhs, s);
    let r = apply_subst(rhs, s);
    match (&l, &r) {
        (a, b) if a == b => Ok(()),
        (Var(v), t) | (t, Var(v)) => {
            if occurs(v, t) { bail!("occurs check failed: `{}` in `{}`", v, show_ty(t)); }
            s.insert(v.clone(), t.clone());
            Ok(())
        }
        (App { name: ln, args: la }, App { name: rn, args: ra }) => {
            if ln != rn || la.len() != ra.len() {
                bail!("type constructor mismatch: `{}` vs `{}`", show_ty(&l), show_ty(&r));
            }
            for (a, b) in la.iter().zip(ra.iter()) { unify(a, b, s)? }
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

/* ===================== 对外入口 ===================== */
/* 返回：(函数方案们, 全局类型, TraitEnv, ImplEnv) */
pub fn typecheck_program(
    p: &Program,
    file_id: &str,
    diags: &mut DiagSink,
) -> Result<(
    HashMap<String, Vec<Scheme>>,
    HashMap<String, Ty>,
    TraitEnv,
    ImplEnv,
)> {
    // 0) 收集 trait / impl（并做结构校验）
    let mut tenv = TraitEnv::default();
    let mut ienv = ImplEnv::default();

    // 收集 trait 声明
    for it in &p.items {
        if let Item::Trait(td, tspan) = it {
            let key = TraitKey::from(td.name.clone());
            if tenv.decls.contains_key(&key) {
                diags.error("E2001", file_id, Some(*tspan), format!("duplicate trait `{}`", td.name));
                bail!("duplicate trait `{}`", td.name);
            }
            // 基本自检：方法名/关联类型名重复？
            {
                let mut seen_m = FastSet::<String>::default();
                let mut seen_a = FastSet::<String>::default();
                for it in &td.items {
                    match it {
                        TraitItem::Method(m) => {
                            if !seen_m.insert(m.name.clone()) {
                                diags.error(
                                    "E2001",
                                    file_id,
                                    Some(m.span),
                                    format!("trait `{}` has duplicate method `{}`", td.name, m.name),
                                );
                                bail!("trait `{}` has duplicate method `{}`", td.name, m.name);
                            }
                        }
                        TraitItem::AssocType(a) => {
                            if !a.type_params.is_empty() {
                                diags.error(
                                    "E2014",
                                    file_id,
                                    Some(a.span),
                                    format!("trait `{}` associated type `{}` with type params is not supported yet", td.name, a.name),
                                );
                                bail!("unsupported generic associated type `{}` in trait `{}`", a.name, td.name);
                            }
                            if !seen_a.insert(a.name.clone()) {
                                diags.error(
                                    "E2001",
                                    file_id,
                                    Some(a.span),
                                    format!("trait `{}` has duplicate associated type `{}`", td.name, a.name),
                                );
                                bail!("trait `{}` has duplicate associated type `{}`", td.name, a.name);
                            }
                        }
                    }
                }
            }
            tenv.decls.insert(key, td.clone());
        }
    }

    // 校验 impl：trait 存在、元数匹配、方法/关联类型集合匹配、方法签名匹配、实参具体；同时注册到 ImplEnv
    for it in &p.items {
        if let Item::Impl(id, ispan) = it {
            let td = match tenv.get(&id.trait_name) {
                Some(x) => x,
                None => {
                    diags.error("E2002", file_id, Some(*ispan), format!("impl refers unknown trait `{}`", id.trait_name));
                    bail!("impl refers unknown trait `{}`", id.trait_name);
                }
            };

            // 元数匹配
            if td.type_params.len() != id.trait_args.len() {
                diags.error(
                    "E2003",
                    file_id,
                    Some(*ispan),
                    format!(
                        "impl `{}` expects {} type args, got {}",
                        id.trait_name,
                        td.type_params.len(),
                        id.trait_args.len()
                    ),
                );
                bail!(
                    "impl `{}` expects {} type args, got {}",
                    id.trait_name,
                    td.type_params.len(),
                    id.trait_args.len()
                );
            }
            // impl 的实参必须全部是具体类型
            for ta in &id.trait_args {
                if let Err(e) = ensure_no_free_tyvar(ta) {
                    diags.error(
                        "E2004",
                        file_id,
                        Some(*ispan),
                        format!("impl `{}` arg not concrete: {}", id.trait_name, e),
                    );
                    return Err(e);
                }
            }

            // trait 形参 -> impl 实参 的替换表
            let mut subst: Subst = Subst::default();
            for (tp, ta) in td.type_params.iter().zip(id.trait_args.iter()) {
                subst.insert(tp.clone(), ta.clone());
            }

            // 方法/关联类型集合相等性检查（不多不少）
            let trait_method_names: FastSet<_> = td.items.iter()
                .filter_map(|it| if let TraitItem::Method(m) = it { Some(m.name.clone()) } else { None })
                .collect();
            let trait_assoc_names: FastSet<_> = td.items.iter()
                .filter_map(|it| if let TraitItem::AssocType(a) = it { Some(a.name.clone()) } else { None })
                .collect();

            let impl_method_names: FastSet<_> = id.items.iter()
                .filter_map(|it| if let ImplItem::Method(m) = it { Some(m.name.clone()) } else { None })
                .collect();
            let impl_assoc_names: FastSet<_> = id.items.iter()
                .filter_map(|it| if let ImplItem::AssocType(a) = it { Some(a.name.clone()) } else { None })
                .collect();

            for extra in impl_method_names.difference(&trait_method_names) {
                diags.error(
                    "E2005",
                    file_id,
                    Some(*ispan),
                    format!("impl `{}` provides unknown method `{}`", id.trait_name, extra),
                );
                bail!("impl `{}` provides unknown method `{}`", id.trait_name, extra);
            }
            for miss in trait_method_names.difference(&impl_method_names) {
                diags.error(
                    "E2006",
                    file_id,
                    Some(*ispan),
                    format!("impl `{}` missing method `{}`", id.trait_name, miss),
                );
                bail!("impl `{}` missing method `{}`", id.trait_name, miss);
            }
            for extra in impl_assoc_names.difference(&trait_assoc_names) {
                diags.error(
                    "E2005",
                    file_id,
                    Some(*ispan),
                    format!("impl `{}` provides unknown associated type `{}`", id.trait_name, extra),
                );
                bail!("impl `{}` provides unknown associated type `{}`", id.trait_name, extra);
            }
            for miss in trait_assoc_names.difference(&impl_assoc_names) {
                diags.error(
                    "E2006",
                    file_id,
                    Some(*ispan),
                    format!("impl `{}` missing associated type `{}`", id.trait_name, miss),
                );
                bail!("impl `{}` missing associated type `{}`", id.trait_name, miss);
            }

            // 每个方法签名与 trait 一致（经替换后）
            for it2 in &id.items {
                let ImplItem::Method(m) = it2 else { continue };
                let decl = td.items.iter().find_map(|ti| match ti {
                    TraitItem::Method(mm) if mm.name == m.name => Some(mm.clone()),
                    _ => None,
                }).unwrap();

                let want_params: Vec<Ty> =
                    decl.params.iter().map(|(_, t)| apply_subst(t, &subst)).collect();
                let want_ret: Ty = apply_subst(&decl.ret, &subst);

                for (_, t) in &m.params {
                    ensure_no_free_tyvar(t).map_err(|e| {
                        diags.error(
                            "E2007",
                            file_id,
                            Some(m.span),
                            format!("impl `{}` method `{}` param not concrete: {}", id.trait_name, m.name, e),
                        );
                        e
                    })?;
                }
                ensure_no_free_tyvar(&m.ret).map_err(|e| {
                    diags.error(
                        "E2007",
                        file_id,
                        Some(m.span),
                        format!("impl `{}` method `{}` return not concrete: {}", id.trait_name, m.name, e),
                    );
                    e
                })?;

                let got_params: Vec<Ty> = m.params.iter().map(|(_, t)| t.clone()).collect();
                if want_params != got_params {
                    diags.error(
                        "E2008",
                        file_id,
                        Some(m.span),
                        format!(
                            "impl `{}` method `{}` params mismatch: expect ({:?}), got ({:?})",
                            id.trait_name, m.name, want_params, got_params
                        ),
                    );
                    bail!(
                        "impl `{}` method `{}` params mismatch: expect ({:?}), got ({:?})",
                        id.trait_name,
                        m.name,
                        want_params,
                        got_params
                    );
                }
                if want_ret != m.ret {
                    diags.error(
                        "E2009",
                        file_id,
                        Some(m.span),
                        format!(
                            "impl `{}` method `{}` return mismatch: expect `{}`, got `{}`",
                            id.trait_name, m.name, want_ret, m.ret
                        ),
                    );
                    bail!(
                        "impl `{}` method `{}` return mismatch: expect `{}`, got `{}`",
                        id.trait_name,
                        m.name,
                        want_ret,
                        m.ret
                    );
                }
            }

            // 关联类型：具体性 + bound 满足性
            for it2 in &id.items {
                let ImplItem::AssocType(a) = it2 else { continue };
                // impl 侧类型必须具体
                ensure_no_free_tyvar(&a.ty).map_err(|e| {
                    diags.error(
                        "E2012A",
                        file_id,
                        Some(a.span),
                        format!("impl `{}` associated type `{}` must be concrete: {}", id.trait_name, a.name, e),
                    );
                    e
                })?;

                // 找到 trait 中该关联类型的 bound，并检查
                let Some(tr_a) = td.items.iter().find_map(|ti| match ti {
                    TraitItem::AssocType(x) if x.name == a.name => Some(x.clone()),
                    _ => None,
                }) else {
                    continue; // 前面集合相等性已经保证必然存在，这里只是稳妥
                };

                // bound 中可能引用 trait 形参，这里用 subst 替换
                for b in &tr_a.bounds {
                    let ar = match tenv.arity(&b.name) {
                        Some(n) => n,
                        None => {
                            diags.error("E2013A", file_id, Some(a.span),
                                        format!("unknown trait `{}` in bound of associated type `{}`", b.name, a.name));
                            bail!("unknown trait `{}` in assoc bound", b.name);
                        }
                    };

                    // 构造实参：若 bound 未显式提供实参（零长度），当作一元，把关联类型 a.ty 作为唯一实参
                    let args_full: Vec<Ty> = if b.args.is_empty() {
                        if ar != 1 {
                            diags.error("E2013B", file_id, Some(a.span),
                                        format!("trait `{}` expects {} type args in bound, got 0", b.name, ar));
                            bail!("arity mismatch in assoc bound");
                        }
                        vec![a.ty.clone()]
                    } else {
                        if b.args.len() != ar {
                            diags.error("E2013C", file_id, Some(a.span),
                                        format!("trait `{}` expects {} type args in bound, got {}", b.name, ar, b.args.len()));
                            bail!("arity mismatch in assoc bound");
                        }
                        b.args.iter().map(|t| apply_subst(t, &subst)).collect()
                    };

                    // 要求具体并且存在 impl
                    if args_full.iter().any(has_free_tyvar) {
                        diags.error("E2013D", file_id, Some(a.span),
                                    format!("bound `{0}<{1}>` not concrete for associated type `{2}`",
                                            b.name,
                                            args_full.iter().map(show_ty).collect::<Vec<_>>().join(", "),
                                            a.name));
                        bail!("assoc bound not concrete");
                    }
                    if !ienv.has_impl(&b.name, &args_full) {
                        diags.error("E2013E", file_id, Some(a.span),
                                    format!("missing `impl {0}<...>` for bound `{0}<{1}>` (associated type `{2}`)",
                                            b.name,
                                            args_full.iter().map(show_ty).collect::<Vec<_>>().join(", "),
                                            a.name));
                        bail!("missing impl for assoc bound");
                    }
                }
            }

            // 记录 impl 实例（供 where 检查 / 合法性校验）
            ienv.insert(&id.trait_name, &id.trait_args)?;
        }
    }

    // 1) 收集函数方案（允许同名多份；impl 方法已由 passes 降成自由函数）
    let mut fnscheme = HashMap::<String, Vec<Scheme>>::new();
    for it in &p.items {
        if let Item::Fun(f, _fspan) = it {
            let entry = fnscheme.entry(f.name.clone()).or_default();
            let sch = Scheme {
                tparams: f.type_params.iter().cloned().collect(),
                params: f.params.iter().map(|(_, t)| t.clone()).collect(),
                ret: f.ret.clone(),
                where_bounds: f.where_bounds.iter().cloned().collect(),
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
        if let Item::Global { name, ty, is_const, span, .. } = it {
            if globals.contains_key(name) {
                diags.error("E2010", file_id, Some(*span), format!("duplicate global `{}`", name));
                return Err(anyhow!("duplicate global `{}`", name));
            }
            ensure_no_free_tyvar(ty).map_err(|e| {
                diags.error(
                    "E2011",
                    file_id,
                    Some(*span),
                    format!("global `{}` type not concrete: {}", name, e),
                );
                anyhow!("global `{}`: {}", name, e)
            })?;
            globals.insert(name.clone(), ty.clone());
            if *is_const {
                globals_const.insert(name.clone());
            }
        }
    }

    // 3) 检查全局初始化
    let mut ck = TyCk::new(
        &fnscheme,
        &tenv,
        &ienv,
        globals.clone(),
        globals_const.clone(),
        diags,
        file_id,
    );
    for it in &p.items {
        if let Item::Global { name, ty, init, span: gspan, .. } = it {
            let t = ck.expr(init)?;
            ck.require_assignable_from_expr(init, &t, ty)
                .map_err(|e| {
                    ck.diags.error("E2012", file_id, Some(*gspan), format!("global `{}` init: {}", name, e));
                    anyhow!("global `{}` init: {}", name, e)
                })?;
        }
    }

    // 4) 检查函数体（忽略 extern）
    for it in &p.items {
        if let Item::Fun(f, _span) = it {
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
        Ty::App { args, .. } => for a in args { collect_free_tyvars(a, out) },
        _ => {}
    }
}

/* ===================== 类型检查器 ===================== */

pub struct TyCk<'a> {
    fnscheme: &'a HashMap<String, Vec<Scheme>>,
    // 小优化：按形参数量建立候选索引（快表）
    fnscheme_arity: FastMap<String, FastMap<usize, Vec<Scheme>>>,

    tenv: &'a TraitEnv,
    ienv: &'a ImplEnv,
    globals: HashMap<String, Ty>,       // 对外保持 std
    globals_const: FastSet<String>,     // 快集合
    scopes: Vec<FastMap<String, Ty>>,   // 局部作用域（快表）
    locals_const: Vec<FastSet<String>>, // 局部常量集合
    current_ret: Option<Ty>,
    in_loop: usize,

    current_tparams: FastSet<String>,
    current_where: Vec<WherePred>,

    // 诊断
    diags: &'a mut DiagSink,
    file_id: &'a str,
}

impl<'a> TyCk<'a> {
    pub fn new(
        fnscheme: &'a HashMap<String, Vec<Scheme>>,
        tenv: &'a TraitEnv,
        ienv: &'a ImplEnv,
        globals: HashMap<String, Ty>,
        globals_const: FastSet<String>,
        diags: &'a mut DiagSink,
        file_id: &'a str,
    ) -> Self {
        // 构建按形参数量的索引
        let mut idx: FastMap<String, FastMap<usize, Vec<Scheme>>> = FastMap::default();
        for (name, vecs) in fnscheme {
            let mut sub: FastMap<usize, Vec<Scheme>> = FastMap::default();
            for s in vecs { sub.entry(s.params.len()).or_default().push(s.clone()); }
            idx.insert(name.clone(), sub);
        }

        Self {
            fnscheme,
            fnscheme_arity: idx,
            tenv,
            ienv,
            globals,
            globals_const,
            scopes: vec![],
            locals_const: vec![],
            current_ret: None,
            in_loop: 0,
            current_tparams: FastSet::default(),
            current_where: Vec::new(),
            diags,
            file_id,
        }
    }

    fn push(&mut self) { self.scopes.push(FastMap::default()); self.locals_const.push(FastSet::default()); }
    fn pop(&mut self)  { self.scopes.pop(); self.locals_const.pop(); }

    fn lookup(&self, n: &str) -> Option<Ty> {
        for s in self.scopes.iter().rev() {
            if let Some(t) = s.get(n) { return Some(t.clone()); }
        }
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
                    if ar == 1 && targs.len() == 1 && targs[0] == wp.ty { return Ok(true); }
                } else if b.args == targs {
                    return Ok(true);
                }
            }
        }
        Ok(false)
    }

    // NEW: 供被调函数 where 检查使用——若不具体，允许通过调用者 where 满足
    fn caller_where_satisfies(&self, trait_name: &str, args: &[Ty]) -> bool {
        let ar = match self.tenv.arity(trait_name) { Some(n) => n, None => return false };
        for wp in &self.current_where {
            for b in &wp.bounds {
                if b.name != trait_name { continue; }
                if ar == 1 && b.args.is_empty() {
                    if args.len()==1 && args[0] == wp.ty { return true; }
                } else if !b.args.is_empty() && b.args == args {
                    return true;
                }
            }
        }
        false
    }

    fn is_const_name(&self, n: &str) -> bool {
        for cset in self.locals_const.iter().rev() {
            if cset.contains(n) { return true; }
        }
        self.globals_const.contains(n)
    }

    #[inline] fn is_intish(&self, t: &Ty) -> bool {
        matches!(t, Ty::Byte | Ty::Int | Ty::Long | Ty::Char)
    }
    #[inline] fn is_floatish(&self, t: &Ty) -> bool {
        matches!(t, Ty::Float | Ty::Double)
    }
    #[inline] fn is_numeric_ty(&self, t: &Ty) -> bool {
        self.is_intish(t) || self.is_floatish(t)
    }

    /// 只允许“严格相等”的隐式赋值；其它一律提示使用 `as`
    fn require_assignable(&self, src: &Ty, dst: &Ty) -> Result<()> {
        if src == dst { return Ok(()); }
        ensure_no_free_tyvar(src)?;
        ensure_no_free_tyvar(dst)?;
        Err(anyhow!("expect `{}`, got `{}` (use `as` for explicit conversion)", show_ty(dst), show_ty(src)))
    }

    /// CHG: 根据“字面量→目标类型”的放宽规则放行（Byte/Float/Double）
    fn require_assignable_from_expr(&self, expr: &Expr, src: &Ty, dst: &Ty) -> Result<()> {
        if self.literal_coerces_to(expr, dst) { return Ok(()); }
        self.require_assignable(src, dst)
    }

    /// 显式 as 转换的合法性检查（仅做类型层面的“是否允许”判断）
    fn allow_as_cast(&self, src: &Ty, dst: &Ty) -> Result<()> {
        use Ty::*;
        if src == dst { return Ok(()); }

        ensure_no_free_tyvar(src).map_err(|e| anyhow!("source of cast must be concrete: {}", e))?;
        ensure_no_free_tyvar(dst).map_err(|e| anyhow!("target of cast must be concrete: {}", e))?;

        if matches!(src, String | Bool | Void | App { .. }) || matches!(dst, String | Bool | Void | App { .. }) {
            return Err(anyhow!(
                "unsupported cast: `{}` as `{}` (use domain APIs/traits such as parse/Display)",
                show_ty(src), show_ty(dst)
            ));
        }

        let src_int = self.is_intish(src);
        let dst_int = self.is_intish(dst);
        let src_fp = self.is_floatish(src);
        let dst_fp = self.is_floatish(dst);

        if (src_int && dst_int) || (src_fp && dst_fp) { return Ok(()); } // 族内
        if src_int && dst_fp { return Ok(()); } // 整→浮
        if src_fp && dst_int { return Ok(()); } // 浮→整

        Err(anyhow!("cannot cast from `{}` to `{}`", show_ty(src), show_ty(dst)))
    }

    fn check_fun(&mut self, f: &FunDecl) -> Result<()> {
        self.current_ret = Some(f.ret.clone());
        self.current_tparams = f.type_params.iter().cloned().collect();
        self.current_where = f.where_bounds.clone();

        self.push();
        for (n, t) in &f.params {
            if self.scopes.last().unwrap().contains_key(n) {
                tc_bail!(self, "E2100", Some(f.span), "dup param `{}`", n);
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

    fn block(&mut self, b: &Block) -> Result<Ty> {
        self.push();
        for s in &b.stmts { self.stmt(s)? }
        let ty = if let Some(e) = &b.tail { self.expr(e)? } else { Ty::Int };
        self.pop();
        Ok(ty)
    }

    fn stmt(&mut self, s: &Stmt) -> Result<()> {
        match s {
            Stmt::Let { name, ty, init, is_const, span } => {
                let t = self.expr(init)?;
                self.require_assignable_from_expr(init, &t, ty)
                    .map_err(|e| anyhow!("let `{}`: {}", name, e))?;
                if self.scopes.last().unwrap().contains_key(name) {
                    tc_bail!(self, "E2101", Some(*span), "dup local `{}`", name);
                }
                self.scopes.last_mut().unwrap().insert(name.clone(), ty.clone());
                if *is_const { self.locals_const.last_mut().unwrap().insert(name.clone()); }
                Ok(())
            }

            Stmt::Assign { name, expr, span } => {
                let var_ty = match self.lookup(name) {
                    Some(t) => t,
                    None => tc_bail!(self, "E2102", Some(*span), "unknown var `{}`", name),
                };
                if self.is_const_name(name) {
                    tc_bail!(self, "E2103", Some(*span), "cannot assign to const `{}`", name);
                }
                let rhs_ty = self.expr(expr)?;
                self.require_assignable_from_expr(expr, &rhs_ty, &var_ty)?;
                Ok(())
            }

            Stmt::If { cond, then_b, else_b, span } => {
                let ct = self.expr(cond)?;
                self.require_assignable(&ct, &Ty::Bool).map_err(|e| {
                    tc_err!(self, "E2108", Some(*span), "if(cond) expects Bool: {}", e);
                    anyhow!("if(cond): {e}")
                })?;
                let _ = self.block(then_b)?;
                if let Some(eb) = else_b { let _ = self.block(eb)?; }
                Ok(())
            }

            Stmt::While { cond, body, span } => {
                let t = self.expr(cond)?;
                self.require_assignable(&t, &Ty::Bool).map_err(|e| {
                    tc_err!(self, "E2109", Some(*span), "while(cond) expects Bool: {}", e);
                    anyhow!("while cond: {}", e)
                })?;
                self.in_loop += 1;
                let _ = self.block(body)?;
                self.in_loop -= 1;
                Ok(())
            }

            Stmt::For { init, cond, step, body, span } => {
                self.push();
                if let Some(fi) = init {
                    match fi {
                        ForInit::Let { name, ty, init, is_const, span: lspan } => {
                            let t = self.expr(init)?;
                            self.require_assignable_from_expr(init, &t, ty).map_err(|e| {
                                tc_err!(self, "E2110", Some(*lspan), "for-init let type mismatch: {}", e);
                                e
                            })?;
                            if self.scopes.last().unwrap().contains_key(name) {
                                tc_bail!(self, "E2101", Some(*lspan), "dup local `{}`", name);
                            }
                            self.scopes.last_mut().unwrap().insert(name.clone(), ty.clone());
                            if *is_const { self.locals_const.last_mut().unwrap().insert(name.clone()); }
                        }
                        ForInit::Assign { name, expr, span: aspan } => {
                            let vt = match self.lookup(name) {
                                Some(t) => t,
                                None => tc_bail!(self, "E2102", Some(*aspan), "unknown var `{}`", name),
                            };
                            let et = self.expr(expr)?;
                            self.require_assignable_from_expr(expr, &et, &vt)?;
                        }
                        ForInit::Expr(_, _espan) => { let _ = self.expr(match fi { ForInit::Expr(e,_) => e, _ => unreachable!() })?; }
                    }
                }
                if let Some(c) = cond {
                    let t = self.expr(c)?;
                    self.require_assignable(&t, &Ty::Bool).map_err(|e| {
                        tc_err!(self, "E2111", Some(*span), "for(cond) expects Bool: {}", e);
                        anyhow!("for(cond): {e}")
                    })?;
                }
                if let Some(st) = step {
                    match st {
                        ForStep::Assign { name, expr, span: sspan } => {
                            let vt = match self.lookup(name) {
                                Some(t) => t,
                                None => tc_bail!(self, "E2102", Some(*sspan), "unknown var `{}`", name),
                            };
                            let et = self.expr(expr)?;
                            self.require_assignable_from_expr(expr, &et, &vt)?;
                        }
                        ForStep::Expr(_, _es) => {
                            let _ = self.expr(match st { ForStep::Expr(e,_) => e, _ => unreachable!() })?;
                        }
                    }
                }
                self.in_loop += 1;
                let _ = self.block(body)?;
                self.in_loop -= 1;
                self.pop();
                Ok(())
            }

            Stmt::Break { span } => {
                if self.in_loop == 0 {
                    tc_bail!(self, "E2104", Some(*span), "`break` outside of loop");
                }
                Ok(())
            }
            Stmt::Continue { span } => {
                if self.in_loop == 0 {
                    tc_bail!(self, "E2105", Some(*span), "`continue` outside of loop");
                }
                Ok(())
            }

            Stmt::Expr { expr, .. } => { let _ = self.expr(expr)?; Ok(()) }

            Stmt::Return { expr, span } => {
                let ret_ty = match self.current_ret.clone() {
                    Some(t) => t,
                    None => tc_bail!(self, "E2106", Some(*span), "return outside function"),
                };
                if let Some(e) = expr {
                    let t = self.expr(e)?;
                    self.require_assignable_from_expr(e, &t, &ret_ty)?;
                } else if ret_ty != Ty::Void {
                    tc_bail!(self, "E2107", Some(*span), "function expects `{}`, but return without value", ret_ty);
                }
                Ok(())
            }
        }
    }

    fn expr(&mut self, e: &Expr) -> Result<Ty> {
        use BinOp::*;
        use UnOp::*;
        Ok(match e {
            Expr::Int   { .. } => Ty::Int,
            Expr::Long  { .. } => Ty::Long,
            Expr::Bool  { .. } => Ty::Bool,
            Expr::Str   { .. } => Ty::String,
            Expr::Char  { .. } => Ty::Char,
            Expr::Float { .. } => Ty::Float,
            Expr::Double{ .. } => Ty::Double,

            Expr::Var { name, span } => match self.lookup(name) {
                Some(t) => t,
                None => tc_bail!(self, "E2102", Some(*span), "unknown var `{}`", name),
            },

            Expr::Unary { op, rhs, span } => {
                let t = self.expr(rhs)?;
                match op {
                    Neg => {
                        if self.is_numeric_ty(&t) { t }
                        else { tc_bail!(self, "E2200", Some(*span), "unary `-` expects numeric, got `{}`", t); }
                    }
                    Not => {
                        self.require_assignable(&t, &Ty::Bool)
                            .map_err(|e| anyhow!("unary ! expects Bool: {}", e))?;
                        Ty::Bool
                    }
                }
            }

            Expr::Binary { op, lhs, rhs, span } => {
                let lt = self.expr(lhs)?;
                let rt = self.expr(rhs)?;
                match op {
                    Add | Sub | Mul | Div => {
                        if has_free_tyvar(&lt) || has_free_tyvar(&rt) {
                            tc_bail!(self, "E2201", Some(*span),
                                "numeric op requires concrete numeric types (consider adding a trait bound)");
                        }
                        if !self.is_numeric_ty(&lt) || !self.is_numeric_ty(&rt) {
                            tc_bail!(self, "E2201", Some(*span), "numeric op expects numeric types");
                        }
                        if lt != rt {
                            tc_bail!(self, "E2201", Some(*span),
                                "operands must have the same type for arithmetic: `{}` vs `{}`; use `as`", lt, rt);
                        }
                        lt
                    }
                    Lt | Le | Gt | Ge => {
                        if has_free_tyvar(&lt) || has_free_tyvar(&rt) {
                            tc_bail!(self, "E2202", Some(*span), "comparison requires concrete numeric types");
                        }
                        if !self.is_numeric_ty(&lt) || !self.is_numeric_ty(&rt) {
                            tc_bail!(self, "E2202", Some(*span), "comparison expects numeric types");
                        }
                        if lt != rt {
                            tc_bail!(self, "E2202", Some(*span),
                                "comparison operands must have the same type: `{}` vs `{}`; use `as`", lt, rt);
                        }
                        Ty::Bool
                    }
                    Eq | Ne => {
                        if lt != rt {
                            tc_bail!(self, "E2204", Some(*span),
                                "equality requires the same type: `{}` vs `{}`; use `as`", lt, rt);
                        }
                        if has_free_tyvar(&lt) {
                            tc_bail!(self, "E2205", Some(*span), "equality for generic type requires trait bound");
                        }
                        Ty::Bool
                    }
                    And | Or => {
                        self.require_assignable(&lt, &Ty::Bool)?;
                        self.require_assignable(&rt, &Ty::Bool)?;
                        Ty::Bool
                    }
                }
            }

            // 显式 as：仅返回目标类型，这里做合法性与具体性检查
            Expr::Cast { expr, ty, span } => {
                let src_t = self.expr(expr)?;
                match self.allow_as_cast(&src_t, ty) {
                    Ok(()) => ty.clone(),
                    Err(e) => tc_bail!(self, "E2400", Some(*span),
                        "invalid `as` cast from `{}` to `{}`: {}", show_ty(&src_t), show_ty(ty), e),
                }
            }

            Expr::If { cond, then_b, else_b, span } => {
                let ct = self.expr(cond)?;
                self.require_assignable(&ct, &Ty::Bool).map_err(|e| anyhow!("if cond: {}", e))?;
                let tt = self.block(then_b)?;
                let et = self.block(else_b)?;
                if tt != et {
                    tc_bail!(self, "E2210", Some(*span), "if branches type mismatch: then `{}`, else `{}`", tt, et);
                }
                tt
            }

            Expr::Match { scrut, arms, default, span } => {
                let st = self.expr(scrut)?;
                if has_free_tyvar(&st) {
                    tc_bail!(self, "E2220", Some(*span), "match scrutinee must be concrete type, got `{}`", st);
                }
                let mut out_ty: Option<Ty> = None;

                for (pat, blk) in arms {
                    match (pat, &st) {
                        (Pattern::Int(_), Ty::Int) => {}
                        (Pattern::Long(_), Ty::Long) => {}
                        (Pattern::Bool(_), Ty::Bool) => {}
                        (Pattern::Char(_), Ty::Char) => {}
                        (Pattern::Wild, _) => {}
                        (Pattern::Int(_), other)  => tc_bail!(self, "E2221", Some(blk.span), "pattern Int but scrut is `{}`", other),
                        (Pattern::Long(_), other) => tc_bail!(self, "E2221", Some(blk.span), "pattern Long but scrut is `{}`", other),
                        (Pattern::Bool(_), other) => tc_bail!(self, "E2221", Some(blk.span), "pattern Bool but scrut is `{}`", other),
                        (Pattern::Char(_), other) => tc_bail!(self, "E2221", Some(blk.span), "pattern Char but scrut is `{}`", other),
                    }
                    let bt = self.block(blk)?;
                    match &mut out_ty {
                        None => out_ty = Some(bt),
                        Some(t) => {
                            if *t != bt {
                                tc_bail!(self, "E2222", Some(blk.span), "match arm type mismatch: expect `{t}`, got `{bt}`");
                            }
                        }
                    }
                }
                if let Some(d) = default {
                    let dt = self.block(d)?;
                    if let Some(t) = &out_ty {
                        if *t != dt {
                            tc_bail!(self, "E2223", Some(d.span), "match default type mismatch");
                        }
                    }
                    out_ty.get_or_insert(dt);
                }
                out_ty.unwrap_or(Ty::Int)
            }

            // 函数/方法调用
            Expr::Call { callee, generics, args, span } => match callee {
                Callee::Name(name) => {
                    return self.resolve_fun_call(name, generics, args, Some(*span));
                }
                Callee::Qualified { trait_name, method } => {
                    if generics.is_empty() {
                        tc_bail!(self, "E2303", Some(*span),
                            "qualified call `{}::{}` needs explicit type args", trait_name, method);
                    }

                    let has_generic_vars = generics.iter().any(has_free_tyvar);

                    // 形参里若有自由类型变量，必须属于当前函数的类型参数集合
                    for ta in generics {
                        if has_free_tyvar(ta) && !self.free_vars_all_in_current_tparams(ta) {
                            tc_bail!(self, "E2304", Some(*span),
                                "unknown type parameter appears in qualified call: `{}`", show_ty(ta));
                        }
                    }

                    if !has_generic_vars {
                        // —— 全部具体：走“已实例化 impl 方法”的路径 —— //
                        if !self.ienv.has_impl(trait_name, generics) {
                            tc_bail!(self, "E2305", Some(*span),
                                "no `impl {0}<...>` found matching `{0}<{1}>`",
                                trait_name,
                                generics.iter().map(show_ty).collect::<Vec<_>>().join(", "));
                        }
                        let sym = mangle_impl_method(trait_name, generics, method);
                        let schs = self.fnscheme.get(&sym)
                            .ok_or_else(|| anyhow!("no such impl method `{}`; missing `impl {0}<...>`?", sym))?;
                        // 选择：tparams==0 且 params.len()==args.len() 的唯一项
                        let mut pick: Option<Scheme> = None;
                        for s in schs {
                            if !s.tparams.is_empty() { continue; }
                            if s.params.len() != args.len() { continue; }
                            if pick.is_some() {
                                tc_bail!(self, "E2306", Some(*span),
                                    "ambiguous impl method `{}` (multiple monomorphic candidates)", sym);
                            }
                            pick = Some(s.clone());
                        }
                        let sch = pick.ok_or_else(|| anyhow!("no monomorphic candidate for `{}`", sym))?;

                        // 预先计算参数类型（一次）
                        let mut arg_tys = Vec::with_capacity(args.len());
                        for a in args { arg_tys.push(self.expr(a)?); }

                        // 参数检查（严格同型，但允许“字面量→目标类型”的放宽）
                        for (i, (at, pt)) in arg_tys.iter().zip(sch.params.iter()).enumerate() {
                            if at == pt { continue; }
                            if self.literal_coerces_to(&args[i], pt) { continue; }
                            tc_bail!(self, "E2309", Some(*span),
                                "arg#{i}: type mismatch: expect `{}`, got `{}`", show_ty(pt), show_ty(at));
                        }
                        sch.ret
                    } else {
                        // —— 含有类型形参：必须由 where 约束保证存在 impl；按 trait 方法原型做类型检查 —— //
                        let (arity, md, tparams): (usize, TraitMethodSig, Vec<String>) = {
                            let td = self.tenv.get(trait_name)
                                .ok_or_else(|| anyhow!("unknown trait `{}`", trait_name))?;
                            let md_ref = td.items.iter().find_map(|ti| match ti {
                                TraitItem::Method(m) if m.name == *method => Some(m.clone()),
                                _ => None,
                            }).ok_or_else(|| anyhow!("trait `{}` has no method `{}`", trait_name, method))?;
                            (td.type_params.len(), md_ref, td.type_params.clone())
                        };

                        if arity != generics.len() {
                            tc_bail!(self, "E2307", Some(*span),
                                "trait `{}` expects {} type args, got {}", trait_name, arity, generics.len());
                        }

                        if !self.where_allows_trait_call(trait_name, generics)? {
                            tc_bail!(self, "E2308", Some(*span),
                                "cannot call `{0}::<{1}>::{2}` here; missing matching `where` bound",
                                trait_name,
                                generics.iter().map(show_ty).collect::<Vec<_>>().join(", "),
                                method);
                        }

                        // 形参替换：trait 形参 -> 本次提供的实参（其中可含 T/U）
                        let mut subst: Subst = Subst::default();
                        for (tp, ta) in tparams.iter().zip(generics.iter()) {
                            subst.insert(tp.clone(), ta.clone());
                        }
                        let want_params: Vec<Ty> = md.params.iter().map(|(_, t)| apply_subst(t, &subst)).collect();
                        let want_ret: Ty = apply_subst(&md.ret, &subst);

                        if want_params.len() != args.len() {
                            tc_bail!(self, "E2309", Some(*span),
                                "method `{}` expects {} args, got {}", method, want_params.len(), args.len());
                        }

                        // 预先计算参数类型（一次）
                        let mut arg_tys = Vec::with_capacity(args.len());
                        for a in args { arg_tys.push(self.expr(a)?); }

                        for (i, (at, want)) in arg_tys.iter().zip(want_params.iter()).enumerate() {
                            if *at == *want { continue; }
                            if self.literal_coerces_to(&args[i], want) { continue; }
                            tc_bail!(self, "E2310", Some(*span),
                                "arg#{i}: type mismatch: expect `{}`, got `{}`", show_ty(want), show_ty(at));
                        }
                        want_ret
                    }
                }
            },

            Expr::Block { block, .. } => self.block(block)?,
        })
    }

    // CHG: where 检查允许通过“调用者 where”满足不具体的情形
    fn check_where_bounds(&self, preds: &[WherePred], subst: &Subst) -> Result<()> {
        for wp in preds {
            let ty_i = apply_subst(&wp.ty, subst);
            // `ty_i` 可能仍含自由类型变量（比如由调用者的 T 替换进来），这里不强制具体
            for b in &wp.bounds {
                let ar = self.tenv.arity(&b.name).ok_or_else(|| anyhow!("unknown trait `{}` in where", b.name))?;
                // 按元数构造完整实参列表
                let all_args: Vec<Ty> = if b.args.is_empty() {
                    if ar != 1 { bail!("trait `{}` expects {} type args in where, got 0", b.name, ar); }
                    vec![ty_i.clone()]
                } else {
                    if b.args.len() != ar { bail!("trait `{}` expects {} type args in where, got {}", b.name, ar, b.args.len()); }
                    b.args.iter().map(|a| apply_subst(a, subst)).collect()
                };

                if all_args.iter().any(has_free_tyvar) {
                    // 不具体：要求“调用者 where”可满足
                    if !self.caller_where_satisfies(&b.name, &all_args) {
                        bail!("missing where bound for `{0}<{1}>` in current context",
                            b.name,
                            all_args.iter().map(show_ty).collect::<Vec<_>>().join(", "));
                    }
                } else {
                    // 具体：必须在全局 impl 表里有实例
                    if !self.ienv.has_impl(&b.name, &all_args) {
                        bail!("missing `impl {0}<...>` for `{0}<{1}>`",
                            b.name,
                            all_args.iter().map(show_ty).collect::<Vec<_>>().join(", "));
                    }
                }
            }
        }
        Ok(())
    }

    /* ============ 重载/泛型 解析核心 ============ */

    /// 根据名字 / 显式类型实参 / 实参表达式，解析“最佳候选”，并返回其实例化后的返回类型
    fn resolve_fun_call(
        &mut self,
        name: &str,
        generics: &[Ty],
        args: &[Expr],
        call_span: Option<Span>,
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

        // 评分候选：score 越小越优
        let mut viable: Vec<(usize /*score*/, Scheme, Subst, Vec<Ty> /*inst params*/, Ty /*inst ret*/)> = Vec::with_capacity(cands.len());

        'CAND: for sch in &cands {
            let mut subst: Subst = Subst::default();

            // 处理类型实参/推断
            if !sch.tparams.is_empty() {
                if !generics.is_empty() {
                    if generics.len() != sch.tparams.len() { continue; }
                    for (tp, ta) in sch.tparams.iter().zip(generics.iter()) {
                        // 允许 ta 中含自由类型变量（由调用者提供）
                        subst.insert(tp.clone(), ta.clone());
                    }
                } else {
                    // 从实参类型推断（严格统一）
                    for (at, pty) in arg_tys.iter().zip(sch.params.iter()) {
                        let mut s_local = subst.clone();
                        if let Err(_) = unify(pty, at, &mut s_local) { continue 'CAND; }
                        subst = s_local;
                    }
                }
            } else if !generics.is_empty() {
                continue;
            }

            // 实例化
            let inst_params: Vec<Ty> = sch.params.iter().map(|t| apply_subst(t, &subst)).collect();
            let inst_ret: Ty = apply_subst(&sch.ret, &subst);

            // where 检查（NEW: 允许由“调用者 where”满足不具体的约束）
            if let Err(_) = self.check_where_bounds(&sch.where_bounds, &subst) { continue; }

            // 参数检查 + 评分（严格匹配，但允许字面量受控放宽）
            let mut score: usize = 0;
            for (i, (at, want)) in arg_tys.iter().zip(inst_params.iter()).enumerate() {
                if has_free_tyvar(at) || has_free_tyvar(want) {
                    if *at != *want {
                        if !self.literal_coerces_to(&args[i], want) {
                            continue 'CAND;
                        }
                    }
                } else if at == want {
                    // exact：0 分
                } else if self.literal_coerces_to(&args[i], want) {
                    // 字面量 → 目标类型：允许
                } else {
                    continue 'CAND;
                }
            }
            if !sch.tparams.is_empty() { score += 1; } // 泛型候选稍降级

            viable.push((score, sch.clone(), subst, inst_params, inst_ret));
        }

        if viable.is_empty() {
            if self.fnscheme.contains_key(name) {
                tc_bail!(self, "E2311", call_span,
                    "no matching overload for `{}`{}",
                    name,
                    if generics.is_empty() {
                        " (try adding explicit type arguments, or ensure a proper where-bound/impl is in scope)"
                    } else { "" }
                );
            } else {
                tc_bail!(self, "E2312", call_span, "unknown function `{}`", name);
            }
        }

        viable.sort_by_key(|(sc, ..)| *sc);
        let best_score = viable[0].0;
        let n_best = viable.iter().take_while(|(sc, ..)| *sc == best_score).count();
        if n_best > 1 {
            tc_bail!(self, "E2313", call_span,
                "ambiguous call to `{}{}{}: {} candidates tie",
                name,
                if !generics.is_empty() { "<...>" } else { "" },
                if args.is_empty() { "()" } else { "(...)" },
                n_best
            );
        }

        let (_sc, _sch, _subst, _inst_params, inst_ret) = viable.remove(0);
        Ok(inst_ret)
    }

    /* ---------- 字面量辅助：Byte / Float / Double ---------- */

    /// 如果是整数字面量，取其值（用于常量范围判断）
    fn int_literal_value(&self, e: &Expr) -> Option<i128> {
        match e {
            Expr::Int  { value, .. } => Some(*value as i128),
            Expr::Long { value, .. } => Some(*value as i128),
            _ => None,
        }
    }

    /// “字面量是否可隐式收窄到 Byte(0..=255)”
    fn literal_fits_byte(&self, e: &Expr) -> bool {
        match self.int_literal_value(e) {
            Some(v) if v >= 0 && v <= 255 => true,
            _ => false,
        }
    }

    /// 是否是浮点字面量
    fn is_float_literal(&self, e: &Expr) -> bool {
        matches!(e, Expr::Float{..} | Expr::Double{..})
    }

    /// Double 字面量能否精确表示为 Float
    fn double_literal_exact_fits_f32(&self, e: &Expr) -> bool {
        if let Expr::Double { value: x, .. } = e {
            let v32 = *x as f32;
            (v32 as f64) == *x
        } else {
            false
        }
    }

    /// 通用放宽：字面量能否隐式“直接视作”目标类型
    fn literal_coerces_to(&self, expr: &Expr, dst: &Ty) -> bool {
        match dst {
            Ty::Byte   => self.literal_fits_byte(expr),
            Ty::Float  => matches!(expr, Expr::Float{..}) || self.double_literal_exact_fits_f32(expr),
            Ty::Double => self.is_float_literal(expr), // Float 或 Double 字面量都可提升到 Double
            _ => false,
        }
    }
}
