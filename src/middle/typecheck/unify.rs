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