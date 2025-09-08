use anyhow::Result;
use std::collections::HashMap;

use cranelift_codegen::{
    ir::{self, types, AbiParam, InstBuilder},
    isa, settings,
};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{default_libcall_names, FuncId, Linkage, Module};
use cranelift_object::{ObjectBuilder, ObjectModule};
use target_lexicon::Triple;

use crate::ast::*;

/// Cranelift 后端：生成 .obj，由系统链接器/clang 链接为可执行
pub struct CLBackend {
    pub module: ObjectModule,
    /// 收集到的全局常量的“运行期值”（只支持字面量；Bool → 0/1）
    pub globals_val: HashMap<String, i64>,
}

impl CLBackend {
    pub fn new() -> Result<Self> {
        let flags = settings::Flags::new(settings::builder());
        let isa = isa::lookup(Triple::host())?.finish(flags)?;
        let obj = ObjectBuilder::new(isa, "paw_obj".to_string(), default_libcall_names())?;
        Ok(Self {
            module: ObjectModule::new(obj),
            globals_val: HashMap::new(),
        })
    }

    /// 从顶层语句收集全局常量的值（仅支持字面量 Int/Bool）
    pub fn set_globals_from_tops(&mut self, tops: &[Stmt]) {
        for s in tops {
            if let Stmt::Let { name, init, is_const, .. } = s {
                if !*is_const { continue; }
                match init {
                    Expr::Int(n)  => { self.globals_val.insert(name.clone(), *n); }
                    Expr::Bool(b) => { self.globals_val.insert(name.clone(), if *b { 1 } else { 0 }); }
                    _ => { /* 需要支持复杂常量可在此扩展常量折叠 */ }
                }
            }
        }
    }

    /// 第一遍：仅声明函数，获取 FuncId
    pub fn declare_fns(&mut self, funs: &[FunDecl]) -> Result<HashMap<String, FuncId>> {
        let mut ids = HashMap::new();
        for f in funs {
            let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
            for (_, ty) in &f.params {
                sig.params.push(AbiParam::new(cl_ty(ty)));
            }
            sig.returns.push(AbiParam::new(cl_ty(&f.ret)));
            let id = self.module.declare_function(&f.name, Linkage::Export, &sig)?;
            ids.insert(f.name.clone(), id);
        }
        Ok(ids)
    }

    /// 第二遍：定义函数体
    pub fn define_fn(&mut self, f: &FunDecl, ids: &HashMap<String, FuncId>) -> Result<()> {
        // 函数签名
        let mut sig = ir::Signature::new(self.module.isa().default_call_conv());
        for (_, ty) in &f.params {
            sig.params.push(AbiParam::new(cl_ty(ty)));
        }
        sig.returns.push(AbiParam::new(cl_ty(&f.ret)));

        // 上下文 + Builder
        let mut ctx = self.module.make_context();
        ctx.func.signature = sig;
        let mut fb_ctx = FunctionBuilderContext::new();
        let mut b = FunctionBuilder::new(&mut ctx.func, &mut fb_ctx);

        // 入口块
        let entry = b.create_block();
        b.append_block_params_for_function_params(entry);
        b.switch_to_block(entry);
        b.seal_block(entry);

        // 作用域：变量名 -> Variable
        let mut scopes: Vec<HashMap<String, Variable>> = vec![HashMap::new()];

        // 形参入作用域（统一按 I64 存；Bool 以 0/1）
        for (i, (name, _ty)) in f.params.iter().enumerate() {
            let var = b.declare_var(types::I64);
            let arg = b.block_params(entry)[i];
            b.def_var(var, arg);
            scopes.last_mut().unwrap().insert(name.clone(), var);
        }

        // 注入全局常量（成为最外层 scope 的已定义“局部”）
        {
            let scope0 = scopes.last_mut().unwrap();
            for (gname, gval) in &self.globals_val {
                let var = b.declare_var(types::I64);
                let cst = b.ins().iconst(types::I64, *gval);
                b.def_var(var, cst);
                scope0.insert(gname.clone(), var);
            }
        }

        // 生成函数体
        let mut cg = ExprGen { module: &mut self.module, ids, scopes, ret_ty: f.ret.clone() };
        let mut ret_val_i64 = cg.emit_block(&mut b, &f.body)?; // 表达式层统一 i64
        // 若函数声明返回 Bool(I1)，把 i64(0/1) 转为 b1
        if matches!(f.ret, Ty::Bool) {
            ret_val_i64 = cg.as_bool(&mut b, ret_val_i64);
        }
        b.ins().return_(&[ret_val_i64]);

        // 结束构建
        b.finalize();

        let id = *ids
            .get(&f.name)
            .ok_or_else(|| anyhow::anyhow!("missing FuncId for `{}`", f.name))?;
        self.module.define_function(id, &mut ctx)?;
        self.module.clear_context(&mut ctx);
        Ok(())
    }

    /// 导出目标文件字节（COFF/ELF/Mach-O 由平台决定）
    pub fn finish(self) -> Result<Vec<u8>> {
        let obj = self.module.finish();
        Ok(obj.emit()?)
    }
}

/// 语言类型到 Cranelift IR 类型
fn cl_ty(t: &Ty) -> ir::Type {
    match t {
        Ty::Int  => types::I64,
        Ty::Bool => types::I8,
    }
}

struct ExprGen<'a> {
    module: &'a mut ObjectModule,
    ids: &'a HashMap<String, FuncId>,
    scopes: Vec<HashMap<String, Variable>>,
    /// 当前函数的返回类型（用于处理 `return` 的正确返回类型）
    ret_ty: Ty,
}

impl<'a> ExprGen<'a> {
    fn push_scope(&mut self) { self.scopes.push(HashMap::new()); }
    fn pop_scope(&mut self)  { self.scopes.pop(); }

    fn declare_named(&mut self, b: &mut FunctionBuilder, name: &str, ty: ir::Type) -> Variable {
        let v = b.declare_var(ty);
        self.scopes.last_mut().unwrap().insert(name.to_string(), v);
        v
    }

    fn lookup(&self, name: &str) -> Option<Variable> {
        for s in self.scopes.iter().rev() {
            if let Some(v) = s.get(name) { return Some(*v); }
        }
        None
    }

    // i64 -> b1（非零为真）
    fn as_bool(&mut self, bld: &mut FunctionBuilder, v_i64: ir::Value) -> ir::Value {
        let zero = bld.ins().iconst(types::I64, 0);
        bld.ins().icmp(ir::condcodes::IntCC::NotEqual, v_i64, zero)
    }

    // b1 -> i64(0/1)
    fn bool_to_i64(&mut self, bld: &mut FunctionBuilder, b1: ir::Value) -> ir::Value {
        let one = bld.ins().iconst(types::I64, 1);
        let zero = bld.ins().iconst(types::I64, 0);
        bld.ins().select(b1, one, zero)
    }

    fn emit_block(&mut self, b: &mut FunctionBuilder, blk: &Block) -> Result<ir::Value> {
        self.push_scope();
        for s in &blk.stmts {
            match s {
                Stmt::Let { name, ty, init, .. } => {
                    let v = self.emit_expr(b, init)?;
                    let store_v = match ty {
                        Ty::Int  => v,
                        Ty::Bool => {
                            let b1 = self.as_bool(b, v);
                            self.bool_to_i64(b, b1)
                        }
                    };
                    let var = self.declare_named(b, name, types::I64);
                    b.def_var(var, store_v);
                }
                Stmt::Expr(e) => {
                    let _ = self.emit_expr(b, e)?;
                }
                Stmt::Return(opt) => {
                    // 计算返回值（表达式层统一 i64）
                    let mut rv_i64 = if let Some(e) = opt {
                        self.emit_expr(b, e)?
                    } else {
                        b.ins().iconst(types::I64, 0)
                    };
                    // 根据当前函数签名决定真正返回类型
                    if matches!(self.ret_ty, Ty::Bool) {
                        rv_i64 = self.as_bool(b, rv_i64); // 转 b1
                    }
                    b.ins().return_(&[rv_i64]);
                    // 为了满足函数有返回值，这里给个占位（不会被用到）
                    self.pop_scope();
                    return Ok(b.ins().iconst(types::I64, 0));
                }
                Stmt::While { cond, body } => {
                    let hdr = b.create_block();
                    let bb  = b.create_block();
                    let out = b.create_block();

                    b.ins().jump(hdr, &[]);
                    b.switch_to_block(hdr);
                    b.seal_block(hdr);

                    let cv = self.emit_expr(b, cond)?;
                    let c1 = self.as_bool(b, cv);
                    b.ins().brif(c1, bb, &[], out, &[]);

                    b.switch_to_block(bb);
                    b.seal_block(bb);
                    let _ = self.emit_block(b, body)?;
                    b.ins().jump(hdr, &[]);

                    b.switch_to_block(out);
                    b.seal_block(out);
                }
            }
        }
        // 块尾表达式：无则视为 0（i64）
        let out = match &blk.tail {
            Some(e) => self.emit_expr(b, e)?,
            None    => b.ins().iconst(types::I64, 0),
        };
        self.pop_scope();
        Ok(out)
    }

    fn emit_expr(&mut self, b: &mut FunctionBuilder, e: &Expr) -> Result<ir::Value> {
        use BinOp::*;
        Ok(match e {
            Expr::Int(n)  => b.ins().iconst(types::I64, *n),
            Expr::Bool(x) => b.ins().iconst(types::I64, if *x { 1 } else { 0 }),
            Expr::Var(name) => {
                let v = self
                    .lookup(name)
                    .ok_or_else(|| anyhow::anyhow!("unknown var in codegen `{name}`"))?;
                b.use_var(v)
            }
            Expr::Unary { op, rhs } => {
                let r = self.emit_expr(b, rhs)?;
                match op {
                    UnOp::Neg => b.ins().ineg(r),
                    UnOp::Not => {
                        let c1  = self.as_bool(b, r);
                        let nb1 = b.ins().bnot(c1);
                        self.bool_to_i64(b, nb1)
                    }
                }
            }
            Expr::Binary { op, lhs, rhs } => {
                let l = self.emit_expr(b, lhs)?;
                let r = self.emit_expr(b, rhs)?;
                match op {
                    Add => b.ins().iadd(l, r),
                    Sub => b.ins().isub(l, r),
                    Mul => b.ins().imul(l, r),
                    Div => b.ins().sdiv(l, r),
                    Lt | Le | Gt | Ge | Eq | Ne => {
                        let cc = match op {
                            Lt => ir::condcodes::IntCC::SignedLessThan,
                            Le => ir::condcodes::IntCC::SignedLessThanOrEqual,
                            Gt => ir::condcodes::IntCC::SignedGreaterThan,
                            Ge => ir::condcodes::IntCC::SignedGreaterThanOrEqual,
                            Eq => ir::condcodes::IntCC::Equal,
                            Ne => ir::condcodes::IntCC::NotEqual,
                            _ => unreachable!(),
                        };
                        let b1 = b.ins().icmp(cc, l, r);
                        self.bool_to_i64(b, b1)
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
                // 条件：i64 -> b1
                let cv = self.emit_expr(b, cond)?;
                let c1 = self.as_bool(b, cv);

                // 基本块
                let then_bb = b.create_block();
                let else_bb = b.create_block();
                let merge   = b.create_block();

                // 用 Variable 抽象做 φ（由前端自动插入）
                let phi_tmp = b.declare_var(types::I64);

                b.ins().brif(c1, then_bb, &[], else_bb, &[]);

                // then
                b.switch_to_block(then_bb);
                b.seal_block(then_bb);
                let tv = self.emit_block(b, then_b)?;
                b.def_var(phi_tmp, tv);
                b.ins().jump(merge, &[]);

                // else
                b.switch_to_block(else_bb);
                b.seal_block(else_bb);
                let ev = self.emit_block(b, else_b)?;
                b.def_var(phi_tmp, ev);
                b.ins().jump(merge, &[]);

                // merge
                b.switch_to_block(merge);
                b.seal_block(merge);
                b.use_var(phi_tmp)
            }
            Expr::Call { callee, args } => {
                let fid = *self
                    .ids
                    .get(callee)
                    .ok_or_else(|| anyhow::anyhow!("unknown fn `{callee}`"))?;
                let fref = self.module.declare_func_in_func(fid, b.func);
                let mut av = Vec::with_capacity(args.len());
                for a in args {
                    av.push(self.emit_expr(b, a)?);
                }
                let call = b.ins().call(fref, &av);
                b.inst_results(call)[0] // 返回 I64（或按被调用者签名返回）
            }
            Expr::Block(blk) => self.emit_block(b, blk)?,
        })
    }
}
