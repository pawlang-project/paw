impl CLBackend {
    /// 核心定义：根据 FunDecl 生成 CLIF 并 define
    fn define_fn_core(&mut self, fid: FuncId, f: &FunDecl) -> Result<()> {
        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for (_, ty) in &f.params {
            let t = match cl_ty(ty) {
                Some(t) => t,
                None => {
                    let msg = format!("invalid param ty {:?}", ty);
                    self.diag_err("CG0002", &msg);
                    return Err(anyhow!(msg));
                }
            };
            sig.params.push(AbiParam::new(t));
        }
        if let Some(ret_t) = cl_ty(&f.ret) {
            sig.returns.push(AbiParam::new(ret_t));
        }

        let mut ctx = self.module.make_context();
        ctx.func.signature = sig;
        let mut fb_ctx = FunctionBuilderContext::new();
        let mut b = FunctionBuilder::new(&mut ctx.func, &mut fb_ctx);

        let entry = b.create_block();
        b.append_block_params_for_function_params(entry);
        b.switch_to_block(entry);
        b.seal_block(entry);

        let mut scopes: Vec<FastMap<String, Variable>> = vec![FastMap::default()];
        let mut scopes_ty: Vec<FastMap<String, Ty>>     = vec![FastMap::default()];

        for (i, (name, ty)) in f.params.iter().enumerate() {
            let v = b.declare_var(cl_ty(ty).unwrap());
            let arg = b.block_params(entry)[i];
            b.def_var(v, arg);
            scopes.last_mut().unwrap().insert(name.clone(), v);
            scopes_ty.last_mut().unwrap().insert(name.clone(), ty.clone());
        }

        // 可内联全局常量
        {
            let scope_vars  = scopes.last_mut().unwrap();
            let scope_types = scopes_ty.last_mut().unwrap();
            for (gname, (gty, gval)) in &self.globals_val {
                let v = b.declare_var(cl_ty(gty).unwrap());
                let c = match (gty, gval) {
                    (Ty::Byte,  GConst::I8(n))  => b.ins().iconst(types::I8,  *n as i64),
                    (Ty::Int,   GConst::I32(n)) => b.ins().iconst(types::I32, *n as i64),
                    (Ty::Long,  GConst::I64(n)) => b.ins().iconst(types::I64, *n),
                    (Ty::Bool,  GConst::I8(n))  => b.ins().iconst(types::I8,  *n as i64),
                    (Ty::Char,  GConst::I32(n)) => b.ins().iconst(types::I32, *n as i64),
                    (Ty::Float, GConst::F32(x)) => b.ins().f32const(*x),
                    (Ty::Double,GConst::F64(x)) => b.ins().f64const(*x),
                    (Ty::String, _) | (Ty::Void, _) => unreachable!(),
                    _ => unreachable!(),
                };
                b.def_var(v, c);
                scope_vars.insert(gname.clone(), v);
                scope_types.insert(gname.clone(), gty.clone());
            }
        }

        let mut cg = ExprGen {
            be: self,
            scopes,
            scopes_ty,
            loop_stack: Vec::new(),
            fun_ret: f.ret.clone(),
        };

        let ret_val = cg.emit_block(&mut b, &f.body)?;

        let ret_ty = cg.fun_ret.clone();
        if matches!(ret_ty, Ty::Void) {
            b.ins().return_(&[]);
        } else {
            let dummy0 = Expr::Int { value: 0, span: Span::DUMMY };
            let src_expr = f.body.tail.as_deref().unwrap_or(&dummy0);
            let final_v = cg.coerce_for_dst_from_expr(&mut b, src_expr, ret_val, &ret_ty)?;
            b.ins().return_(&[final_v]);
        }

        b.finalize();

        self.module.define_function(fid, &mut ctx)?;
        self.module.clear_context(&mut ctx);
        Ok(())
    }

    /// 定义**非泛型**函数体（注意：按与 declare_fns 相同的取名规则；main/extern/__impl_* 例外）
    pub fn define_fn(&mut self, f: &FunDecl, ids: &FastMap<String, FuncId>) -> Result<()> {
        if f.is_extern { return Ok(()); }
        if !f.type_params.is_empty() { return Ok(()); }

        let sym = if f.is_extern {
            f.name.clone()
        } else if f.name == "main" {
            "main".to_string()
        } else if Self::is_lowered_impl_name(&f.name) {
            f.name.clone()
        } else {
            Self::mangle_overload(
                &f.name,
                &f.params.iter().map(|(_,t)| t.clone()).collect::<Vec<_>>(),
                &f.ret
            )
        };

        let fid = match ids.get(&sym) {
            Some(id) => *id,
            None => {
                let msg = format!("missing FuncId for `{}`", sym);
                self.diag_err("CG0001", &msg);
                return Err(anyhow!(msg));
            }
        };
        self.define_fn_core(fid, f)
    }

    /// 定义所有 impl 方法体（若 passes 已降解为自由函数则跳过；对非泛型 impl）
    pub fn define_impls_from_program(&mut self, prog: &Program) -> Result<()> {
        let mut fun_names = FastSet::<String>::default();
        for it in &prog.items {
            if let Item::Fun(f, _) = it { fun_names.insert(f.name.clone()); }
        }

        for it in &prog.items {
            if let Item::Impl(id, _) = it {
                // 泛型 impl 的定义由 ensure_impl_monomorph 按需完成，这里只处理非泛型 impl
                if id.trait_args.iter().any(Self::has_tyvar) { continue; }

                for item in &id.items {
                    let m = match item {
                        ImplItem::Method(m) => m,
                        ImplItem::AssocType(_) => continue,
                    };

                    let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);

                    if fun_names.contains(&sym) {
                        continue;
                    }

                    let fid = match self.base_func_ids.get(&sym) {
                        Some(fid) => *fid,
                        None => {
                            let msg = format!("define_impls: missing FuncId for `{}`", sym);
                            self.diag_err("CG0003", &msg);
                            return Err(anyhow!(msg));
                        }
                    };

                    let fdecl = FunDecl {
                        name: sym.clone(),
                        vis: Visibility::Private,
                        type_params: vec![],
                        params: m.params.clone(),
                        ret: m.ret.clone(),
                        where_bounds: vec![],
                        body: m.body.clone(),
                        is_extern: false,
                        span: Span::DUMMY,
                    };
                    self.define_fn_core(fid, &fdecl)?;
                }
            }
        }
        Ok(())
    }

    /// 定义所有已声明的**专门化**函数体（自由函数）
    pub fn define_mono_from_program(&mut self, _prog: &Program, _base_ids: &FastMap<String, FuncId>) -> Result<()> {
        let specs: Vec<(String, (String, Vec<Ty>))> =
            self.mono_specs.iter().map(|(k, v)| (k.clone(), v.clone())).collect();

        for (mname, (base, targs)) in specs {
            if !self.mono_defined.insert(mname.clone()) { continue; }
            let templ = match self.templates.get(&base) {
                Some(t) => t,
                None => {
                    let msg = format!("monomorph define: template `{}` not found", base);
                    self.diag_err("CG0004", &msg);
                    return Err(anyhow!(msg));
                }
            };
            if templ.type_params.len() != targs.len() {
                let msg = format!("template `{}` type params mismatch for `{}`", base, mname);
                self.diag_err("CG0004", &msg);
                return Err(anyhow!(msg));
            }
            let fid = match self.mono_func_ids.get(&mname) {
                Some(fid) => *fid,
                None => {
                    let msg = format!("monomorph define: missing FuncId for `{}`", mname);
                    self.diag_err("CG0001", &msg);
                    return Err(anyhow!(msg));
                }
            };

            let subst = build_subst_map(&templ.type_params, &targs);
            let mono_fun = specialize_fun(templ, &subst, &mname)?;
            self.define_fn_core(fid, &mono_fun)?;
        }
        Ok(())
    }

    pub fn finish(self) -> Result<Vec<u8>> {
        let obj = self.module.finish();
        Ok(obj.emit()?)
    }
}

pub fn compile_program(prog: &Program, diag: Option<Rc<RefCell<DiagSink>>>, file_names: Vec<String>) -> Result<Vec<u8>> {
    let mut be = CLBackend::new()?;
    if let Some(d) = diag { be.set_diag(d); }
    // 提供 file_id -> 显示名 映射，便于用 span.file 定位真实文件名
    be.set_file_names(file_names);
    be.set_globals_from_program(prog);

    // 收集 struct 模板（为布局做准备）
    be.declare_structs_from_program(prog)?;

    // 1) 自由函数（含 extern、非泛型重载；main 保持原名；__impl_* 保持原名）声明
    let funs: Vec<&FunDecl> = prog.items.iter().filter_map(|it| {
        if let Item::Fun(f, _) = it { Some(f) } else { None }
    }).collect();
    let base_ids = be.declare_fns(funs.clone())?;

    // 2) impl 方法：声明非泛型具体符号 + 记录泛型模板
    be.declare_impls_from_program(prog)?;

    // 3) 收集并声明所有需要的单态化实例（自由函数，显式 `<...>`）
    be.declare_mono_from_program(prog)?;

    // 4) 定义自由函数（非泛型；使用与声明一致的命名规则）
    for f in &funs { be.define_fn(f, &base_ids)?; }

    // 5) 定义非泛型 impl 方法（泛型 impl 由调用时 ensure_impl_monomorph 定义）
    be.define_impls_from_program(prog)?;

    // 6) 定义单态化实例（自由函数）
    be.define_mono_from_program(prog, &base_ids)?;

    // 7) 输出 obj
    be.finish()
}