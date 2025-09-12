use crate::ast::*;
use anyhow::{anyhow, Result};
use cranelift_codegen::{
    ir::{self, types, AbiParam, InstBuilder},
    ir::condcodes::{FloatCC, IntCC},
    isa, settings,
};
use cranelift_codegen::settings::Configurable;
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{default_libcall_names, DataDescription as DataContext, DataId, FuncId, Linkage, Module};
use cranelift_object::{ObjectBuilder, ObjectModule};
use std::collections::HashMap;
use target_lexicon::Triple;

// ---------- 承载可内联注入的全局常量 ----------
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
    // 函数签名：便于 call 时做形参与返回值处理
    fn_sig: HashMap<String, (Vec<Ty>, Ty)>,
}

impl CLBackend {
    pub fn new() -> Result<Self> {
        let mut fb = settings::builder();
        fb.set("is_pic", "true")?;
        let flags = settings::Flags::new(fb);
        let isa = isa::lookup(Triple::host())?.finish(flags)?;
        let obj = ObjectBuilder::new(isa, "paw_obj".to_string(), default_libcall_names())?;
        Ok(Self {
            module: ObjectModule::new(obj),
            globals_val: HashMap::new(),
            str_pool: HashMap::new(),
            fn_sig: HashMap::new(),
        })
    }

    pub fn set_globals_from_program(&mut self, prog: &Program) {
        self.globals_val.clear();
        for it in &prog.items {
            if let Item::Global { name, ty, init, is_const } = it {
                if !*is_const { continue; }
                match (ty, init) {
                    (Ty::Int,    Expr::Int(n))    => { self.globals_val.insert(name.clone(), (Ty::Int,    GConst::I32(*n))); }
                    (Ty::Long,   Expr::Long(n))   => { self.globals_val.insert(name.clone(), (Ty::Long,   GConst::I64(*n))); }
                    (Ty::Bool,   Expr::Bool(b))   => { self.globals_val.insert(name.clone(), (Ty::Bool,   GConst::I8(if *b {1} else {0}))); }
                    (Ty::Char,   Expr::Char(u))   => { self.globals_val.insert(name.clone(), (Ty::Char,   GConst::I32(*u as i32))); }
                    (Ty::Float,  Expr::Float(x))  => { self.globals_val.insert(name.clone(), (Ty::Float,  GConst::F32(*x))); }
                    (Ty::Double, Expr::Double(x)) => { self.globals_val.insert(name.clone(), (Ty::Double, GConst::F64(*x))); }
                    // String/Void 或者非常量初始化都不内联
                    _ => {}
                }
            }
        }
    }

    pub fn declare_fns(&mut self, funs: &[FunDecl]) -> Result<HashMap<String, FuncId>> {
        let mut ids = HashMap::new();
        for f in funs {
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
            // 记录完整签名（参数 + 返回）
            self.fn_sig.insert(f.name.clone(), (f.params.iter().map(|(_, t)| t.clone()).collect(), f.ret.clone()));
        }
        Ok(ids)
    }

    pub fn define_fn(&mut self, f: &FunDecl, ids: &HashMap<String, FuncId>) -> Result<()> {
        if f.is_extern { return Ok(()); }

        // ---- 函数签名 ----
        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for (_, ty) in &f.params {
            sig.params.push(AbiParam::new(cl_ty(ty).ok_or_else(|| anyhow!("invalid param ty {:?}", ty))?));
        }
        if let Some(ret_t) = cl_ty(&f.ret) {
            sig.returns.push(AbiParam::new(ret_t));
        }

        // ---- 建 IR ----
        let mut ctx = self.module.make_context();
        ctx.func.signature = sig;
        let mut fb_ctx = FunctionBuilderContext::new();
        let mut b = FunctionBuilder::new(&mut ctx.func, &mut fb_ctx);

        let entry = b.create_block();
        b.append_block_params_for_function_params(entry);
        b.switch_to_block(entry);
        b.seal_block(entry);

        // 作用域：变量 -> SSA & 类型
        let mut scopes: Vec<HashMap<String, Variable>> = vec![HashMap::new()];
        let mut scopes_ty: Vec<HashMap<String, Ty>>     = vec![HashMap::new()];

        // 参数注入
        for (i, (name, ty)) in f.params.iter().enumerate() {
            let v = b.declare_var(cl_ty(ty).unwrap());
            let arg = b.block_params(entry)[i];
            b.def_var(v, arg);
            scopes.last_mut().unwrap().insert(name.clone(), v);
            scopes_ty.last_mut().unwrap().insert(name.clone(), ty.clone());
        }

        // 编译期全局常量注入
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
                    (Ty::String, _) | (Ty::Void, _) => unreachable!("not a const-literal here"),
                    _ => unreachable!("const type/value mismatch: {:?}/{:?}", gty, gval),
                };
                b.def_var(v, c);
                scope_vars.insert(gname.clone(), v);
                scope_types.insert(gname.clone(), gty.clone());
            }
        }

        // 生成函数体
        let mut cg = ExprGen {
            be: self,
            ids,
            scopes,
            scopes_ty,
            loop_stack: Vec::new(),
            fun_ret: f.ret.clone(),
        };

        // emit body，函数末尾兜底 return
        let ret_val = cg.emit_block(&mut b, &f.body)?;

        match f.ret {
            Ty::Void => {
                b.ins().return_(&[]);
            }
            _ => {
                let v = cg.coerce_to_ty(&mut b, ret_val, &f.ret);
                b.ins().return_(&[v]);
            }
        }

        b.finalize();

        // 回写模块
        let id = *ids.get(&f.name).ok_or_else(|| anyhow!("missing FuncId for `{}`", f.name))?;
        self.module.define_function(id, &mut ctx)?;
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

// Ty → CLIF Type（Void -> None）
fn cl_ty(t: &Ty) -> Option<ir::Type> {
    match t {
        Ty::Int    => Some(types::I32),
        Ty::Long   => Some(types::I64),
        Ty::Bool   => Some(types::I8),   // 表达式值/ABI 都用 i8
        Ty::Char   => Some(types::I32),  // u32 语义，ABI 用 i32
        Ty::Float  => Some(types::F32),
        Ty::Double => Some(types::F64),
        Ty::String => Some(types::I64),  // 句柄/指针
        Ty::Void   => None,
    }
}

struct ExprGen<'a> {
    be: &'a mut CLBackend,
    ids: &'a HashMap<String, FuncId>,
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

    // i8 -> b1
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

    // 把 IR value 转成目标 Paw 类型
    fn coerce_to_ty(&mut self, b:&mut FunctionBuilder, v: ir::Value, dst:&Ty)->ir::Value {
        use cranelift_codegen::ir::types::*;
        let want = cl_ty(dst).expect("no void here");
        let have = self.val_ty(b, v);
        if have == want { return v; }
        match (have, want) {
            (B1, I8) => self.bool_b1_to_i8(b, v),
            (I8, B1) => self.bool_i8_to_b1(b, v),

            (I32, I64) => b.ins().sextend(I64, v),
            (I64, I32) => b.ins().ireduce(I32, v),

            (F32, F64) => b.ins().fpromote(F64, v),
            (F64, F32) => b.ins().fdemote(F32, v),

            // 其他（跨数值族等）——保守给 0/0.0，避免崩
            (_, I8)  => b.ins().iconst(I8,  0),
            (_, I32) => b.ins().iconst(I32, 0),
            (_, I64) => b.ins().iconst(I64, 0),
            (_, F32) => b.ins().f32const(0.0),
            (_, F64) => b.ins().f64const(0.0),
            _ => v,
        }
    }

    fn coerce_value_to_irtype(
        &mut self,
        b: &mut FunctionBuilder,
        v: ir::Value,
        dst: ir::Type,
    ) -> ir::Value {
        use cranelift_codegen::ir::types::*;
        let src = b.func.dfg.value_type(v);
        if src == dst { return v; }

        // 同为整数：按位宽扩展/截断（有符号）
        if src.is_int() && dst.is_int() {
            return if src.bits() < dst.bits() {
                b.ins().sextend(dst, v)
            } else {
                b.ins().ireduce(dst, v)
            }
        }

        // 同为浮点：提升/降级精度
        if src.is_float() && dst.is_float() {
            return if src.bits() < dst.bits() {
                b.ins().fpromote(dst, v)
            } else {
                b.ins().fdemote(dst, v)
            }
        }

        // 其他跨族或不期望的组合：按目标类型补“零值”
        match dst {
            I8  => b.ins().iconst(I8,  0),
            I32 => b.ins().iconst(I32, 0),
            I64 => b.ins().iconst(I64, 0),
            F32 => b.ins().f32const(0.0),
            F64 => b.ins().f64const(0.0),
            _   => v, // 兜底（通常用不到）
        }
    }


    // 仅做位宽/精度统一（整型宽化到 i64；浮点宽化到 f64）
    fn unify_values_for_numeric(
        &mut self,
        b:&mut FunctionBuilder,
        mut l: ir::Value,
        mut r: ir::Value,
    )->(ir::Value, ir::Value, ir::Type){
        let lt = self.val_ty(b, l);
        let rt = self.val_ty(b, r);
        if lt == rt { return (l, r, lt); }
        match (lt, rt) {
            (types::I32, types::I64) => { l = b.ins().sextend(types::I64, l); (l, r, types::I64) }
            (types::I64, types::I32) => { r = b.ins().sextend(types::I64, r); (l, r, types::I64) }
            (types::F32, types::F64) => { l = b.ins().fpromote(types::F64, l); (l, r, types::F64) }
            (types::F64, types::F32) => { r = b.ins().fpromote(types::F64, r); (l, r, types::F64) }
            _ => (l, r, lt), // 其他组合按 typecheck 不应出现
        }
    }

    fn emit_block(&mut self, b:&mut FunctionBuilder, blk:&Block)->Result<ir::Value>{
        self.push();
        for s in &blk.stmts {
            match s {
                Stmt::Let { name, ty, init, .. } => {
                    let v0 = self.emit_expr(b, init)?;
                    let v  = match ty {
                        Ty::Bool => { // 归一 0/1
                            let b1 = self.bool_i8_to_b1(b, v0);
                            self.bool_b1_to_i8(b, b1)
                        }
                        _ => self.coerce_to_ty(b, v0, ty),
                    };
                    let var = self.declare_named(b, name, ty);
                    b.def_var(var, v);
                }

                Stmt::Assign { name, expr } => {
                    let var = self
                        .lookup(name)
                        .ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                    let ty = self
                        .lookup_ty(name)
                        .ok_or_else(|| anyhow!("no type for `{name}`"))?;
                    let rhs = self.emit_expr(b, expr)?;
                    let v = self.coerce_to_ty(b, rhs, &ty);

                    b.def_var(var, v);
                }

                Stmt::Expr(e) => { let _ = self.emit_expr(b, e)?; }

                Stmt::If { cond, then_b, else_b } => {
                    let cv  = self.emit_expr(b, cond)?;
                    let c1  = self.bool_i8_to_b1(b, cv);

                    let tbb = b.create_block();
                    let ebb = b.create_block();
                    let out = b.create_block();

                    b.ins().brif(c1, tbb, &[], ebb, &[]);

                    // then
                    b.switch_to_block(tbb);
                    b.seal_block(tbb);
                    let _ = self.emit_block(b, then_b)?;
                    b.ins().jump(out, &[]);

                    // else
                    b.switch_to_block(ebb);
                    b.seal_block(ebb);
                    if let Some(eb) = else_b { let _ = self.emit_block(b, eb)?; }
                    b.ins().jump(out, &[]);

                    // out
                    b.switch_to_block(out);
                    b.seal_block(out);
                }

                Stmt::While { cond, body } => {
                    let hdr = b.create_block();
                    let bb  = b.create_block();
                    let out = b.create_block();

                    b.ins().jump(hdr, &[]);
                    b.switch_to_block(hdr);

                    let cv = self.emit_expr(b, cond)?;
                    let c  = self.bool_i8_to_b1(b, cv);
                    b.ins().brif(c, bb, &[], out, &[]);

                    // body
                    b.switch_to_block(bb);
                    b.seal_block(bb);

                    self.loop_stack.push((hdr, out));
                    let _ = self.emit_block(b, body)?;
                    self.loop_stack.pop();

                    b.ins().jump(hdr, &[]);
                    b.seal_block(hdr);

                    b.switch_to_block(out);
                    b.seal_block(out);
                }

                Stmt::For { init, cond, step, body } => {
                    if let Some(fi) = init {
                        match fi {
                            ForInit::Let { name, ty, init, .. } => {
                                let rhs = self.emit_expr(b, init)?;
                                let v = self.coerce_to_ty(b, rhs, ty);
                                let var = self.declare_named(b, name, ty);
                                b.def_var(var, v);
                            }
                            ForInit::Assign { name, expr } => {
                                let (var, ty) = {
                                    let v  = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                                    (v, ty)
                                };
                                let rhs = self.emit_expr(b, expr)?;
                                let v   = self.coerce_to_ty(b, rhs, &ty);
                                b.def_var(var, v);
                            }
                            ForInit::Expr(e) => { let _ = self.emit_expr(b, e)?; }
                        }
                    }

                    let hdr   = b.create_block();
                    let stepb = b.create_block();
                    let bodyb = b.create_block();
                    let out   = b.create_block();

                    b.ins().jump(hdr, &[]);
                    b.switch_to_block(hdr);

                    let cval = if let Some(c) = cond {
                        let cv = self.emit_expr(b, c)?;
                        self.bool_i8_to_b1(b, cv)
                    } else {
                        let one = b.ins().iconst(types::I8, 1);
                        self.bool_i8_to_b1(b, one)
                    };
                    b.ins().brif(cval, bodyb, &[], out, &[]);

                    b.switch_to_block(bodyb);
                    b.seal_block(bodyb);

                    self.loop_stack.push((stepb, out));
                    let _ = self.emit_block(b, body)?;
                    self.loop_stack.pop();

                    b.ins().jump(stepb, &[]);

                    b.switch_to_block(stepb);
                    b.seal_block(stepb);

                    if let Some(st) = step {
                        match st {
                            ForStep::Assign { name, expr } => {
                                let (var, ty) = {
                                    let v  = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                                    (v, ty)
                                };
                                let rhs = self.emit_expr(b, expr)?;
                                let v   = self.coerce_to_ty(b, rhs, &ty);
                                b.def_var(var, v);
                            }
                            ForStep::Expr(e) => { let _ = self.emit_expr(b, e)?; }
                        }
                    }

                    b.ins().jump(hdr, &[]);
                    b.seal_block(hdr);

                    b.switch_to_block(out);
                    b.seal_block(out);
                }

                Stmt::Break => {
                    let &(_, out) = self.loop_stack.last().ok_or_else(|| anyhow!("`break` outside loop"))?;
                    b.ins().jump(out, &[]);
                    let cont = b.create_block();
                    b.switch_to_block(cont);
                    b.seal_block(cont);
                }

                Stmt::Continue => {
                    let &(cont_tgt, _) = self.loop_stack.last().ok_or_else(|| anyhow!("`continue` outside loop"))?;
                    b.ins().jump(cont_tgt, &[]);
                    let cont = b.create_block();
                    b.switch_to_block(cont);
                    b.seal_block(cont);
                }

                Stmt::Return(opt) => {
                    match (&self.fun_ret, opt) {
                        (Ty::Void, Some(e)) => {
                            let _ = self.emit_expr(b, e)?;
                            let _ = b.ins().return_(&[]);
                        }
                        (_ret, Some(e)) => {
                            let ret_ty = self.fun_ret.clone();
                            let raw    = self.emit_expr(b, e)?;
                            let v      = self.coerce_to_ty(b, raw, &ret_ty);
                            b.ins().return_(&[v]);
                        }
                        (_ret, None) => {
                            match self.fun_ret {
                                Ty::Bool => {
                                    let z = b.ins().iconst(types::I8, 0);
                                    b.ins().return_(&[z]);
                                }
                                Ty::Int | Ty::Char => {
                                    let z = b.ins().iconst(types::I32, 0);
                                    b.ins().return_(&[z]);
                                }
                                Ty::Long | Ty::String => {
                                    let z = b.ins().iconst(types::I64, 0);
                                    b.ins().return_(&[z]);
                                }
                                Ty::Float => {
                                    let z = b.ins().f32const(0.0);
                                    b.ins().return_(&[z]);
                                }
                                Ty::Double => {
                                    let z = b.ins().f64const(0.0);
                                    b.ins().return_(&[z]);
                                }
                                Ty::Void => {
                                    b.ins().return_(&[]);
                                }
                            }
                        }
                    }
                    // return 后开新块，容忍死代码
                    let cont = b.create_block();
                    b.switch_to_block(cont);
                    b.seal_block(cont);
                }
            }
        }
        // 块表达式值：无 tail 则 i32 0（与 typecheck 约定一致）
        let out = match &blk.tail { Some(e) => self.emit_expr(b, e)?, None => b.ins().iconst(types::I32, 0) };
        self.pop();
        Ok(out)
    }

    fn emit_expr(&mut self, b:&mut FunctionBuilder, e:&Expr)->Result<ir::Value>{
        use BinOp::*; use UnOp::*;
        Ok(match e {
            // 字面量
            Expr::Int(n)    => b.ins().iconst(types::I32, *n as i64),
            Expr::Long(n)   => b.ins().iconst(types::I64, *n),
            Expr::Float(x)  => b.ins().f32const(*x),
            Expr::Double(x) => b.ins().f64const(*x),
            Expr::Char(u)   => b.ins().iconst(types::I32, *u as i64),
            Expr::Bool(bi)  => b.ins().iconst(types::I8, if *bi {1} else {0}),
            Expr::Str(s)    => {
                let did = self.be.intern_str(s)?;
                let gv  = self.be.module.declare_data_in_func(did, b.func);
                b.ins().global_value(types::I64, gv)
            }

            // 变量
            Expr::Var(name) => {
                let v = self.lookup(name).ok_or_else(|| anyhow!("unknown var in codegen `{name}`"))?;
                b.use_var(v)
            }

            // 一元
            Expr::Unary{op, rhs} => {
                let r = self.emit_expr(b, rhs)?;
                let rt = self.val_ty(b, r);
                match op {
                    Neg => if rt.is_int() { b.ins().ineg(r) } else { b.ins().fneg(r) },
                    Not => {
                        let b1 = self.bool_i8_to_b1(b, r);
                        let nb = b.ins().bnot(b1);
                        self.bool_b1_to_i8(b, nb)
                    }
                }
            }

            // 二元
            Expr::Binary{op,lhs,rhs}=>{
                let l0 = self.emit_expr(b, lhs)?;
                let r0 = self.emit_expr(b, rhs)?;
                match op {
                    And => {
                        let l1 = self.bool_i8_to_b1(b, l0);
                        let r1 = self.bool_i8_to_b1(b, r0);
                        let both = b.ins().band(l1, r1);
                        self.bool_b1_to_i8(b, both)
                    }
                    Or => {
                        let l1 = self.bool_i8_to_b1(b, l0);
                        let r1 = self.bool_i8_to_b1(b, r0);
                        let any = b.ins().bor(l1, r1);
                        self.bool_b1_to_i8(b, any)
                    }
                    Add | Sub | Mul | Div | Lt | Le | Gt | Ge | Eq | Ne => {
                        let (l, r, ty) = self.unify_values_for_numeric(b, l0, r0);
                        match op {
                            Add => if ty.is_int() { b.ins().iadd(l,r) } else { b.ins().fadd(l,r) },
                            Sub => if ty.is_int() { b.ins().isub(l,r) } else { b.ins().fsub(l,r) },
                            Mul => if ty.is_int() { b.ins().imul(l,r) } else { b.ins().fmul(l,r) },
                            Div => if ty.is_int() { b.ins().sdiv(l,r) } else { b.ins().fdiv(l,r) },
                            Lt | Le | Gt | Ge => {
                                let b1 = if ty.is_int() {
                                    let cc = match op {
                                        Lt => IntCC::SignedLessThan,
                                        Le => IntCC::SignedLessThanOrEqual,
                                        Gt => IntCC::SignedGreaterThan,
                                        Ge => IntCC::SignedGreaterThanOrEqual,
                                        _ => unreachable!(),
                                    };
                                    b.ins().icmp(cc, l, r)
                                } else {
                                    let cc = match op {
                                        Lt => FloatCC::LessThan,
                                        Le => FloatCC::LessThanOrEqual,
                                        Gt => FloatCC::GreaterThan,
                                        Ge => FloatCC::GreaterThanOrEqual,
                                        _ => unreachable!(),
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
                            _ => unreachable!(),
                        }
                    }
                }
            }

            // 表达式 if
            Expr::If { cond, then_b, else_b } => {
                // 条件 i8 -> b1
                let cv = self.emit_expr(b, cond)?;
                let c1 = self.bool_i8_to_b1(b, cv);

                let tbb = b.create_block();
                let ebb = b.create_block();
                let mb  = b.create_block();

                b.ins().brif(c1, tbb, &[], ebb, &[]);

                // then
                b.switch_to_block(tbb);
                b.seal_block(tbb);
                let tv0   = self.emit_block(b, then_b)?;
                let tv_ty = self.val_ty(b, tv0);        // 以 then 的 IR 类型作为 φ 的目标类型
                let phi   = b.declare_var(tv_ty);       // 声明“φ变量”
                b.def_var(phi, tv0);
                b.ins().jump(mb, &[]);

                // else
                b.switch_to_block(ebb);
                b.seal_block(ebb);
                let ev0 = self.emit_block(b, else_b)?;
                let ev  = self.coerce_value_to_irtype(b, ev0, tv_ty); // else -> then 的类型
                b.def_var(phi, ev);
                b.ins().jump(mb, &[]);

                // merge
                b.switch_to_block(mb);
                b.seal_block(mb);
                b.use_var(phi)
            }

            // match
            Expr::Match { scrut, arms, default } => {
                let sv = self.emit_expr(b, scrut)?;
                let out = b.create_block();
                let mut phi: Option<Variable> = None;

                let mut next: Option<ir::Block> = None;
                for (pat, blk) in arms {
                    let this  = b.create_block();
                    let thenb = b.create_block();
                    let cont  = b.create_block();

                    if let Some(prev) = next.take() {
                        b.switch_to_block(prev);
                        b.seal_block(prev);
                        b.ins().jump(this, &[]);
                    } else {
                        b.ins().jump(this, &[]);
                    }

                    b.switch_to_block(this);
                    b.seal_block(this);

                    let hit_b1 = match pat {
                        Pattern::Int(n) => {
                            let cn = b.ins().iconst(types::I32, *n as i64);
                            let s  = if self.val_ty(b, sv) == types::I32 { sv } else { b.ins().ireduce(types::I32, sv) };
                            b.ins().icmp(IntCC::Equal, s, cn)
                        }
                        Pattern::Long(n) => {
                            let cn = b.ins().iconst(types::I64, *n);
                            let s  = if self.val_ty(b, sv) == types::I64 { sv } else { b.ins().sextend(types::I64, sv) };
                            b.ins().icmp(IntCC::Equal, s, cn)
                        }
                        Pattern::Char(u) => {
                            let cn = b.ins().iconst(types::I32, *u as i64);
                            let s  = if self.val_ty(b, sv) == types::I32 { sv } else { b.ins().ireduce(types::I32, sv) };
                            b.ins().icmp(IntCC::Equal, s, cn)
                        }
                        Pattern::Bool(bt) => {
                            let cn = b.ins().iconst(types::I8, if *bt {1} else {0});
                            let s  = if self.val_ty(b, sv) == types::I8 { sv } else { b.ins().ireduce(types::I8, sv) };
                            b.ins().icmp(IntCC::Equal, s, cn)
                        }
                        Pattern::Wild => {
                            let one = b.ins().iconst(types::I8, 1);
                            self.bool_i8_to_b1(b, one)
                        }
                    };
                    b.ins().brif(hit_b1, thenb, &[], cont, &[]);

                    // then
                    b.switch_to_block(thenb);
                    b.seal_block(thenb);
                    let v = self.emit_block(b, blk)?;
                    if phi.is_none() {
                        let vt = self.val_ty(b, v);
                        phi = Some(b.declare_var(vt));
                    }
                    b.def_var(phi.unwrap(), v);
                    b.ins().jump(out, &[]);

                    next = Some(cont);
                }

                if let Some(last) = next.take() {
                    b.switch_to_block(last);
                    b.seal_block(last);
                    if let Some(db) = default {
                        let v = self.emit_block(b, db)?;
                        if phi.is_none() {
                            let vt = self.val_ty(b, v);
                            phi = Some(b.declare_var(vt));
                        }
                        b.def_var(phi.unwrap(), v);
                        b.ins().jump(out, &[]);
                    } else {
                        if phi.is_none() { phi = Some(b.declare_var(types::I32)); }
                        let z = b.ins().iconst(types::I32, 0);
                        b.def_var(phi.unwrap(), z);
                        b.ins().jump(out, &[]);
                    }
                }

                b.switch_to_block(out);
                b.seal_block(out);
                b.use_var(phi.unwrap())
            }

            // 调用
            Expr::Call { callee, args } => {
                let fid  = *self.ids.get(callee).ok_or_else(|| anyhow!("unknown fn `{callee}`"))?;
                let fref = self.be.module.declare_func_in_func(fid, b.func);

                let (param_tys, ret_ty) = self.be.fn_sig.get(callee)
                    .ok_or_else(|| anyhow!("missing signature for `{callee}`"))?
                    .clone();

                if param_tys.len() != args.len() {
                    return Err(anyhow!("function `{}` expects {} args, got {}", callee, param_tys.len(), args.len()));
                }

                let mut av = Vec::with_capacity(args.len());
                for (a, pt) in args.iter().zip(param_tys.iter()) {
                    let v0 = self.emit_expr(b, a)?;
                    let v  = self.coerce_to_ty(b, v0, pt);
                    av.push(v);
                }

                let call = b.ins().call(fref, &av);
                if matches!(ret_ty, Ty::Void) {
                    // 容错：表达式位置不应调用 void；给个 0（i32）
                    b.ins().iconst(types::I32, 0)
                } else {
                    b.inst_results(call)[0]
                }
            }

            // 块表达式
            Expr::Block(blk) => self.emit_block(b, blk)?,
        })
    }
}
