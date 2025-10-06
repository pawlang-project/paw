impl CLBackend {
    fn has_tyvar(t: &Ty) -> bool {
        match t {
            Ty::Var(_) => true,
            Ty::App { args, .. } => args.iter().any(Self::has_tyvar),
            _ => false,
        }
    }

    fn collect_unique_tyvars(ts: &[Ty]) -> Vec<String> {
        let mut out: Vec<String> = Vec::new();
        fn walk(t: &Ty, seen: &mut Vec<String>) {
            match t {
                Ty::Var(v) => if !seen.iter().any(|x| x == v) { seen.push(v.clone()); },
                Ty::App { args, .. } => for a in args { walk(a, seen); },
                _ => {}
            }
        }
        for t in ts { walk(t, &mut out); }
        out
    }

    /* ---------------------------
     * 重载：类型编码 + 符号名mangle
     * --------------------------- */
    fn enc_ty(t: &Ty) -> String {
        match t {
            Ty::Byte   => "u8".into(),
            Ty::Int    => "i32".into(),
            Ty::Long   => "i64".into(),
            Ty::Bool   => "bool".into(),
            Ty::Char   => "char".into(),
            Ty::Float  => "f32".into(),
            Ty::Double => "f64".into(),
            Ty::String => "str".into(),
            Ty::Void   => "void".into(),
            Ty::App { name, args } => {
                let inner = if args.is_empty() { "unit".into() } else { Self::enc_ty(&args[0]) };
                match name.as_str() {
                    "Box" => format!("box_{}", inner),
                    "Rc"  => format!("rc_{}", inner),
                    "Arc" => format!("arc_{}", inner),
                    other => format!("app{}_{}", other, inner),
                }
            }
            Ty::Var(v) => format!("tv_{}", v),
        }
    }

    fn mangle_overload(base: &str, params: &[Ty], ret: &Ty) -> String {
        let p = params.iter().map(Self::enc_ty).collect::<Vec<_>>().join("_");
        let r = Self::enc_ty(ret);
        format!("{base}__ol__P{p}__R{r}")
    }

    #[inline]
    fn is_lowered_impl_name(name: &str) -> bool {
        name.starts_with("__impl_")
    }

    /// 在限定名调用处按需对 **泛型 impl 方法** 单态化并声明/定义
    pub fn ensure_impl_monomorph(&mut self, trait_name: &str, targs: &[Ty], method: &str) -> Result<(String, FuncId)> {
        let sym = mangle_impl_method(trait_name, targs, method);

        if let Some(&fid) = self.base_func_ids.get(&sym) {
            return Ok((sym, fid));
        }

        // 找模板
        let key = (trait_name.to_string(), method.to_string());
        let tpl = match self.impl_templates.get(&key) {
            Some(t) => t.clone(),
            None => {
                let msg = format!("unknown impl method template `{}::{}` for monomorph", trait_name, method);
                self.diag_err("CG0003", &msg);
                return Err(anyhow!(msg));
            }
        };

        if tpl.type_params.len() != targs.len() {
            let msg = format!(
                "impl `{}`::{} expects {} type args, got {}",
                trait_name, method, tpl.type_params.len(), targs.len()
            );
            self.diag_err("CG0004", &msg);
            return Err(anyhow!(msg));
        }

        // 建 substitution：trait 层类型变量 -> 调用时实参
        let subst = build_subst_map(&tpl.type_params, targs);

        // 计算方法参数/返回类型
        let mut param_tys = Vec::<Ty>::new();
        for (_, pty) in &tpl.params { param_tys.push(subst_ty(pty, &subst)); }
        let ret_ty = subst_ty(&tpl.ret, &subst);

        // 声明签名
        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for t in &param_tys {
            let ct = match cl_ty(t) {
                Some(t) => t,
                None => {
                    let msg = format!("impl monomorph param type not ABI-legal: {:?}", t);
                    self.diag_err("CG0002", &msg);
                    return Err(anyhow!(msg));
                }
            };
            sig.params.push(AbiParam::new(ct));
        }
        if let Some(rt) = cl_ty(&ret_ty) {
            sig.returns.push(AbiParam::new(rt));
        }

        let fid = self.module.declare_function(&sym, Linkage::Local, &sig)?;
        self.base_func_ids.insert(sym.clone(), fid);
        self.fn_sig.insert(sym.clone(), (param_tys.clone(), ret_ty.clone()));

        // 定义：把模板方法体做一次类型替换
        let body = subst_block(&tpl.body, &subst);
        let fdecl = FunDecl {
            name: sym.clone(),
            vis: Visibility::Private,
            type_params: vec![],
            params: tpl.params.clone().into_iter()
                .zip(param_tys.into_iter())
                .map(|((n,_), t)| (n, t)).collect(),
            ret: ret_ty,
            where_bounds: vec![],
            body,
            is_extern: false,
            span: Span::DUMMY,
        };
        self.define_fn_core(fid, &fdecl)?;

        Ok((sym, fid))
    }

    /// 按需单态化：用于 **隐式泛型调用** 的即时实例生成/定义（自由函数）
    pub fn ensure_monomorph(&mut self, base: &str, targs: &[Ty]) -> Result<(String, FuncId)> {
        let mname = mangle_name(base, targs);
        if let Some(&fid) = self.mono_func_ids.get(&mname) {
            if !self.mono_defined.contains(&mname) {
                let (b, ta) = self.mono_specs.get(&mname)
                    .cloned().unwrap_or((base.to_string(), targs.to_vec()));
                let tpl = match self.templates.get(&b) {
                    Some(t) => t,
                    None => {
                        let msg = format!("ensure_monomorph: template `{}` not found", b);
                        self.diag_err("CG0004", &msg);
                        return Err(anyhow!(msg));
                    }
                };
                let subst = build_subst_map(&tpl.type_params, &ta);
                let mf = specialize_fun(tpl, &subst, &mname)?;
                self.define_fn_core(fid, &mf)?;
                self.mono_defined.insert(mname.clone());
            }
            return Ok((mname, fid));
        }

        let tpl = match self.templates.get(base) {
            Some(t) => t,
            None => {
                let msg = format!("call `{}`<...> but no generic template found", base);
                self.diag_err("CG0004", &msg);
                return Err(anyhow!(msg));
            }
        };
        if tpl.type_params.len() != targs.len() {
            let msg = format!(
                "function `{}` expects {} type args, got {}",
                base, tpl.type_params.len(), targs.len()
            );
            self.diag_err("CG0004", &msg);
            return Err(anyhow!(msg));
        }

        let subst = build_subst_map(&tpl.type_params, targs);
        let mut param_tys = Vec::<Ty>::new();
        for (_, pty) in &tpl.params { param_tys.push(subst_ty(pty, &subst)); }
        let ret_ty = subst_ty(&tpl.ret, &subst);

        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for t in &param_tys {
            let ct = match cl_ty(t) {
                Some(t) => t,
                None => {
                    let msg = format!("monomorph param type not ABI-legal: {:?}", t);
                    self.diag_err("CG0002", &msg);
                    return Err(anyhow!(msg));
                }
            };
            sig.params.push(AbiParam::new(ct));
        }
        if let Some(rt) = cl_ty(&ret_ty) {
            sig.returns.push(AbiParam::new(rt));
        }

        let fid = self.module.declare_function(&mname, Linkage::Local, &sig)?;
        self.fn_sig.insert(mname.clone(), (param_tys.clone(), ret_ty.clone()));
        self.mono_func_ids.insert(mname.clone(), fid);
        self.mono_specs.insert(mname.clone(), (base.to_string(), targs.to_vec()));
        self.mono_declared.insert(mname.clone());

        let mf = specialize_fun(tpl, &subst, &mname)?;
        self.define_fn_core(fid, &mf)?;
        self.mono_defined.insert(mname.clone());

        Ok((mname, fid))
    }
}

/* ----------------------------------
 * ----- 单态化：替换工具 -----
 * ---------------------------------- */
fn build_subst_map(params: &[String], args: &[Ty]) -> FastMap<String, Ty> {
    let mut m = FastMap::default();
    for (p, a) in params.iter().zip(args.iter()) { m.insert(p.clone(), a.clone()); }
    m
}
fn subst_ty(t: &Ty, s: &FastMap<String, Ty>) -> Ty {
    match t {
        Ty::Var(n) => s.get(n).cloned().unwrap_or_else(|| Ty::Var(n.clone())),
        Ty::App { name, args } => Ty::App { name: name.clone(), args: args.iter().map(|x| subst_ty(x, s)).collect() },
        _ => t.clone(),
    }
}

fn subst_expr(e: &Expr, s: &FastMap<String, Ty>) -> Expr {
    match e {
        Expr::Call { callee, generics, args, span } => Expr::Call {
            callee: callee.clone(),
            generics: generics.iter().map(|t| subst_ty(t, s)).collect(),
            args: args.iter().map(|a| subst_expr(a, s)).collect(),
            span: *span,
        },
        Expr::Unary { op, rhs, span } =>
            Expr::Unary { op: *op, rhs: Box::new(subst_expr(rhs, s)), span: *span },
        Expr::Binary { op, lhs, rhs, span } =>
            Expr::Binary { op: *op, lhs: Box::new(subst_expr(lhs, s)), rhs: Box::new(subst_expr(rhs, s)), span: *span },
        Expr::If { cond, then_b, else_b, span } =>
            Expr::If { cond: Box::new(subst_expr(cond, s)), then_b: subst_block(then_b, s), else_b: subst_block(else_b, s), span: *span },
        Expr::Match { scrut, arms, default, span } => {
            let scr = Box::new(subst_expr(scrut, s));
            let mut new_arms = Vec::with_capacity(arms.len());
            for (pat, blk) in arms { new_arms.push((pat.clone(), subst_block(blk, s))); }
            let def = default.as_ref().map(|b| subst_block(b, s));
            Expr::Match { scrut: scr, arms: new_arms, default: def, span: *span }
        }
        Expr::Cast { expr, ty, span } =>
            Expr::Cast { expr: Box::new(subst_expr(expr, s)), ty: subst_ty(ty, s), span: *span },
        Expr::Block { block, span } =>
            Expr::Block { block: subst_block(block, s), span: *span },
        _ => e.clone(),
    }
}
fn subst_stmt(st: &Stmt, s: &FastMap<String, Ty>) -> Stmt {
    match st {
        Stmt::Let { name, ty, init, is_const, span } =>
            Stmt::Let { name: name.clone(), ty: subst_ty(ty, s), init: subst_expr(init, s), is_const: *is_const, span: *span },
        Stmt::Assign { name, expr, span } =>
            Stmt::Assign { name: name.clone(), expr: subst_expr(expr, s), span: *span },
        Stmt::While { cond, body, span } =>
            Stmt::While { cond: subst_expr(cond, s), body: subst_block(body, s), span: *span },
        Stmt::For { init, cond, step, body, span } => Stmt::For {
            init: init.as_ref().map(|fi| match fi {
                ForInit::Let { name, ty, init, is_const, span } =>
                    ForInit::Let { name: name.clone(), ty: subst_ty(ty, s), init: subst_expr(init, s), is_const: *is_const, span: *span },
                ForInit::Assign { name, expr, span } =>
                    ForInit::Assign { name: name.clone(), expr: subst_expr(expr, s), span: *span },
                ForInit::Expr(e, sp) => ForInit::Expr(subst_expr(e, s), *sp),
            } ),
            cond: cond.as_ref().map(|c| subst_expr(c, s)),
            step: step.as_ref().map(|stp| match stp {
                ForStep::Assign { name, expr, span } =>
                    ForStep::Assign { name: name.clone(), expr: subst_expr(expr, s), span: *span },
                ForStep::Expr(e, sp) => ForStep::Expr(subst_expr(e, s), *sp),
            } ),
            body: subst_block(body, s),
            span: *span,
        },
        Stmt::If { cond, then_b, else_b, span } =>
            Stmt::If { cond: subst_expr(cond, s), then_b: subst_block(then_b, s), else_b: else_b.as_ref().map(|b| subst_block(b, s)), span: *span },
        Stmt::Expr { expr, span } => Stmt::Expr { expr: subst_expr(expr, s), span: *span },
        Stmt::Return { expr, span } => Stmt::Return { expr: expr.as_ref().map(|x| subst_expr(x, s)), span: *span },
        Stmt::Break { .. } | Stmt::Continue { .. } => st.clone(),
    }
}
fn subst_block(b: &Block, s: &FastMap<String, Ty>) -> Block {
    Block {
        stmts: b.stmts.iter().map(|st| subst_stmt(st, s)).collect(),
        tail:  b.tail.as_ref().map(|e| Box::new(subst_expr(e, s))),
        span:  b.span,
    }
}
fn specialize_fun(tpl: &FunDecl, subst: &FastMap<String, Ty>, mono_name: &str) -> Result<FunDecl> {
    let mut params = Vec::with_capacity(tpl.params.len());
    for (n, t) in &tpl.params {
        let nt = subst_ty(t, subst);
        if cl_ty(&nt).is_none() { return Err(anyhow!("monomorph param type not concrete/ABI-legal in `{}`: {:?}", mono_name, nt)); }
        params.push((n.clone(), nt));
    }
    let ret = subst_ty(&tpl.ret, subst);
    if !matches!(ret, Ty::Void) && cl_ty(&ret).is_none() { return Err(anyhow!("monomorph return type not concrete/ABI-legal in `{}`: {:?}", mono_name, ret)); }
    let body = subst_block(&tpl.body, subst);
    Ok(FunDecl {
        name: mono_name.to_string(),
        vis: Visibility::Private,
        type_params: vec![],
        params,
        ret,
        where_bounds: vec![],
        body,
        is_extern: false,
        span: Span::DUMMY,
    })
}

/* ----------------------------------
 * ----- 遍历：收集泛型调用 -----
 * ---------------------------------- */
fn collect_calls_with_generics_in_program(p: &Program, out: &mut Vec<(String, Vec<Ty>)>) {
    for it in &p.items {
        match it {
            Item::Fun(f, _) => collect_calls_in_block(&f.body, out),
            Item::Global { init, .. } => collect_calls_in_expr(init, out),
            Item::Import(_, _) => {}
            Item::Trait(_, _) | Item::Impl(_, _) | Item::Struct(_, _) => {}
        }
    }
}
fn collect_calls_in_block(b: &Block, out: &mut Vec<(String, Vec<Ty>)>) {
    for s in &b.stmts {
        match s {
            Stmt::Let { init, .. } => collect_calls_in_expr(init, out),
            Stmt::Assign { expr, .. } => collect_calls_in_expr(expr, out),
            Stmt::While { cond, body, .. } => { collect_calls_in_expr(cond, out); collect_calls_in_block(body, out); }
            Stmt::For { init, cond, step, body, .. } => {
                if let Some(fi) = init {
                    match fi {
                        ForInit::Let { init, .. } => collect_calls_in_expr(init, out),
                        ForInit::Assign { expr, .. } => collect_calls_in_expr(expr, out),
                        ForInit::Expr(e, ..) => collect_calls_in_expr(e, out),
                    }
                }
                if let Some(c) = cond { collect_calls_in_expr(c, out); }
                if let Some(st) = step {
                    match st {
                        ForStep::Assign { expr, .. } => collect_calls_in_expr(expr, out),
                        ForStep::Expr(e, ..) => collect_calls_in_expr(e, out),
                    }
                }
                collect_calls_in_block(body, out);
            }
            Stmt::If { cond, then_b, else_b, .. } => {
                collect_calls_in_expr(cond, out);
                collect_calls_in_block(then_b, out);
                if let Some(b) = else_b { collect_calls_in_block(b, out); }
            }
            Stmt::Expr { expr, .. } => collect_calls_in_expr(expr, out),
            Stmt::Return { expr: opt, .. } => if let Some(e) = opt { collect_calls_in_expr(e, out); },
            Stmt::Break { .. } | Stmt::Continue { .. } => {}
        }
    }
    if let Some(t) = &b.tail { collect_calls_in_expr(t, out); }
}
fn collect_calls_in_expr(e: &Expr, out: &mut Vec<(String, Vec<Ty>)>) {
    match e {
        Expr::Call { callee, generics, args, .. } => {
            if !generics.is_empty() {
                if let Callee::Name(nm) = callee { out.push((nm.clone(), generics.clone())); }
                // 限定名调用（impl 方法）按需实例化，这里不用收集
            }
            for a in args { collect_calls_in_expr(a, out); }
        }
        Expr::Unary { rhs, .. } => collect_calls_in_expr(rhs, out),
        Expr::Binary { lhs, rhs, .. } => { collect_calls_in_expr(lhs, out); collect_calls_in_expr(rhs, out); }
        Expr::If { cond, then_b, else_b, .. } => {
            collect_calls_in_expr(cond, out); collect_calls_in_block(then_b, out); collect_calls_in_block(else_b, out);
        }
        Expr::Match { scrut, arms, default, .. } => {
            collect_calls_in_expr(scrut, out);
            for (_, b) in arms { collect_calls_in_block(b, out); }
            if let Some(b) = default { collect_calls_in_block(b, out); }
        }
        Expr::Cast { expr, .. } => collect_calls_in_expr(expr, out),
        Expr::Block { block, .. } => collect_calls_in_block(block, out),
        _ => {}
    }
}