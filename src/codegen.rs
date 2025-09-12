use crate::ast::*;
use anyhow::Result;
use cranelift_codegen::{
    ir::{self, AbiParam, InstBuilder, types},
    isa, settings,
};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{
    DataDescription as DataContext, DataId, FuncId, Linkage, Module, default_libcall_names,
};
use cranelift_object::{ObjectBuilder, ObjectModule};
use std::collections::HashMap;
use cranelift_codegen::settings::Configurable;
use target_lexicon::Triple;

pub struct CLBackend {
    pub module: ObjectModule,
    pub globals_val: HashMap<String, i64>,
    str_pool: HashMap<String, DataId>,
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
        })
    }

    pub fn set_globals_from_program(&mut self, prog: &Program) {
        for it in &prog.items {
            if let Item::Global { name, init, is_const, .. } = it {
                if !*is_const { continue; }
                match init {
                    Expr::Int(n)  => { self.globals_val.insert(name.clone(), *n); }
                    Expr::Bool(b) => { self.globals_val.insert(name.clone(), if *b {1} else {0}); }
                    _ => {}
                }
            }
        }
    }

    pub fn declare_fns(&mut self, funs: &[FunDecl]) -> Result<HashMap<String, FuncId>> {
        let mut ids = HashMap::new();
        for f in funs {
            let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
            for (_, ty) in &f.params { sig.params.push(AbiParam::new(cl_ty(ty))); }
            sig.returns.push(AbiParam::new(cl_ty(&f.ret)));
            let linkage = if f.is_extern { Linkage::Import } else { Linkage::Export };
            let id = self.module.declare_function(&f.name, linkage, &sig)?;
            ids.insert(f.name.clone(), id);
        }
        Ok(ids)
    }

    pub fn define_fn(&mut self, f: &FunDecl, ids: &HashMap<String, FuncId>) -> Result<()> {
        if f.is_extern { return Ok(()); }

        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for (_, ty) in &f.params { sig.params.push(AbiParam::new(cl_ty(ty))); }
        sig.returns.push(AbiParam::new(cl_ty(&f.ret)));

        let mut ctx = self.module.make_context();
        ctx.func.signature = sig;
        let mut fb_ctx = FunctionBuilderContext::new();
        let mut b = FunctionBuilder::new(&mut ctx.func, &mut fb_ctx);

        let entry = b.create_block();
        b.append_block_params_for_function_params(entry);
        b.switch_to_block(entry);
        b.seal_block(entry);

        let mut scopes: Vec<HashMap<String, Variable>> = vec![HashMap::new()];

        for (i,(name,_)) in f.params.iter().enumerate() {
            let var = b.declare_var(types::I64);
            let arg = b.block_params(entry)[i];
            b.def_var(var, arg);
            scopes.last_mut().unwrap().insert(name.clone(), var);
        }
        {
            let scope0 = scopes.last_mut().unwrap();
            for (gname,gval) in &self.globals_val {
                let v = b.declare_var(types::I64);
                let c = b.ins().iconst(types::I64, *gval);
                b.def_var(v, c);
                scope0.insert(gname.clone(), v);
            }
        }

        let mut cg = ExprGen {
            be: self,
            ids,
            scopes,
            loop_stack: Vec::new(),
        };
        let mut ret_val = cg.emit_block(&mut b, &f.body)?;
        if matches!(f.ret, Ty::Bool) {
            ret_val = cg.as_bool(&mut b, ret_val);
            ret_val = cg.bool_to_i64(&mut b, ret_val);
        }
        b.ins().return_(&[ret_val]);
        b.finalize();

        let id = *ids.get(&f.name).ok_or_else(|| anyhow::anyhow!("missing FuncId for `{}`", f.name))?;
        self.module.define_function(id, &mut ctx)?;
        self.module.clear_context(&mut ctx);
        Ok(())
    }

    pub fn finish(self) -> Result<Vec<u8>> {
        let obj = self.module.finish();
        Ok(obj.emit()?)
    }

    fn intern_str(&mut self, s:&str) -> Result<DataId> {
        if let Some(&id) = self.str_pool.get(s) { return Ok(id); }
        let mut dc = DataContext::new();
        let mut bytes = s.as_bytes().to_vec(); bytes.push(0);
        dc.define(bytes.into_boxed_slice());
        let name = format!("__str_{}", self.str_pool.len());
        let id = self.module.declare_data(&name, Linkage::Local, true, false)?;
        self.module.define_data(id, &dc)?;
        self.str_pool.insert(s.to_string(), id);
        Ok(id)
    }
}

fn cl_ty(t:&Ty)->ir::Type {
    match t {
        Ty::Int    => types::I64,
        Ty::Bool   => types::I64, // 统一用 I64 避免 extern i8/i64 不匹配
        Ty::String => types::I64,
    }
}

struct ExprGen<'a> {
    be: &'a mut CLBackend,
    ids: &'a HashMap<String, FuncId>,
    scopes: Vec<HashMap<String, Variable>>,
    // (continue_target, break_target)
    loop_stack: Vec<(ir::Block, ir::Block)>,
}

impl<'a> ExprGen<'a> {
    fn push(&mut self){ self.scopes.push(HashMap::new()); }
    fn pop(&mut self){ self.scopes.pop(); }

    fn declare_named(&mut self, b:&mut FunctionBuilder, name:&str) -> Variable {
        let v = b.declare_var(types::I64);
        self.scopes.last_mut().unwrap().insert(name.to_string(), v);
        v
    }
    fn lookup(&self, name:&str)->Option<Variable>{
        for s in self.scopes.iter().rev(){ if let Some(v)=s.get(name){ return Some(*v); } }
        None
    }
    fn as_bool(&mut self, b:&mut FunctionBuilder, v:ir::Value)->ir::Value{
        let z = b.ins().iconst(types::I64, 0);
        b.ins().icmp(ir::condcodes::IntCC::NotEqual, v, z)
    }
    fn bool_to_i64(&mut self, b:&mut FunctionBuilder, v1:ir::Value)->ir::Value{
        let o = b.ins().iconst(types::I64, 1);
        let z = b.ins().iconst(types::I64, 0);
        b.ins().select(v1, o, z)
    }

    fn emit_block(&mut self, b:&mut FunctionBuilder, blk:&Block) -> Result<ir::Value> {
        self.push();
        for s in &blk.stmts {
            match s {
                Stmt::Let { name, ty, init, .. } => {
                    let v = self.emit_expr(b, init)?;
                    let sv = match ty {
                        Ty::Int | Ty::String => v,
                        Ty::Bool => {
                            let b1 = self.as_bool(b, v);
                            self.bool_to_i64(b, b1)
                        }
                    };
                    let var = self.declare_named(b, name);
                    b.def_var(var, sv);
                }

                Stmt::Assign { name, expr } => {
                    let v = self.emit_expr(b, expr)?;
                    let var = self
                        .lookup(name)
                        .ok_or_else(|| anyhow::anyhow!("unknown var `{name}`"))?;
                    b.def_var(var, v);
                }

                Stmt::Expr(e) => { let _ = self.emit_expr(b, e)?; }

                Stmt::If { cond, then_b, else_b } => {
                    let cv = self.emit_expr(b, cond)?;
                    let c1 = self.as_bool(b, cv);

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
                    // ❌ 不要在这里 seal hdr（还有回边会指向它）

                    let cv = self.emit_expr(b, cond)?;
                    let c  = self.as_bool(b, cv);
                    b.ins().brif(c, bb, &[], out, &[]);

                    // body
                    b.switch_to_block(bb);
                    b.seal_block(bb); // bb 的前驱已确定（来自 hdr）

                    self.loop_stack.push((hdr, out));
                    let _ = self.emit_block(b, body)?;
                    self.loop_stack.pop();

                    // 回边
                    b.ins().jump(hdr, &[]);

                    // 现在 hdr 的所有前驱（入口 + 回边）都已确定，seal 它
                    b.seal_block(hdr);

                    // 出口：等所有 break 都可能发出后，再 seal
                    b.switch_to_block(out);
                    b.seal_block(out);
                }


                Stmt::For { init, cond, step, body } => {
                    if let Some(fi) = init {
                        match fi {
                            ForInit::Let { name, ty: _, init, is_const: _ } => {
                                let v = self.emit_expr(b, init)?;
                                let var = self.declare_named(b, name);
                                b.def_var(var, v);
                            }
                            ForInit::Assign { name, expr } => {
                                let v = self.emit_expr(b, expr)?;
                                let var = self.lookup(name).ok_or_else(|| anyhow::anyhow!("unknown var `{name}`"))?;
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
                    // ❌ 不要 seal hdr

                    let cval = if let Some(c) = cond {
                        let cv = self.emit_expr(b, c)?;
                        self.as_bool(b, cv)
                    } else {
                        let one = b.ins().iconst(types::I64, 1);
                        self.as_bool(b, one)
                    };
                    b.ins().brif(cval, bodyb, &[], out, &[]);

                    // body
                    b.switch_to_block(bodyb);
                    b.seal_block(bodyb);

                    // for 的 continue 跳到 stepb，break 跳到 out
                    self.loop_stack.push((stepb, out));
                    let _ = self.emit_block(b, body)?;
                    self.loop_stack.pop();

                    // 体结束先去 step
                    b.ins().jump(stepb, &[]);

                    // step
                    b.switch_to_block(stepb);
                    // 这里此时 stepb 的所有可能前驱（普通路径 + continue）都已经发出，可以 seal
                    b.seal_block(stepb);

                    if let Some(st) = step {
                        match st {
                            ForStep::Assign { name, expr } => {
                                let v = self.emit_expr(b, expr)?;
                                let var = self.lookup(name).ok_or_else(|| anyhow::anyhow!("unknown var `{name}`"))?;
                                b.def_var(var, v);
                            }
                            ForStep::Expr(e) => { let _ = self.emit_expr(b, e)?; }
                        }
                    }
                    // 回边到 hdr
                    b.ins().jump(hdr, &[]);

                    // 现在 hdr 的所有前驱都已确定（入口 + 回边），可以 seal
                    b.seal_block(hdr);

                    // 出口：等所有 break 都可能发出后再 seal
                    b.switch_to_block(out);
                    b.seal_block(out);
                }


                Stmt::Break => {
                    let &(_, out) = self.loop_stack.last().ok_or_else(|| anyhow::anyhow!("`break` outside loop"))?;
                    b.ins().jump(out, &[]);
                    let cont = b.create_block();
                    b.switch_to_block(cont);
                    b.seal_block(cont);
                }

                Stmt::Continue => {
                    let &(cont_tgt, _) = self.loop_stack.last().ok_or_else(|| anyhow::anyhow!("`continue` outside loop"))?;
                    b.ins().jump(cont_tgt, &[]);
                    let cont = b.create_block();
                    b.switch_to_block(cont);
                    b.seal_block(cont);
                }

                Stmt::Return(opt) => {
                    let rv = if let Some(e)=opt { self.emit_expr(b, e)? } else { b.ins().iconst(types::I64, 0) };
                    b.ins().return_(&[rv]);
                    let cont = b.create_block();
                    b.switch_to_block(cont);
                    b.seal_block(cont);
                }
            }
        }
        let out = match &blk.tail { Some(e)=>self.emit_expr(b, e)?, None=>b.ins().iconst(types::I64, 0) };
        self.pop(); Ok(out)
    }

    fn emit_expr(&mut self, b:&mut FunctionBuilder, e:&Expr)->Result<ir::Value>{
        use BinOp::*; use UnOp::*;
        Ok(match e {
            Expr::Int(n)=> b.ins().iconst(types::I64, *n),
            Expr::Bool(x)=> b.ins().iconst(types::I64, if *x {1} else {0}),
            Expr::Str(s) => {
                // 1) 仍然将字面量驻留到 .rodata，拿到 C 字符串指针
                let did = self.be.intern_str(s)?;
                let gv  = self.be.module.declare_data_in_func(did, b.func);
                let cptr = b.ins().global_value(types::I64, gv);

                // 2) 调用运行时：String paw_string_from_cstr(Int cptr)
                //    prelude 中已 extern 了 paw_string_from_cstr，所以 ids 里应该有它
                if let Some(fid) = self.ids.get("paw_string_from_cstr") {
                    let fref = self.be.module.declare_func_in_func(*fid, b.func);
                    let call = b.ins().call(fref, &[cptr]);
                    b.inst_results(call)[0]   // 返回真正的 String 句柄（I64）
                } else {
                    // 兜底：如果没有导入 prelude，就退回到原始指针（不推荐）
                    cptr
                }
            }
            Expr::Var(name)=>{
                let v=self.lookup(name).ok_or_else(||anyhow::anyhow!("unknown var in codegen `{name}`"))?;
                b.use_var(v)
            }
            Expr::Unary { op, rhs } => {
                let r = self.emit_expr(b, rhs)?;
                match op {
                    Neg => b.ins().ineg(r),
                    Not => {
                        let c1 = self.as_bool(b, r);
                        let nb = b.ins().bnot(c1);
                        self.bool_to_i64(b, nb)
                    }
                }
            }
            Expr::Binary{op,lhs,rhs}=>{
                let l=self.emit_expr(b, lhs)?; let r=self.emit_expr(b, rhs)?;
                match op {
                    Add=>b.ins().iadd(l,r), Sub=>b.ins().isub(l,r), Mul=>b.ins().imul(l,r), Div=>b.ins().sdiv(l,r),
                    Lt|Le|Gt|Ge|Eq|Ne => {
                        let cc=match op {
                            Lt=>ir::condcodes::IntCC::SignedLessThan,
                            Le=>ir::condcodes::IntCC::SignedLessThanOrEqual,
                            Gt=>ir::condcodes::IntCC::SignedGreaterThan,
                            Ge=>ir::condcodes::IntCC::SignedGreaterThanOrEqual,
                            Eq=>ir::condcodes::IntCC::Equal,
                            Ne=>ir::condcodes::IntCC::NotEqual,
                            _=>unreachable!()
                        };
                        let b1=b.ins().icmp(cc,l,r); self.bool_to_i64(b,b1)
                    }
                    And => {
                        let l1 = self.as_bool(b, l);
                        let r1 = self.as_bool(b, r);
                        let both = b.ins().band(l1, r1);
                        self.bool_to_i64(b, both)
                    }
                    Or => {
                        let l1 = self.as_bool(b, l);
                        let r1 = self.as_bool(b, r);
                        let any = b.ins().bor(l1, r1);
                        self.bool_to_i64(b, any)
                    }
                }
            }
            Expr::If { cond, then_b, else_b } => {
                let cv  = self.emit_expr(b, cond)?;
                let c1  = self.as_bool(b, cv);

                let tbb = b.create_block();
                let ebb = b.create_block();
                let mb  = b.create_block();

                let phi = b.declare_var(types::I64);

                b.ins().brif(c1, tbb, &[], ebb, &[]);

                // then
                b.switch_to_block(tbb);
                b.seal_block(tbb);
                let tv = self.emit_block(b, then_b)?;
                b.def_var(phi, tv);
                b.ins().jump(mb, &[]);

                // else
                b.switch_to_block(ebb);
                b.seal_block(ebb);
                let ev = self.emit_block(b, else_b)?;
                b.def_var(phi, ev);
                b.ins().jump(mb, &[]);

                // merge
                b.switch_to_block(mb);
                b.seal_block(mb);
                b.use_var(phi)
            }
            Expr::Match { scrut, arms, default } => {
                let sv = self.emit_expr(b, scrut)?;
                let phi = b.declare_var(types::I64);
                let out = b.create_block();

                let mut next: Option<ir::Block> = None;
                for (pat, blk) in arms {
                    let this = b.create_block();
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

                    let hit = match pat {
                        Pattern::Int(n)  => {
                            let cn = b.ins().iconst(types::I64, *n);
                            b.ins().icmp(ir::condcodes::IntCC::Equal, sv, cn)
                        }
                        Pattern::Bool(bt) => {
                            let cn = b.ins().iconst(types::I64, if *bt {1} else {0});
                            b.ins().icmp(ir::condcodes::IntCC::Equal, sv, cn)
                        }
                        Pattern::Wild => {
                            let one = b.ins().iconst(types::I64, 1);
                            self.as_bool(b, one)
                        }
                    };
                    b.ins().brif(hit, thenb, &[], cont, &[]);

                    b.switch_to_block(thenb);
                    b.seal_block(thenb);
                    let v = self.emit_block(b, blk)?;
                    b.def_var(phi, v);
                    b.ins().jump(out, &[]);

                    next = Some(cont);
                }

                if let Some(last) = next.take() {
                    b.switch_to_block(last);
                    b.seal_block(last);
                    if let Some(db) = default {
                        let v = self.emit_block(b, db)?;
                        b.def_var(phi, v);
                        b.ins().jump(out, &[]);
                    } else {
                        let z = b.ins().iconst(types::I64, 0);
                        b.def_var(phi, z);
                        b.ins().jump(out, &[]);
                    }
                }

                b.switch_to_block(out);
                b.seal_block(out);
                b.use_var(phi)
            }
            Expr::Call { callee, args } => {
                let fid = *self
                    .ids
                    .get(callee)
                    .ok_or_else(|| anyhow::anyhow!("unknown fn `{callee}`"))?;
                let fref = self.be.module.declare_func_in_func(fid, b.func);
                let mut av = Vec::with_capacity(args.len());
                for a in args {
                    av.push(self.emit_expr(b, a)?);
                }
                let call = b.ins().call(fref, &av);
                b.inst_results(call)[0]
            }
            Expr::Block(blk) => self.emit_block(b, blk)?,
        })
    }
}
