impl CLBackend {
    /// 声明**非泛型/extern**函数；保存**泛型模板**；对非泛型自由函数进行**重载mangle**。
    /// 特殊：名字恰为 "main" 的非泛型函数不做重载 mangle；名字以 "__impl_" 开头的**保持原名**。
    pub fn declare_fns<'a, I>(&mut self, funs: I) -> anyhow::Result<FastMap<String, FuncId>>
    where
        I: IntoIterator<Item = &'a FunDecl>,
    {
        let mut ids: FastMap<String, FuncId> = FastMap::default();

        for f in funs {
            if !f.type_params.is_empty() {
                // 保存模板
                self.templates.insert(f.name.clone(), f.clone());
                continue;
            }

            // 构建CLIF签名
            let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
            for (_, ty) in &f.params {
                let t = match cl_ty(ty) {
                    Some(t) => t,
                    None => {
                        let msg = format!("param type `{:?}` cannot be used in ABI", ty);
                        self.diag_err("CG0002", &msg);
                        return Err(anyhow!(msg));
                    }
                };
                sig.params.push(AbiParam::new(t));
            }
            if let Some(ret_t) = cl_ty(&f.ret) {
                sig.returns.push(AbiParam::new(ret_t));
            }

            // 链接属性 & 目标符号名
            let (linkage, sym) = if f.is_extern {
                (Linkage::Import, f.name.clone())
            } else if f.name == "main" {
                (Linkage::Export, "main".to_string())
            } else if Self::is_lowered_impl_name(&f.name) {
                // 已由 passes 降解的 impl 自由函数：保持原名，不做重载 mangle
                (Linkage::Local, f.name.clone())
            } else {
                let sym = Self::mangle_overload(
                    &f.name,
                    &f.params.iter().map(|(_,t)| t.clone()).collect::<Vec<_>>(),
                    &f.ret
                );
                (match f.vis {
                    Visibility::Public  => Linkage::Export,
                    Visibility::Private => Linkage::Local,
                }, sym)
            };

            // 去重 & 声明
            if !self.declared_symbols.insert(sym.clone()) {
                continue;
            }
            let id = self.module.declare_function(&sym, linkage, &sig)?;
            ids.insert(sym.clone(), id);
            self.base_func_ids.insert(sym.clone(), id);

            // 记录签名
            self.fn_sig.insert(
                sym.clone(),
                (f.params.iter().map(|(_, t)| t.clone()).collect(), f.ret.clone()),
            );

            // 记录重载集合（main / extern / 已降解 impl 不加入）
            if !f.is_extern && f.name != "main" && !Self::is_lowered_impl_name(&f.name) {
                self.overloads.entry(f.name.clone()).or_default().push(sym.clone());
            }
        }

        Ok(ids)
    }

    /// 扫描程序，声明所有 impl 方法（作为普通函数符号）。
    /// 对 trait 实参**全为具体类型**的 impl：直接声明为具体符号；
    /// 对**含类型变量**的 impl：只登记模板（不声明具体符号），调用时按需单态化。
    pub fn declare_impls_from_program(&mut self, prog: &Program) -> Result<()> {
        for it in &prog.items {
            if let Item::Impl(id, _) = it {
                let is_generic_impl = id.trait_args.iter().any(Self::has_tyvar);
                if is_generic_impl {
                    // 记录成模板
                    let tvars = Self::collect_unique_tyvars(&id.trait_args);
                    for item in &id.items {
                        let m = match item {
                            ImplItem::Method(m) => m,
                            ImplItem::ExternMethod(_) => continue,
                            ImplItem::AssocType(_) => continue,
                        };
                        let key = (id.trait_name.clone(), m.name.clone());
                        self.impl_templates.insert(key, ImplMethodTpl {
                            trait_name: id.trait_name.clone(),
                            type_params: tvars.clone(),
                            trait_args_tpl: id.trait_args.clone(),
                            method_name: m.name.clone(),
                            params: m.params.clone(),
                            ret: m.ret.clone(),
                            body: m.body.clone(),
                        });
                    }
                    continue;
                }

                // 非泛型 impl：直接声明具体符号
                for item in &id.items {
                    let m = match item {
                        ImplItem::Method(m) => m,
                        ImplItem::ExternMethod(_) => continue,
                        ImplItem::AssocType(_) => continue,
                    };

                    let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);
                    if self.base_func_ids.contains_key(&sym) {
                        continue;
                    }

                    let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
                    for (_, ty) in &m.params {
                        let t = match cl_ty(ty) {
                            Some(t) => t,
                            None => {
                                let msg = format!("impl method param type not ABI-legal: {:?}", ty);
                                self.diag_err("CG0002", &msg);
                                return Err(anyhow!(msg));
                            }
                        };
                        sig.params.push(AbiParam::new(t));
                    }
                    if let Some(rt) = cl_ty(&m.ret) {
                        sig.returns.push(AbiParam::new(rt));
                    }

                    let fid = self.module.declare_function(&sym, Linkage::Local, &sig)?;
                    self.base_func_ids.insert(sym.clone(), fid);
                    self.fn_sig.insert(
                        sym.clone(),
                        (m.params.iter().map(|(_, t)| t.clone()).collect(), m.ret.clone())
                    );
                }
            }
        }
        Ok(())
    }

    /// 收集 struct 模板（字段与类型形参），供后端布局
    pub fn declare_structs_from_program(&mut self, prog: &Program) -> Result<()> {
        for it in &prog.items {
            if let Item::Struct(sd, _sp) = it {
                let tpl = StructTpl { type_params: sd.type_params.clone(), fields: sd.fields.clone() };
                if self.struct_templates.insert(sd.name.clone(), tpl).is_some() {
                    let msg = format!("duplicate struct `{}` in codegen declare", sd.name);
                    self.diag_err("CG0001", &msg);
                    return Err(anyhow!(msg));
                }
            }
        }
        Ok(())
    }

    /// 扫描程序并声明所有**带显式类型实参**的专门化符号（自由函数）
    pub fn declare_mono_from_program(&mut self, prog: &Program) -> Result<()> {
        let mut calls: Vec<(String, Vec<Ty>)> = Vec::new();
        collect_calls_with_generics_in_program(prog, &mut calls);

        for (base, targs) in calls {
            if targs.is_empty() { continue; }
            let templ = match self.templates.get(&base) {
                Some(t) => t,
                None => {
                    let msg = format!("call `{}`<...> but no generic template found", base);
                    self.diag_err("CG0004", &msg);
                    return Err(anyhow!(msg));
                }
            };
            if templ.type_params.len() != targs.len() {
                let msg = format!(
                    "function `{}` expects {} type args, got {}",
                    base, templ.type_params.len(), targs.len()
                );
                self.diag_err("CG0004", &msg);
                return Err(anyhow!(msg));
            }

            let mname = mangle_name(&base, &targs);
            if !self.mono_declared.insert(mname.clone()) {
                continue;
            }

            let subst = build_subst_map(&templ.type_params, &targs);
            let mut param_tys = Vec::<Ty>::new();
            for (_, pty) in &templ.params { param_tys.push(subst_ty(pty, &subst)); }
            let ret_ty = subst_ty(&templ.ret, &subst);

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
            self.mono_specs.insert(mname.clone(), (base.clone(), targs.clone()));
        }
        Ok(())
    }
}