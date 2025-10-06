impl<'a> TyCk<'a> {
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

    // 供被调函数 where 检查使用——若不具体，允许通过调用者 where 满足
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

    // where 检查允许通过“调用者 where”满足不具体的情形
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
}