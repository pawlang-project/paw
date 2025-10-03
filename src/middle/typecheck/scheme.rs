/* ===================== 多态函数签名（小向量优化） ===================== */

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Scheme {
    pub tparams: SmallVec4<String>,
    pub params: SmallVec4<Ty>,
    pub ret: Ty,
    pub where_bounds: SmallVec4<WherePred>,
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

fn collect_free_tyvars(t: &Ty, out: &mut FastSet<String>) {
    match t {
        Ty::Var(v) => { out.insert(v.clone()); }
        Ty::App { args, .. } => for a in args { collect_free_tyvars(a, out) },
        _ => {}
    }
}