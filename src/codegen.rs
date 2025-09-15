use crate::ast::*;
use crate::mangle::{mangle_name, mangle_impl_method}; // 统一使用全局搅拌
use anyhow::{anyhow, Result, bail};
use cranelift_codegen::{
    ir::{self, types, AbiParam, InstBuilder},
    ir::condcodes::{FloatCC, IntCC},
    settings,
};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{default_libcall_names, DataDescription as DataContext, DataId, FuncId, Linkage, Module};
use cranelift_object::{ObjectBuilder, ObjectModule};
use cranelift_native;
use std::collections::{HashMap, HashSet};
use cranelift_codegen::settings::{Configurable, Flags};

/* ---------- 可内联注入的全局常量 ---------- */
#[derive(Clone, Debug)]
pub enum GConst {
    I8(u8),
    I32(i32),
    I64(i64),
    F32(f32),
    F64(f64),
}

pub struct CLBackend {
    pub module: ObjectModule,

    // name -> (Ty, const)
    pub globals_val: HashMap<String, (Ty, GConst)>,
    str_pool: HashMap<String, DataId>,

    // 符号名 -> (参数类型列表, 返回类型)
    fn_sig: HashMap<String, (Vec<Ty>, Ty)>,

    // —— 单态化管理 —— //
    templates: HashMap<String, FunDecl>,              // 基名 -> 模板
    mono_declared: HashSet<String>,                   // 已声明专门化符号
    mono_defined: HashSet<String>,                    // 已定义专门化符号
    mono_func_ids: HashMap<String, FuncId>,           // 专门化名 -> FuncId
    mono_specs: HashMap<String, (String, Vec<Ty>)>,   // 专门化名 -> (基名, 类型实参)

    // —— 基名函数缓存（修复调用处重新声明的问题） —— //
    base_func_ids: HashMap<String, FuncId>,           // 基名或具体符号 -> FuncId
}

impl CLBackend {
    pub fn new() -> Result<Self> {
        let mut fbuilder = settings::builder();
        fbuilder.set("is_pic", "true")?;

        let flags = Flags::new(fbuilder);

        let isa = cranelift_native::builder()
            .map_err(|e| anyhow!("cranelift_native::builder(): {e}"))?
            .finish(flags)
            .map_err(|e| anyhow!("finish ISA: {e}"))?;

        let obj = ObjectBuilder::new(isa, "paw_obj".to_string(), default_libcall_names())?;
        Ok(Self {
            module: ObjectModule::new(obj),
            globals_val: HashMap::new(),
            str_pool: HashMap::new(),
            fn_sig: HashMap::new(),
            templates: HashMap::new(),
            mono_declared: HashSet::new(),
            mono_defined: HashSet::new(),
            mono_func_ids: HashMap::new(),
            mono_specs: HashMap::new(),
            base_func_ids: HashMap::new(),
        })
    }

    pub fn set_globals_from_program(&mut self, prog: &Program) {
        self.globals_val.clear();
        for it in &prog.items {
            if let Item::Global { name, ty, init, is_const } = it {
                if !*is_const { continue; }
                match (ty, init) {
                    (&Ty::Int,    &Expr::Int(n))    => { self.globals_val.insert(name.clone(), (Ty::Int,    GConst::I32(n))); }
                    (&Ty::Long,   &Expr::Long(n))   => { self.globals_val.insert(name.clone(), (Ty::Long,   GConst::I64(n))); }
                    (&Ty::Long,   &Expr::Int(n))    => { self.globals_val.insert(name.clone(), (Ty::Long,   GConst::I64(n as i64))); }
                    (&Ty::Bool,   &Expr::Bool(b))   => { self.globals_val.insert(name.clone(), (Ty::Bool,   GConst::I8(if b {1} else {0}))); }
                    (&Ty::Char,   &Expr::Char(u))   => { self.globals_val.insert(name.clone(), (Ty::Char,   GConst::I32(u as i32))); }
                    (&Ty::Float,  &Expr::Float(x))  => { self.globals_val.insert(name.clone(), (Ty::Float,  GConst::F32(x))); }
                    (&Ty::Double, &Expr::Double(x)) => { self.globals_val.insert(name.clone(), (Ty::Double, GConst::F64(x))); }
                    (&Ty::String, &Expr::Str(_))    => {}
                    _ => {}
                }
            }
        }
    }

    /// 声明**非泛型/extern**函数；保存**泛型模板**
    pub fn declare_fns<'a, I>(&mut self, funs: I) -> anyhow::Result<HashMap<String, FuncId>>
    where
        I: IntoIterator<Item = &'a FunDecl>,
    {
        use cranelift_codegen::settings::Configurable;
        let mut ids = HashMap::new();

        for f in funs {
            if !f.type_params.is_empty() {
                // 保存模板
                self.templates.insert(f.name.clone(), f.clone());
                continue;
            }

            // 非泛型/extern 声明（以下与原逻辑相同）
            let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
            for (_, ty) in &f.params {
                let t = cl_ty(ty).ok_or_else(|| anyhow!("param type `{:?}` cannot be used in ABI", ty))?;
                sig.params.push(AbiParam::new(t));
            }
            if let Some(ret_t) = cl_ty(&f.ret) {
                sig.returns.push(AbiParam::new(ret_t));
            }
            let linkage = if f.is_extern { Linkage::Import } else { Linkage::Export };
            let id = self.module.declare_function(&f.name, linkage, &sig)?;
            ids.insert(f.name.clone(), id);
            self.base_func_ids.insert(f.name.clone(), id);

            self.fn_sig.insert(
                f.name.clone(),
                (f.params.iter().map(|(_, t)| t.clone()).collect(), f.ret.clone()),
            );
        }

        Ok(ids)
    }

    /// 扫描程序，声明所有 impl 方法（作为普通函数符号）
    /// 幂等：若 passes 已经把 impl 方法降解为 `FunDecl` 并被 declare_fns 处理过，这里直接跳过。
    pub fn declare_impls_from_program(&mut self, prog: &Program) -> Result<()> {
        for it in &prog.items {
            if let Item::Impl(id) = it {
                for m in &id.items {
                    let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);

                    // 已存在（可能是降解后的自由函数或之前声明过），跳过
                    if self.base_func_ids.contains_key(&sym) || self.base_func_ids.contains_key(&sym) {
                        continue;
                    }

                    let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
                    for (_, ty) in &m.params {
                        let t = cl_ty(ty).ok_or_else(|| anyhow!("impl method param type not ABI-legal: {:?}", ty))?;
                        sig.params.push(AbiParam::new(t));
                    }
                    if let Some(rt) = cl_ty(&m.ret) {
                        sig.returns.push(AbiParam::new(rt));
                    }

                    // 对外可见与否均可，这里用 Local
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

    /// 扫描程序并声明所有**带显式类型实参**的专门化符号
    pub fn declare_mono_from_program(&mut self, prog: &Program) -> Result<()> {
        let mut calls: Vec<(String, Vec<Ty>)> = Vec::new();
        collect_calls_with_generics_in_program(prog, &mut calls);

        for (base, targs) in calls {
            if targs.is_empty() { continue; }
            let templ = self.templates.get(&base)
                .ok_or_else(|| anyhow!("call `{}`<...> but no generic template found", base))?;
            if templ.type_params.len() != targs.len() {
                return Err(anyhow!(
                    "function `{}` expects {} type args, got {}",
                    base, templ.type_params.len(), targs.len()
                ));
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
                let ct = cl_ty(t).ok_or_else(|| anyhow!("monomorph param type not ABI-legal: {:?}", t))?;
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

    /// 按需单态化：用于 **隐式泛型调用** 的即时实例生成/定义
    pub fn ensure_monomorph(&mut self, base: &str, targs: &[Ty]) -> Result<(String, FuncId)> {
        let mname = mangle_name(base, targs);
        if let Some(&fid) = self.mono_func_ids.get(&mname) {
            // 已声明；若未定义则补定义
            if !self.mono_defined.contains(&mname) {
                let (b, ta) = self.mono_specs.get(&mname)
                    .cloned().unwrap_or((base.to_string(), targs.to_vec()));
                let tpl = self.templates.get(&b)
                    .ok_or_else(|| anyhow!("ensure_monomorph: template `{}` not found", b))?;
                let subst = build_subst_map(&tpl.type_params, &ta);
                let mf = specialize_fun(tpl, &subst, &mname)?;
                self.define_fn_core(fid, &mf)?;
                self.mono_defined.insert(mname.clone());
            }
            return Ok((mname, fid));
        }

        // 首次：声明 + 立刻定义
        let tpl = self.templates.get(base)
            .ok_or_else(|| anyhow!("call `{}`<...> but no generic template found", base))?;
        if tpl.type_params.len() != targs.len() {
            return Err(anyhow!(
                "function `{}` expects {} type args, got {}",
                base, tpl.type_params.len(), targs.len()
            ));
        }

        let subst = build_subst_map(&tpl.type_params, targs);
        let mut param_tys = Vec::<Ty>::new();
        for (_, pty) in &tpl.params { param_tys.push(subst_ty(pty, &subst)); }
        let ret_ty = subst_ty(&tpl.ret, &subst);

        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for t in &param_tys {
            let ct = cl_ty(t).ok_or_else(|| anyhow!("monomorph param type not ABI-legal: {:?}", t))?;
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

    /// 定义**非泛型**函数体
    pub fn define_fn(&mut self, f: &FunDecl, ids: &HashMap<String, FuncId>) -> Result<()> {
        if f.is_extern { return Ok(()); }
        if !f.type_params.is_empty() { return Ok(()); }
        let fid = *ids.get(&f.name).ok_or_else(|| anyhow!("missing FuncId for `{}`", f.name))?;
        self.define_fn_core(fid, f)
    }

    /// 定义所有 impl 方法体（若 passes 已降解为自由函数则跳过）
    pub fn define_impls_from_program(&mut self, prog: &Program) -> Result<()> {
        // 收集 Program 里已有的自由函数名
        let mut fun_names = HashSet::<String>::new();
        for it in &prog.items {
            if let Item::Fun(f) = it { fun_names.insert(f.name.clone()); }
        }

        for it in &prog.items {
            if let Item::Impl(id) = it {
                for m in &id.items {
                    let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);

                    // 已有同名自由函数（passes 降解），交给 define_fn 处理，这里跳过
                    if fun_names.contains(&sym) {
                        continue;
                    }

                    let fid = *self.base_func_ids.get(&sym)
                        .ok_or_else(|| anyhow!("define_impls: missing FuncId for `{}`", sym))?;

                    let fdecl = FunDecl {
                        name: sym.clone(),
                        type_params: vec![],
                        params: m.params.clone(),
                        ret: m.ret.clone(),
                        where_bounds: vec![],
                        body: m.body.clone(),
                        is_extern: false,
                    };
                    self.define_fn_core(fid, &fdecl)?;
                }
            }
        }
        Ok(())
    }

    /// 定义所有已声明的**专门化**函数体
    pub fn define_mono_from_program(&mut self, _prog: &Program, _base_ids: &HashMap<String, FuncId>) -> Result<()> {
        let specs: Vec<(String, (String, Vec<Ty>))> =
            self.mono_specs.iter().map(|(k, v)| (k.clone(), v.clone())).collect();

        for (mname, (base, targs)) in specs {
            if !self.mono_defined.insert(mname.clone()) { continue; }
            let templ = self.templates.get(&base)
                .ok_or_else(|| anyhow!("monomorph define: template `{}` not found", base))?;
            if templ.type_params.len() != targs.len() {
                return Err(anyhow!("template `{}` type params mismatch for `{}`", base, mname));
            }
            let fid = *self.mono_func_ids.get(&mname)
                .ok_or_else(|| anyhow!("monomorph define: missing FuncId for `{}`", mname))?;

            let subst = build_subst_map(&templ.type_params, &targs);
            let mono_fun = specialize_fun(templ, &subst, &mname)?;
            self.define_fn_core(fid, &mono_fun)?;
        }
        Ok(())
    }

    /// 核心定义：根据 FunDecl 生成 CLIF 并 define
    fn define_fn_core(&mut self, fid: FuncId, f: &FunDecl) -> Result<()> {
        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for (_, ty) in &f.params {
            sig.params.push(AbiParam::new(cl_ty(ty).ok_or_else(|| anyhow!("invalid param ty {:?}", ty))?));
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

        let mut scopes: Vec<HashMap<String, Variable>> = vec![HashMap::new()];
        let mut scopes_ty: Vec<HashMap<String, Ty>>     = vec![HashMap::new()];

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
                    (Ty::Int,    GConst::I32(n)) => b.ins().iconst(types::I32, *n as i64),
                    (Ty::Long,   GConst::I64(n)) => b.ins().iconst(types::I64, *n),
                    (Ty::Bool,   GConst::I8(n))  => b.ins().iconst(types::I8,  *n as i64),
                    (Ty::Char,   GConst::I32(n)) => b.ins().iconst(types::I32, *n as i64),
                    (Ty::Float,  GConst::F32(x)) => b.ins().f32const(*x),
                    (Ty::Double, GConst::F64(x)) => b.ins().f64const(*x),
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
            let v = cg.coerce_to_ty(&mut b, ret_val, &ret_ty);
            b.ins().return_(&[v]);
        }

        b.finalize();

        self.module.define_function(fid, &mut ctx)?;
        self.module.clear_context(&mut ctx);
        Ok(())
    }

    pub fn finish(self) -> Result<Vec<u8>> {
        let obj = self.module.finish();
        Ok(obj.emit()?)
    }

    fn intern_str(&mut self, s: &str) -> Result<DataId> {
        if let Some(&id) = self.str_pool.get(s) { return Ok(id); }
        let mut dc = DataContext::new();
        let mut bytes = s.as_bytes().to_vec();
        bytes.push(0);
        dc.define(bytes.into_boxed_slice());
        let name = format!("__str_{}", self.str_pool.len());
        let id = self.module.declare_data(&name, Linkage::Local, true, false)?;
        self.module.define_data(id, &dc)?;
        self.str_pool.insert(s.to_string(), id);
        Ok(id)
    }
}

/* Paw Ty → CLIF Type（Void -> None） */
fn cl_ty(t: &Ty) -> Option<ir::Type> {
    match t {
        Ty::Int    => Some(types::I32),
        Ty::Long   => Some(types::I64),
        Ty::Bool   => Some(types::I8),
        Ty::Char   => Some(types::I32),
        Ty::Float  => Some(types::F32),
        Ty::Double => Some(types::F64),
        Ty::String => Some(types::I64),
        Ty::Void   => None,
        Ty::Var(_) | Ty::App { .. } => None,
    }
}

/* ---------------------------
 * ------ Expr 生成器 --------
 * --------------------------- */
struct ExprGen<'a> {
    be: &'a mut CLBackend,
    scopes: Vec<HashMap<String, Variable>>,
    scopes_ty: Vec<HashMap<String, Ty>>,
    // (continue_target, break_target)
    loop_stack: Vec<(ir::Block, ir::Block)>,
    fun_ret: Ty,
}

impl<'a> ExprGen<'a> {
    fn push(&mut self){ self.scopes.push(HashMap::new()); self.scopes_ty.push(HashMap::new()); }
    fn pop(&mut self){ self.scopes.pop(); self.scopes_ty.pop(); }

    fn declare_named(&mut self, b:&mut FunctionBuilder, name:&str, ty:&Ty) -> Variable {
        let v = b.declare_var(cl_ty(ty).unwrap());
        self.scopes.last_mut().unwrap().insert(name.to_string(), v);
        self.scopes_ty.last_mut().unwrap().insert(name.to_string(), ty.clone());
        v
    }
    fn lookup(&self, name:&str)->Option<Variable>{
        for s in self.scopes.iter().rev(){ if let Some(v)=s.get(name){ return Some(*v); } }
        None
    }
    fn lookup_ty(&self, name:&str)->Option<Ty>{
        for s in self.scopes_ty.iter().rev(){ if let Some(t)=s.get(name){ return Some(t.clone()); } }
        None
    }

    // i8 -> b1（比较 != 0）
    fn bool_i8_to_b1(&mut self, b:&mut FunctionBuilder, v_i8: ir::Value)->ir::Value{
        let zero = b.ins().iconst(types::I8, 0);
        b.ins().icmp(IntCC::NotEqual, v_i8, zero)
    }
    // b1 -> i8 {1/0}
    fn bool_b1_to_i8(&mut self, b:&mut FunctionBuilder, v_b1: ir::Value)->ir::Value{
        let one = b.ins().iconst(types::I8, 1);
        let zer = b.ins().iconst(types::I8, 0);
        b.ins().select(v_b1, one, zer)
    }

    #[inline]
    fn val_ty(&self, b:&FunctionBuilder, v:ir::Value)->ir::Type { b.func.dfg.value_type(v) }

    fn coerce_to_ty(&mut self, b:&mut FunctionBuilder, v: ir::Value, dst:&Ty)->ir::Value {
        let want = cl_ty(dst).expect("no void here");
        self.coerce_value_to_irtype(b, v, want)
    }

    fn coerce_value_to_irtype(&mut self, b:&mut FunctionBuilder, v: ir::Value, dst: ir::Type) -> ir::Value {
        let src = b.func.dfg.value_type(v);
        if src == dst { return v; }

        if src.is_int() && dst.is_int() {
            return if src.bits() < dst.bits() { b.ins().sextend(dst, v) } else { b.ins().ireduce(dst, v) }
        }
        if src.is_float() && dst.is_float() {
            return if src.bits() < dst.bits() { b.ins().fpromote(dst, v) } else { b.ins().fdemote(dst, v) }
        }

        if dst == types::I8  { return b.ins().iconst(types::I8,  0); }
        if dst == types::I32 { return b.ins().iconst(types::I32, 0); }
        if dst == types::I64 { return b.ins().iconst(types::I64, 0); }
        if dst == types::F32 { return b.ins().f32const(0.0); }
        if dst == types::F64 { return b.ins().f64const(0.0); }
        v
    }

    fn unify_values_for_numeric(
        &mut self,
        b:&mut FunctionBuilder,
        mut l: ir::Value,
        mut r: ir::Value,
    )->(ir::Value, ir::Value, ir::Type){
        let lt = self.val_ty(b, l);
        let rt = self.val_ty(b, r);
        if lt == rt { return (l, r, lt); }

        if lt.is_int() && rt.is_int() {
            let target = if lt.bits() >= rt.bits() { lt } else { rt };
            if lt.bits() < target.bits() { l = b.ins().sextend(target, l); }
            if rt.bits() < target.bits() { r = b.ins().sextend(target, r); }
            return (l, r, target);
        }

        if lt.is_float() && rt.is_float() {
            if lt == types::F64 || rt == types::F64 {
                if lt == types::F32 { l = b.ins().fpromote(types::F64, l); }
                if rt == types::F32 { r = b.ins().fpromote(types::F64, r); }
                return (l, r, types::F64);
            } else { return (l, r, lt); }
        }

        (l, r, lt)
    }

    fn emit_block(&mut self, b:&mut FunctionBuilder, blk:&Block)->Result<ir::Value>{
        self.push();
        for s in &blk.stmts {
            match s {
                Stmt::Let { name, ty, init, .. } => {
                    let v0 = self.emit_expr(b, init)?;
                    let v  = match ty {
                        Ty::Bool => { let b1 = self.bool_i8_to_b1(b, v0); self.bool_b1_to_i8(b, b1) }
                        _ => self.coerce_to_ty(b, v0, ty),
                    };
                    let var = self.declare_named(b, name, ty);
                    b.def_var(var, v);
                }

                Stmt::Assign { name, expr } => {
                    let var = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                    let rhs = self.emit_expr(b, expr)?;
                    let v = self.coerce_to_ty(b, rhs, &ty);
                    b.def_var(var, v);
                }

                // 表达式语句——**始终**实际求值（即使返回 Void），确保诸如 println_int(...) 的副作用发生
                Stmt::Expr(e) => { let _ = self.emit_expr(b, e)?; }

                Stmt::If { cond, then_b, else_b } => {
                    let cv  = self.emit_expr(b, cond)?; let c1  = self.bool_i8_to_b1(b, cv);
                    let tbb = b.create_block(); let ebb = b.create_block(); let out = b.create_block();
                    b.ins().brif(c1, tbb, &[], ebb, &[]);
                    b.switch_to_block(tbb); b.seal_block(tbb); let _ = self.emit_block(b, then_b)?; b.ins().jump(out, &[]);
                    b.switch_to_block(ebb); b.seal_block(ebb); if let Some(eb) = else_b { let _ = self.emit_block(b, eb)?; } b.ins().jump(out, &[]);
                    b.switch_to_block(out); b.seal_block(out);
                }

                Stmt::While { cond, body } => {
                    let hdr = b.create_block(); let bb  = b.create_block(); let out = b.create_block();
                    b.ins().jump(hdr, &[]); b.switch_to_block(hdr);
                    let cv = self.emit_expr(b, cond)?; let c  = self.bool_i8_to_b1(b, cv);
                    b.ins().brif(c, bb, &[], out, &[]);
                    b.switch_to_block(bb); b.seal_block(bb);
                    self.loop_stack.push((hdr, out)); let _ = self.emit_block(b, body)?; self.loop_stack.pop();
                    b.ins().jump(hdr, &[]); b.seal_block(hdr);
                    b.switch_to_block(out); b.seal_block(out);
                }

                Stmt::For { init, cond, step, body } => {
                    if let Some(fi) = init {
                        match fi {
                            ForInit::Let { name, ty, init, .. } => {
                                let rhs = self.emit_expr(b, init)?; let v = self.coerce_to_ty(b, rhs, ty);
                                let var = self.declare_named(b, name, ty); b.def_var(var, v);
                            }
                            ForInit::Assign { name, expr } => {
                                let (var, ty) = {
                                    let v  = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                                    (v, ty)
                                };
                                let rhs = self.emit_expr(b, expr)?; let v   = self.coerce_to_ty(b, rhs, &ty);
                                b.def_var(var, v);
                            }
                            ForInit::Expr(e) => { let _ = self.emit_expr(b, e)?; }
                        }
                    }

                    let hdr   = b.create_block(); let stepb = b.create_block();
                    let bodyb = b.create_block(); let out   = b.create_block();
                    b.ins().jump(hdr, &[]); b.switch_to_block(hdr);

                    let cval = if let Some(c) = cond {
                        let cv = self.emit_expr(b, c)?; self.bool_i8_to_b1(b, cv)
                    } else {
                        let one = b.ins().iconst(types::I8, 1); self.bool_i8_to_b1(b, one)
                    };
                    b.ins().brif(cval, bodyb, &[], out, &[]);

                    b.switch_to_block(bodyb); b.seal_block(bodyb);
                    self.loop_stack.push((stepb, out)); let _ = self.emit_block(b, body)?; self.loop_stack.pop();
                    b.ins().jump(stepb, &[]);

                    b.switch_to_block(stepb); b.seal_block(stepb);
                    if let Some(st) = step {
                        match st {
                            ForStep::Assign { name, expr } => {
                                let (var, ty) = {
                                    let v  = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                                    (v, ty)
                                };
                                let rhs = self.emit_expr(b, expr)?; let v   = self.coerce_to_ty(b, rhs, &ty);
                                b.def_var(var, v);
                            }
                            ForStep::Expr(e) => { let _ = self.emit_expr(b, e)?; }
                        }
                    }

                    b.ins().jump(hdr, &[]); b.seal_block(hdr);
                    b.switch_to_block(out); b.seal_block(out);
                }

                Stmt::Break => {
                    let &(_, out) = self.loop_stack.last().ok_or_else(|| anyhow!("`break` outside loop"))?;
                    b.ins().jump(out, &[]); let cont = b.create_block(); b.switch_to_block(cont); b.seal_block(cont);
                }

                Stmt::Continue => {
                    let &(cont_tgt, _) = self.loop_stack.last().ok_or_else(|| anyhow!("`continue` outside loop"))?;
                    b.ins().jump(cont_tgt, &[]); let cont = b.create_block(); b.switch_to_block(cont); b.seal_block(cont);
                }

                Stmt::Return(opt) => {
                    match (self.fun_ret.clone(), opt) {
                        (Ty::Void, Some(e)) => { let _ = self.emit_expr(b, e)?; b.ins().return_(&[]); }
                        (ret_ty, Some(e)) => { let raw = self.emit_expr(b, e)?; let v = self.coerce_to_ty(b, raw, &ret_ty); b.ins().return_(&[v]); }
                        (ret_ty, None) => {
                            match ret_ty {
                                Ty::Bool => { let z = b.ins().iconst(types::I8, 0);  b.ins().return_(&[z]); }
                                Ty::Int | Ty::Char => { let z = b.ins().iconst(types::I32, 0); b.ins().return_(&[z]); }
                                Ty::Long | Ty::String => { let z = b.ins().iconst(types::I64, 0); b.ins().return_(&[z]); }
                                Ty::Float => { let z = b.ins().f32const(0.0); b.ins().return_(&[z]); }
                                Ty::Double => { let z = b.ins().f64const(0.0); b.ins().return_(&[z]); }
                                Ty::Void => { b.ins().return_(&[]); }
                                other => {
                                    return Err(anyhow!("codegen: `return` without value but function has non-void/unsupported return type: {:?}", other));
                                }
                            }
                        }
                    }
                    // return 后开新块，容忍死代码继续生成
                    let cont = b.create_block(); b.switch_to_block(cont); b.seal_block(cont);
                }
            }
        }
        let out = match &blk.tail { Some(e) => self.emit_expr(b, e)?, None => b.ins().iconst(types::I32, 0) };
        self.pop();
        Ok(out)
    }

    /// 只处理最常见：模板 1 个类型参数，且唯一形参是裸 `T`；返回 [T := arg_ty]
    fn infer_targs_for_simple_arity1(&mut self, base: &str, args: &[Expr]) -> Result<Vec<Ty>> {
        let tpl = self.be.templates.get(base)
            .ok_or_else(|| anyhow!("internal: no template for `{}`", base))?;

        if tpl.type_params.len() != 1 || tpl.params.len() != 1 {
            bail!("cannot infer type args for `{}`; add explicit `<...>`", base);
        }
        // 形参必须是裸 T
        let tp0 = &tpl.type_params[0];
        match &tpl.params[0].1 {
            Ty::Var(v) if v == tp0 => {},
            _ => bail!("cannot infer `{}`: param is not plain type variable", base),
        }

        // 尝试从实参表达式拿静态类型（字面量、变量等）
        let at = self.static_ty_of_expr(&args[0])?;
        Ok(vec![at])
    }

    /// 从表达式快速给出静态类型；用于推断 println<T>(...)
    fn static_ty_of_expr(&mut self, e: &Expr) -> Result<Ty> {
        Ok(match e {
            Expr::Int(_)    => Ty::Int,
            Expr::Long(_)   => Ty::Long,
            Expr::Bool(_)   => Ty::Bool,
            Expr::Char(_)   => Ty::Char,
            Expr::Float(_)  => Ty::Float,
            Expr::Double(_) => Ty::Double,
            Expr::Str(_)    => Ty::String,
            Expr::Var(n)    => self.lookup_ty(n)
                .ok_or_else(|| anyhow!("cannot infer type of `{}` here; add explicit `<...>`", n))?,
            _ => bail!("cannot infer type from this expression; please write `{}`<...>(...)", "println"),
        })
    }

    fn emit_expr(&mut self, b:&mut FunctionBuilder, e:&Expr)->Result<ir::Value>{
        use BinOp::*; use UnOp::*;
        Ok(match e {
            Expr::Int(n)    => b.ins().iconst(types::I32, *n as i64),
            Expr::Long(n)   => b.ins().iconst(types::I64, *n),
            Expr::Float(x)  => b.ins().f32const(*x),
            Expr::Double(x) => b.ins().f64const(*x),
            Expr::Char(u)   => b.ins().iconst(types::I32, *u as i64),
            Expr::Bool(bi)  => b.ins().iconst(types::I8, if *bi {1} else {0}),
            Expr::Str(s)    => {
                let did = self.be.intern_str(s)?; let gv  = self.be.module.declare_data_in_func(did, b.func);
                b.ins().global_value(types::I64, gv)
            }

            Expr::Var(name) => {
                let v = self.lookup(name).ok_or_else(|| anyhow!("unknown var in codegen `{name}`"))?;
                b.use_var(v)
            }

            Expr::Unary{op, rhs} => {
                let r = self.emit_expr(b, rhs)?; let rt = self.val_ty(b, r);
                match op {
                    Neg => if rt.is_int() { b.ins().ineg(r) } else { b.ins().fneg(r) },
                    Not => { let b1 = self.bool_i8_to_b1(b, r); let nb = b.ins().bnot(b1); self.bool_b1_to_i8(b, nb) }
                }
            }

            Expr::Binary{op,lhs,rhs}=>{
                match op {
                    And => {
                        let l  = self.emit_expr(b, lhs)?; let l1 = self.bool_i8_to_b1(b, l);
                        let rhsb   = b.create_block(); let falseb = b.create_block(); let out = b.create_block();
                        let res = b.declare_var(types::I8);
                        b.ins().brif(l1, rhsb, &[], falseb, &[]);
                        b.switch_to_block(falseb); b.seal_block(falseb); let zero = b.ins().iconst(types::I8, 0); b.def_var(res, zero); b.ins().jump(out, &[]);
                        b.switch_to_block(rhsb);  b.seal_block(rhsb);  let r  = self.emit_expr(b, rhs)?; let r1 = self.bool_i8_to_b1(b, r); let ri = self.bool_b1_to_i8(b, r1); b.def_var(res, ri); b.ins().jump(out, &[]);
                        b.switch_to_block(out);   b.seal_block(out);   b.use_var(res)
                    }
                    Or => {
                        let l  = self.emit_expr(b, lhs)?; let l1 = self.bool_i8_to_b1(b, l);
                        let trueb = b.create_block(); let rhsb  = b.create_block(); let out   = b.create_block();
                        let res = b.declare_var(types::I8);
                        b.ins().brif(l1, trueb, &[], rhsb, &[]);
                        b.switch_to_block(trueb); b.seal_block(trueb); let one = b.ins().iconst(types::I8, 1); b.def_var(res, one); b.ins().jump(out, &[]);
                        b.switch_to_block(rhsb);  b.seal_block(rhsb);  let r  = self.emit_expr(b, rhs)?; let r1 = self.bool_i8_to_b1(b, r); let ri = self.bool_b1_to_i8(b, r1); b.def_var(res, ri); b.ins().jump(out, &[]);
                        b.switch_to_block(out);   b.seal_block(out);   b.use_var(res)
                    }
                    _ => {
                        let l = self.emit_expr(b, lhs)?; let r = self.emit_expr(b, rhs)?; let (l, r, ty) = self.unify_values_for_numeric(b, l, r);
                        match op {
                            Add => if ty.is_int() { b.ins().iadd(l, r) } else { b.ins().fadd(l, r) },
                            Sub => if ty.is_int() { b.ins().isub(l, r) } else { b.ins().fsub(l, r) },
                            Mul => if ty.is_int() { b.ins().imul(l, r) } else { b.ins().fmul(l, r) },
                            Div => if ty.is_int() { b.ins().sdiv(l, r) } else { b.ins().fdiv(l, r) },
                            Lt | Le | Gt | Ge => {
                                let b1 = if ty.is_int() {
                                    let cc = match op {
                                        Lt => IntCC::SignedLessThan,
                                        Le => IntCC::SignedLessThanOrEqual,
                                        Gt => IntCC::SignedGreaterThan,
                                        Ge => IntCC::SignedGreaterThanOrEqual,
                                        _  => unreachable!(),
                                    };
                                    b.ins().icmp(cc, l, r)
                                } else {
                                    let cc = match op {
                                        Lt => FloatCC::LessThan,
                                        Le => FloatCC::LessThanOrEqual,
                                        Gt => FloatCC::GreaterThan,
                                        Ge => FloatCC::GreaterThanOrEqual,
                                        _  => unreachable!(),
                                    };
                                    b.ins().fcmp(cc, l, r)
                                };
                                self.bool_b1_to_i8(b, b1)
                            }
                            Eq | Ne => {
                                let b1 = if ty.is_float() {
                                    let cc = if matches!(op, Eq) { FloatCC::Equal } else { FloatCC::NotEqual };
                                    b.ins().fcmp(cc, l, r)
                                } else {
                                    let cc = if matches!(op, Eq) { IntCC::Equal } else { IntCC::NotEqual };
                                    b.ins().icmp(cc, l, r)
                                };
                                self.bool_b1_to_i8(b, b1)
                            }
                            And | Or => unreachable!(),
                        }
                    }
                }
            }

            Expr::If { cond, then_b, else_b } => {
                let cv = self.emit_expr(b, cond)?; let c1 = self.bool_i8_to_b1(b, cv);
                let tbb = b.create_block(); let ebb = b.create_block(); let mb  = b.create_block();
                b.ins().brif(c1, tbb, &[], ebb, &[]);
                b.switch_to_block(tbb); b.seal_block(tbb);
                let tv0 = self.emit_block(b, then_b)?; let tv_ty = self.val_ty(b, tv0); let phi = b.declare_var(tv_ty); b.def_var(phi, tv0); b.ins().jump(mb, &[]);
                b.switch_to_block(ebb); b.seal_block(ebb);
                let ev0 = self.emit_block(b, else_b)?; let ev  = self.coerce_value_to_irtype(b, ev0, tv_ty); b.def_var(phi, ev); b.ins().jump(mb, &[]);
                b.switch_to_block(mb);  b.seal_block(mb); b.use_var(phi)
            }

            Expr::Match { scrut, arms, default } => {
                let sv = self.emit_expr(b, scrut)?; let out = b.create_block(); let mut phi: Option<Variable> = None;
                let mut next: Option<ir::Block> = None;
                for (pat, blk) in arms {
                    let this  = b.create_block(); let thenb = b.create_block(); let cont  = b.create_block();
                    if let Some(prev) = next.take() { b.switch_to_block(prev); b.seal_block(prev); b.ins().jump(this, &[]); }
                    else { b.ins().jump(this, &[]); }
                    b.switch_to_block(this); b.seal_block(this);
                    let hit_b1 = match pat {
                        Pattern::Int(n) => { let cn = b.ins().iconst(types::I32, *n as i64); let s  = if self.val_ty(b, sv) == types::I32 { sv } else { b.ins().ireduce(types::I32, sv) }; b.ins().icmp(IntCC::Equal, s, cn) }
                        Pattern::Long(n) => { let cn = b.ins().iconst(types::I64, *n); let s  = if self.val_ty(b, sv) == types::I64 { sv } else { b.ins().sextend(types::I64, sv) }; b.ins().icmp(IntCC::Equal, s, cn) }
                        Pattern::Char(u) => { let cn = b.ins().iconst(types::I32, *u as i64); let s  = if self.val_ty(b, sv) == types::I32 { sv } else { b.ins().ireduce(types::I32, sv) }; b.ins().icmp(IntCC::Equal, s, cn) }
                        Pattern::Bool(bt) => { let cn = b.ins().iconst(types::I8, if *bt {1} else {0}); let s  = if self.val_ty(b, sv) == types::I8 { sv } else { b.ins().ireduce(types::I8, sv) }; b.ins().icmp(IntCC::Equal, s, cn) }
                        Pattern::Wild => { let one = b.ins().iconst(types::I8, 1); self.bool_i8_to_b1(b, one) }
                    };
                    b.ins().brif(hit_b1, thenb, &[], cont, &[]);
                    b.switch_to_block(thenb); b.seal_block(thenb);
                    let v = self.emit_block(b, blk)?; if phi.is_none() { let vt = self.val_ty(b, v); phi = Some(b.declare_var(vt)); }
                    b.def_var(phi.unwrap(), v); b.ins().jump(out, &[]);
                    next = Some(cont);
                }
                if let Some(last) = next.take() {
                    b.switch_to_block(last); b.seal_block(last);
                    if let Some(db) = default {
                        let v = self.emit_block(b, db)?; if phi.is_none() { let vt = self.val_ty(b, v); phi = Some(b.declare_var(vt)); }
                        b.def_var(phi.unwrap(), v); b.ins().jump(out, &[]);
                    } else {
                        if phi.is_none() { phi = Some(b.declare_var(types::I32)); }
                        let z = b.ins().iconst(types::I32, 0); b.def_var(phi.unwrap(), z); b.ins().jump(out, &[]);
                    }
                }
                b.switch_to_block(out); b.seal_block(out); b.use_var(phi.unwrap())
            }

            // 调用：支持 Name 与 Qualified
            Expr::Call { callee, generics, args } => {
                let (sym, fid) = match callee {
                    Callee::Name(name) => {
                        // 三种路径：
                        // A) 显式 `<...>` 的泛型调用 → 用 mangle_name
                        // B) 非泛型的普通函数（base_func_ids 里有） → 直接调用
                        // C) 隐式泛型：base 不在 base_func_ids，但在 templates → 现场推断 + ensure_monomorph
                        if !generics.is_empty() {
                            let mname = mangle_name(name, generics);
                            let fid  = *self.be.mono_func_ids.get(&mname)
                                .ok_or_else(|| anyhow!("unknown monomorph function `{}`", mname))?;
                            (mname, fid)
                        } else if let Some(&fid) = self.be.base_func_ids.get(name) {
                            (name.clone(), fid)
                        } else if self.be.templates.contains_key(name) {
                            // 隐式泛型：目前实现 arity=1 的常见形态（println<T>(x:T)）
                            let targs = self.infer_targs_for_simple_arity1(name, args)?;
                            let (mname, fid) = self.be.ensure_monomorph(name, &targs)?;
                            (mname, fid)
                        } else {
                            return Err(anyhow!("unknown function in codegen `{}`", name));
                        }
                    }
                    Callee::Qualified { trait_name, method } => {
                        if generics.is_empty() {
                            return Err(anyhow!("qualified call `{}::{}` needs explicit type args, e.g. `{}::{}<Int>(...)`",
                                               trait_name, method, trait_name, method));
                        }
                        let sym = mangle_impl_method(trait_name, generics, method);
                        let fid = match self.be.base_func_ids.get(&sym) {
                            Some(fid) => *fid,
                            None => {
                                return Err(anyhow!(
                                    "unknown impl method symbol `{}`; 确认你在 codegen 前调用了 \
                                     `declare_impls_from_program(&program)`，或已由 passes 降解为自由函数并调用了 \
                                     `declare_fns(&all_fun_decls)`",
                                    sym
                                ));
                            }
                        };
                        (sym, fid)
                    }
                };

                let (param_tys, ret_ty) = self.be.fn_sig.get(&sym)
                    .ok_or_else(|| anyhow!("missing signature for `{sym}`"))?
                    .clone();

                if param_tys.len() != args.len() {
                    return Err(anyhow!("function `{}` expects {} args, got {}", sym, param_tys.len(), args.len()));
                }

                let mut av = Vec::with_capacity(args.len());
                for (a, pt) in args.iter().zip(param_tys.iter()) {
                    let v0 = self.emit_expr(b, a)?; let v  = self.coerce_to_ty(b, v0, pt); av.push(v);
                }

                let fref = self.be.module.declare_func_in_func(fid, b.func);
                let call = b.ins().call(fref, &av);

                if matches!(ret_ty, Ty::Void) {
                    // 即使返回 Void，也已 **发起调用**，副作用（println）将发生。
                    b.ins().iconst(types::I32, 0)
                } else {
                    b.inst_results(call)[0]
                }
            }

            Expr::Block(blk) => self.emit_block(b, blk)?,
        })
    }
}

/* ----------------------------------
 * ----- 单态化：替换工具 -----
 * ---------------------------------- */
fn build_subst_map(params: &[String], args: &[Ty]) -> HashMap<String, Ty> {
    let mut m = HashMap::new();
    for (p, a) in params.iter().zip(args.iter()) { m.insert(p.clone(), a.clone()); }
    m
}
fn subst_ty(t: &Ty, s: &HashMap<String, Ty>) -> Ty {
    match t {
        Ty::Var(n) => s.get(n).cloned().unwrap_or_else(|| Ty::Var(n.clone())),
        Ty::App { name, args } => Ty::App { name: name.clone(), args: args.iter().map(|x| subst_ty(x, s)).collect() },
        _ => t.clone(),
    }
}
fn subst_expr(e: &Expr, s: &HashMap<String, Ty>) -> Expr {
    match e {
        Expr::Call { callee, generics, args } => Expr::Call {
            callee: callee.clone(),
            generics: generics.iter().map(|t| subst_ty(t, s)).collect(),
            args: args.iter().map(|a| subst_expr(a, s)).collect(),
        },
        Expr::Unary { op, rhs } => Expr::Unary { op: *op, rhs: Box::new(subst_expr(rhs, s)) },
        Expr::Binary { op, lhs, rhs } => Expr::Binary { op: *op, lhs: Box::new(subst_expr(lhs, s)), rhs: Box::new(subst_expr(rhs, s)) },
        Expr::If { cond, then_b, else_b } => Expr::If { cond: Box::new(subst_expr(cond, s)), then_b: subst_block(then_b, s), else_b: subst_block(else_b, s) },
        Expr::Match { scrut, arms, default } => {
            let scr = Box::new(subst_expr(scrut, s));
            let mut new_arms = Vec::with_capacity(arms.len());
            for (pat, blk) in arms { new_arms.push((pat.clone(), subst_block(blk, s))); }
            let def = default.as_ref().map(|b| subst_block(b, s));
            Expr::Match { scrut: scr, arms: new_arms, default: def }
        }
        Expr::Block(b) => Expr::Block(subst_block(b, s)),
        _ => e.clone(),
    }
}
fn subst_stmt(st: &Stmt, s: &HashMap<String, Ty>) -> Stmt {
    match st {
        Stmt::Let { name, ty, init, is_const } => Stmt::Let { name: name.clone(), ty: subst_ty(ty, s), init: subst_expr(init, s), is_const: *is_const },
        Stmt::Assign { name, expr } => Stmt::Assign { name: name.clone(), expr: subst_expr(expr, s) },
        Stmt::While { cond, body } => Stmt::While { cond: subst_expr(cond, s), body: subst_block(body, s) },
        Stmt::For { init, cond, step, body } => Stmt::For {
            init: init.as_ref().map(|fi| match fi {
                ForInit::Let { name, ty, init, is_const } => ForInit::Let { name: name.clone(), ty: subst_ty(ty, s), init: subst_expr(init, s), is_const: *is_const },
                ForInit::Assign { name, expr } => ForInit::Assign { name: name.clone(), expr: subst_expr(expr, s) },
                ForInit::Expr(e) => ForInit::Expr(subst_expr(e, s)),
            }),
            cond: cond.as_ref().map(|c| subst_expr(c, s)),
            step: step.as_ref().map(|stp| match stp {
                ForStep::Assign { name, expr } => ForStep::Assign { name: name.clone(), expr: subst_expr(expr, s) },
                ForStep::Expr(e) => ForStep::Expr(subst_expr(e, s)),
            }),
            body: subst_block(body, s),
        },
        Stmt::If { cond, then_b, else_b } => Stmt::If { cond: subst_expr(cond, s), then_b: subst_block(then_b, s), else_b: else_b.as_ref().map(|b| subst_block(b, s)) },
        Stmt::Expr(e) => Stmt::Expr(subst_expr(e, s)),
        Stmt::Return(e) => Stmt::Return(e.as_ref().map(|x| subst_expr(x, s))),
        Stmt::Break | Stmt::Continue => st.clone(),
    }
}
fn subst_block(b: &Block, s: &HashMap<String, Ty>) -> Block {
    Block { stmts: b.stmts.iter().map(|st| subst_stmt(st, s)).collect(), tail: b.tail.as_ref().map(|e| Box::new(subst_expr(e, s))) }
}
fn specialize_fun(tpl: &FunDecl, subst: &HashMap<String, Ty>, mono_name: &str) -> Result<FunDecl> {
    let mut params = Vec::with_capacity(tpl.params.len());
    for (n, t) in &tpl.params {
        let nt = subst_ty(t, subst);
        if cl_ty(&nt).is_none() { return Err(anyhow!("monomorph param type not concrete/ABI-legal in `{}`: {:?}", mono_name, nt)); }
        params.push((n.clone(), nt));
    }
    let ret = subst_ty(&tpl.ret, subst);
    if !matches!(ret, Ty::Void) && cl_ty(&ret).is_none() { return Err(anyhow!("monomorph return type not concrete/ABI-legal in `{}`: {:?}", mono_name, ret)); }
    let body = subst_block(&tpl.body, subst);
    Ok(FunDecl { name: mono_name.to_string(), type_params: vec![], params, ret, where_bounds: vec![], body, is_extern: false })
}

/* ----------------------------------
 * ----- 遍历：收集泛型调用 -----
 * ---------------------------------- */
fn collect_calls_with_generics_in_program(p: &Program, out: &mut Vec<(String, Vec<Ty>)>) {
    for it in &p.items {
        match it {
            Item::Fun(f) => collect_calls_in_block(&f.body, out),
            Item::Global { init, .. } => collect_calls_in_expr(init, out),
            Item::Import(_) => {}
            Item::Trait(_) | Item::Impl(_) => {}
        }
    }
}
fn collect_calls_in_block(b: &Block, out: &mut Vec<(String, Vec<Ty>)>) {
    for s in &b.stmts {
        match s {
            Stmt::Let { init, .. } => collect_calls_in_expr(init, out),
            Stmt::Assign { expr, .. } => collect_calls_in_expr(expr, out),
            Stmt::While { cond, body } => { collect_calls_in_expr(cond, out); collect_calls_in_block(body, out); }
            Stmt::For { init, cond, step, body } => {
                if let Some(fi) = init {
                    match fi {
                        ForInit::Let { init, .. } => collect_calls_in_expr(init, out),
                        ForInit::Assign { expr, .. } => collect_calls_in_expr(expr, out),
                        ForInit::Expr(e) => collect_calls_in_expr(e, out),
                    }
                }
                if let Some(c) = cond { collect_calls_in_expr(c, out); }
                if let Some(st) = step {
                    match st {
                        ForStep::Assign { expr, .. } => collect_calls_in_expr(expr, out),
                        ForStep::Expr(e) => collect_calls_in_expr(e, out),
                    }
                }
                collect_calls_in_block(body, out);
            }
            Stmt::If { cond, then_b, else_b } => {
                collect_calls_in_expr(cond, out);
                collect_calls_in_block(then_b, out);
                if let Some(b) = else_b { collect_calls_in_block(b, out); }
            }
            Stmt::Expr(e) => collect_calls_in_expr(e, out),
            Stmt::Return(opt) => if let Some(e) = opt { collect_calls_in_expr(e, out); },
            Stmt::Break | Stmt::Continue => {}
        }
    }
    if let Some(t) = &b.tail { collect_calls_in_expr(t, out); }
}
fn collect_calls_in_expr(e: &Expr, out: &mut Vec<(String, Vec<Ty>)>) {
    match e {
        Expr::Call { callee, generics, args } => {
            if !generics.is_empty() {
                if let Callee::Name(nm) = callee { out.push((nm.clone(), generics.clone())); }
            }
            for a in args { collect_calls_in_expr(a, out); }
        }
        Expr::Unary { rhs, .. } => collect_calls_in_expr(rhs, out),
        Expr::Binary { lhs, rhs, .. } => { collect_calls_in_expr(lhs, out); collect_calls_in_expr(rhs, out); }
        Expr::If { cond, then_b, else_b } => {
            collect_calls_in_expr(cond, out); collect_calls_in_block(then_b, out); collect_calls_in_block(else_b, out);
        }
        Expr::Match { scrut, arms, default } => {
            collect_calls_in_expr(scrut, out);
            for (_, b) in arms { collect_calls_in_block(b, out); }
            if let Some(b) = default { collect_calls_in_block(b, out); }
        }
        Expr::Block(b) => collect_calls_in_block(b, out),
        _ => {}
    }
}

/* ---------------------------
 * 可选：统一的编译入口（确保顺序）
 * --------------------------- */
pub fn compile_program(prog: &Program) -> Result<Vec<u8>> {
    let mut be = CLBackend::new()?;
    be.set_globals_from_program(prog);

    // 1) 先把“自由函数”（含 extern）全部声明
    let funs: Vec<&FunDecl> = prog.items.iter().filter_map(|it| {
        if let Item::Fun(f) = it { Some(f) } else { None }
    }).collect();
    let base_ids = be.declare_fns(funs.clone())?;

    // 2) 再把 impl 方法声明成普通符号（若 passes 已降解，会被上面的 declare_fns 覆盖；此处就跳过）
    be.declare_impls_from_program(prog)?;

    // 3) 收集并声明所有需要的单态化实例（对 `foo<T>` 这类：显式 `<...>`）
    be.declare_mono_from_program(prog)?;

    // 4) 定义自由函数（非泛型）
    for f in &funs { be.define_fn(f, &base_ids)?; }

    // 5) 定义 impl 方法（若 passes 已降解为自由函数，这里会跳过）
    be.define_impls_from_program(prog)?;

    // 6) 定义单态化实例（显式 `<...>` 的）
    be.define_mono_from_program(prog, &base_ids)?;

    // 7) 输出 obj
    be.finish()
}
