impl<'a> TyCk<'a> {
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

    #[inline] fn is_intish(&self, t: &Ty) -> bool {
        matches!(t, Ty::Byte | Ty::Int | Ty::Long | Ty::Char)
    }
    #[inline] fn is_floatish(&self, t: &Ty) -> bool {
        matches!(t, Ty::Float | Ty::Double)
    }
    #[inline] fn is_numeric_ty(&self, t: &Ty) -> bool {
        self.is_intish(t) || self.is_floatish(t)
    }
}