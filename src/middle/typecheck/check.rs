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

    fn is_const_name(&self, n: &str) -> bool {
        for cset in self.locals_const.iter().rev() {
            if cset.contains(n) { return true; }
        }
        self.globals_const.contains(n)
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
}

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