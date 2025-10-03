/* ---------------------------
 * ------ Expr 生成器 --------
 * --------------------------- */
struct ExprGen<'a> {
    be: &'a mut CLBackend,
    scopes: Vec<FastMap<String, Variable>>,
    scopes_ty: Vec<FastMap<String, Ty>>,
    // (continue_target, break_target)
    loop_stack: Vec<(ir::Block, ir::Block)>,
    fun_ret: Ty,
}

impl<'a> ExprGen<'a> {
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
    fn val_ty(&self, b:&mut FunctionBuilder, v:ir::Value)->ir::Type { b.func.dfg.value_type(v) }

    fn coerce_ir_numeric(&mut self, b:&mut FunctionBuilder, v: ir::Value, want: ir::Type) -> Result<ir::Value> {
        let got = self.val_ty(b, v);
        if got == want { return Ok(v); }
        let both_int   = got.is_int()   && want.is_int();
        let both_float = got.is_float() && want.is_float();
        let int_to_f   = got.is_int()   && want.is_float();
        let f_to_int   = got.is_float() && want.is_int();

        if both_int || both_float || int_to_f || f_to_int {
            Ok(self.cast_value_to_irtype(b, v, want))
        } else {
            self.expect_irtype(b, v, want)?;
            Ok(v)
        }
    }

    fn expect_ty(&mut self, b:&mut FunctionBuilder, v: ir::Value, dst:&Ty) -> Result<()> {
        let want = cl_ty(dst).expect("no void here");
        self.expect_irtype(b, v, want)
    }

    fn expect_irtype(&mut self, b:&mut FunctionBuilder, v: ir::Value, want: ir::Type) -> Result<()> {
        let got = self.val_ty(b, v);
        if got != want {
            bail!("codegen internal: IR type mismatch (expect {:?}, got {:?})", want, got);
        }
        Ok(())
    }

    fn cast_to_ty(&mut self, b:&mut FunctionBuilder, v: ir::Value, dst:&Ty)->Result<ir::Value>{
        let want = cl_ty(dst).expect("no void here");
        Ok(self.cast_value_to_irtype(b, v, want))
    }

    fn cast_value_to_irtype(&mut self, b:&mut FunctionBuilder, v: ir::Value, dst: ir::Type) -> ir::Value {
        let src = self.val_ty(b, v);
        if src == dst { return v; }

        if src.is_int() && dst.is_int() {
            if src.bits() < dst.bits() {
                if src.bits() == 8 {
                    return b.ins().uextend(dst, v);
                } else {
                    return b.ins().sextend(dst, v);
                }
            } else {
                return b.ins().ireduce(dst, v);
            }
        }
        if src.is_float() && dst.is_float() {
            return if src.bits() < dst.bits() { b.ins().fpromote(dst, v) } else { b.ins().fdemote(dst, v) }
        }
        if src.is_int() && dst.is_float() {
            if src.bits() == 8 { return b.ins().fcvt_from_uint(dst, v); } else { return b.ins().fcvt_from_sint(dst, v); }
        } else if src.is_float() && dst.is_int() {
            return b.ins().fcvt_to_sint(dst, v);
        }
        v
    }

    fn require_same_numeric(
        &mut self,
        b:&mut FunctionBuilder,
        l: ir::Value,
        r: ir::Value,
    )->Result<(ir::Value, ir::Value, ir::Type)>{
        let lt = self.val_ty(b, l);
        let rt = self.val_ty(b, r);
        if lt != rt {
            bail!("codegen internal: arithmetic/compare expects same IR type, got {:?} vs {:?}", lt, rt);
        }
        if !(lt.is_int() || lt.is_float()) {
            bail!("codegen internal: arithmetic/compare expects numeric IR type, got {:?}", lt);
        }
        Ok((l, r, lt))
    }

    /* ---------- Byte 字面量 0..=255 的目标类型协助 ---------- */
    #[inline]
    fn fits_byte_literal(&self, e: &Expr) -> Option<u8> {
        match e {
            Expr::Int  { value: n, .. } if *n >= 0 && *n <= 255 => Some(*n as u8),
            Expr::Long { value: n, .. } if *n >= 0 && *n <= 255 => Some(*n as u8),
            _ => None,
        }
    }

    /// 只处理最常见：模板 1 个类型参数，且唯一形参是裸 `T`；返回 [T := arg_ty]
    fn infer_targs_for_simple_arity1(&mut self, base: &str, args: &[Expr]) -> Result<Vec<Ty>> {
        let tpl = self.be.templates.get(base)
            .ok_or_else(|| anyhow!("internal: no template for `{}`", base))?;

        if tpl.type_params.len() != 1 || tpl.params.len() != 1 {
            bail!("cannot infer type args for `{}`; add explicit `<...>`", base);
        }
        let tp0 = &tpl.type_params[0];
        match &tpl.params[0].1 {
            Ty::Var(v) if v == tp0 => {},
            _ => bail!("cannot infer `{}`: param is not plain type variable", base),
        }

        let at = self.static_ty_of_expr(&args[0])?;
        Ok(vec![at])
    }

    /// 从表达式快速给出静态类型；用于推断 println<T>(...)
    fn static_ty_of_expr(&mut self, e: &Expr) -> Result<Ty> {
        use BinOp::*; use UnOp::*;
        Ok(match e {
            Expr::Int   { .. } => Ty::Int,
            Expr::Long  { .. } => Ty::Long,
            Expr::Bool  { .. } => Ty::Bool,
            Expr::Char  { .. } => Ty::Char,
            Expr::Float { .. } => Ty::Float,
            Expr::Double{ .. } => Ty::Double,
            Expr::Str   { .. } => Ty::String,
            Expr::Var   { name, .. } => self.lookup_ty(name)
                .ok_or_else(|| anyhow!("cannot infer type of `{}` here; add explicit `<...>`", name))?,

            Expr::Unary { op, rhs, .. } => {
                let t = self.static_ty_of_expr(rhs)?;
                match op { Neg => t, Not => Ty::Bool }
            }

            Expr::Cast { expr: _, ty, .. } => ty.clone(),

            Expr::Binary { op, lhs, rhs, .. } => {
                let lt = self.static_ty_of_expr(lhs)?; let rt = self.static_ty_of_expr(rhs)?;
                match op {
                    Add | Sub | Mul | Div => if lt == rt { lt } else { bail!("cannot infer type from mixed arithmetic; add explicit cast") },
                    Lt | Le | Gt | Ge | Eq | Ne | And | Or => Ty::Bool,
                }
            }

            Expr::Block { block, .. } => self.static_ty_of_block(block)?,

            Expr::If { then_b, else_b, .. } => {
                let tt = self.static_ty_of_block(then_b)?; let et = self.static_ty_of_block(else_b)?;
                if tt == et { tt } else { bail!("cannot infer type from this expression; please write `println`<...>(...)") }
            }

            Expr::Match { arms, default, .. } => {
                let mut out: Option<Ty> = None;
                for (_, blk) in arms {
                    let t = self.static_ty_of_block(blk)?; if let Some(prev) = &out { if *prev != t { bail!("cannot infer type from this expression; please write `println`<...>(...)"); } } else { out = Some(t); }
                }
                if let Some(d) = default {
                    let t = self.static_ty_of_block(d)?; if let Some(prev) = &out { if *prev != t { bail!("cannot infer type from this expression; please write `println`<...>(...)"); } } else { out = Some(t); }
                }
                out.unwrap_or(Ty::Int)
            }

            // 调用：这里仅用在类型推断的辅助，不去解析重载
            Expr::Call { callee, generics, .. } => {
                match callee {
                    Callee::Name(name) => {
                        if !generics.is_empty() {
                            if let Some(tpl) = self.be.templates.get(name) {
                                let subst = build_subst_map(&tpl.type_params, generics);
                                subst_ty(&tpl.ret, &subst)
                            } else {
                                let mname = mangle_name(name, generics);
                                self.be.fn_sig.get(&mname)
                                    .map(|(_, ret)| ret.clone())
                                    .ok_or_else(|| anyhow!("cannot infer type from call to `{}`; add explicit `<...>`", name))?
                            }
                        } else {
                            bail!("cannot infer type from this call; please write explicit type args");
                        }
                    }
                    _ => bail!("cannot infer type from this expression; please write explicit type args"),
                }
            }
        })
    }

    fn emit_expr(&mut self, b:&mut FunctionBuilder, e:&Expr)->Result<ir::Value>{
        use BinOp::*; use UnOp::*;
        Ok(match e {
            Expr::Int    { value: n, .. } => b.ins().iconst(types::I32, *n as i64),
            Expr::Long   { value: n, .. } => b.ins().iconst(types::I64, *n),
            Expr::Float  { value: x, .. } => b.ins().f32const(*x),
            Expr::Double { value: x, .. } => b.ins().f64const(*x),
            Expr::Char   { value: u, .. } => b.ins().iconst(types::I32, *u as i64),
            Expr::Bool   { value: bi, .. } => b.ins().iconst(types::I8, if *bi {1} else {0}),
            Expr::Str    { value: s, .. } => {
                let did = self.be.intern_str(s)?; let gv  = self.be.module.declare_data_in_func(did, b.func);
                b.ins().global_value(types::I64, gv)
            }

            Expr::Var { name, .. } => {
                let v = self.lookup(name).ok_or_else(|| anyhow!("unknown var in codegen `{name}`"))?;
                b.use_var(v)
            }

            Expr::Unary{ op, rhs, .. } => {
                let r = self.emit_expr(b, rhs)?; let rt = self.val_ty(b, r);
                match op {
                    UnOp::Neg => if rt.is_int() { b.ins().ineg(r) } else { b.ins().fneg(r) },
                    UnOp::Not => { let b1 = self.bool_i8_to_b1(b, r); let nb = b.ins().bnot(b1); self.bool_b1_to_i8(b, nb) }
                }
            }

            // 显式 as 转换
            Expr::Cast { expr, ty, .. } => {
                let v = self.emit_expr(b, expr)?;
                self.cast_to_ty(b, v, ty)?
            }

            Expr::Binary{ op, lhs, rhs, .. }=>{
                match op {
                    BinOp::And => {
                        let l  = self.emit_expr(b, lhs)?; let l1 = self.bool_i8_to_b1(b, l);
                        let rhsb   = b.create_block(); let falseb = b.create_block(); let out = b.create_block();
                        let res = b.declare_var(types::I8);
                        b.ins().brif(l1, rhsb, &[], falseb, &[]);
                        b.switch_to_block(falseb); b.seal_block(falseb); let zero = b.ins().iconst(types::I8, 0); b.def_var(res, zero); b.ins().jump(out, &[]);
                        b.switch_to_block(rhsb);  b.seal_block(rhsb);  let r  = self.emit_expr(b, rhs)?; let r1 = self.bool_i8_to_b1(b, r); let ri = self.bool_b1_to_i8(b, r1); b.def_var(res, ri); b.ins().jump(out, &[]);
                        b.switch_to_block(out);   b.seal_block(out);   b.use_var(res)
                    }
                    BinOp::Or => {
                        let l  = self.emit_expr(b, lhs)?; let l1 = self.bool_i8_to_b1(b, l);
                        let trueb = b.create_block(); let rhsb  = b.create_block(); let out   = b.create_block();
                        let res = b.declare_var(types::I8);
                        b.ins().brif(l1, trueb, &[], rhsb, &[]);
                        b.switch_to_block(trueb); b.seal_block(trueb); let one = b.ins().iconst(types::I8, 1); b.def_var(res, one); b.ins().jump(out, &[]);
                        b.switch_to_block(rhsb);  b.seal_block(rhsb);  let r  = self.emit_expr(b, rhs)?; let r1 = self.bool_i8_to_b1(b, r); let ri = self.bool_b1_to_i8(b, r1); b.def_var(res, ri); b.ins().jump(out, &[]);
                        b.switch_to_block(out);   b.seal_block(out);   b.use_var(res)
                    }
                    _ => {
                        let l0 = self.emit_expr(b, lhs)?;
                        let r0 = self.emit_expr(b, rhs)?;
                        let (l, r, ty) = self.require_same_numeric(b, l0, r0)?;

                        match op {
                            BinOp::Add => if ty.is_int() { b.ins().iadd(l, r) } else { b.ins().fadd(l, r) },
                            BinOp::Sub => if ty.is_int() { b.ins().isub(l, r) } else { b.ins().fsub(l, r) },
                            BinOp::Mul => if ty.is_int() { b.ins().imul(l, r) } else { b.ins().fmul(l, r) },
                            BinOp::Div => if ty.is_int() { b.ins().sdiv(l, r) } else { b.ins().fdiv(l, r) },
                            BinOp::Lt | BinOp::Le | BinOp::Gt | BinOp::Ge => {
                                let b1 = if ty.is_int() {
                                    let cc = match op {
                                        BinOp::Lt => IntCC::SignedLessThan,
                                        BinOp::Le => IntCC::SignedLessThanOrEqual,
                                        BinOp::Gt => IntCC::SignedGreaterThan,
                                        BinOp::Ge => IntCC::SignedGreaterThanOrEqual,
                                        _  => unreachable!(),
                                    };
                                    b.ins().icmp(cc, l, r)
                                } else {
                                    let cc = match op {
                                        BinOp::Lt => FloatCC::LessThan,
                                        BinOp::Le => FloatCC::LessThanOrEqual,
                                        BinOp::Gt => FloatCC::GreaterThan,
                                        BinOp::Ge => FloatCC::GreaterThanOrEqual,
                                        _  => unreachable!(),
                                    };
                                    b.ins().fcmp(cc, l, r)
                                };
                                self.bool_b1_to_i8(b, b1)
                            }
                            BinOp::Eq | BinOp::Ne => {
                                let b1 = if ty.is_float() {
                                    let cc = if matches!(op, BinOp::Eq) { FloatCC::Equal } else { FloatCC::NotEqual };
                                    b.ins().fcmp(cc, l, r)
                                } else {
                                    let cc = if matches!(op, BinOp::Eq) { IntCC::Equal } else { IntCC::NotEqual };
                                    b.ins().icmp(cc, l, r)
                                };
                                self.bool_b1_to_i8(b, b1)
                            }
                            BinOp::And | BinOp::Or => unreachable!(),
                        }
                    }
                }
            }

            Expr::If { cond, then_b, else_b, .. } => {
                let cv = self.emit_expr(b, cond)?; let c1 = self.bool_i8_to_b1(b, cv);
                let tbb = b.create_block(); let ebb = b.create_block(); let mb  = b.create_block();
                b.ins().brif(c1, tbb, &[], ebb, &[]);
                b.switch_to_block(tbb); b.seal_block(tbb);
                let tv0 = self.emit_block(b, then_b)?; let tv_ty = self.val_ty(b, tv0); let phi = b.declare_var(tv_ty); b.def_var(phi, tv0); b.ins().jump(mb, &[]);
                b.switch_to_block(ebb); b.seal_block(ebb);
                let ev0 = self.emit_block(b, else_b)?;
                let ev  = self.coerce_ir_numeric(b, ev0, tv_ty)?;
                b.def_var(phi, ev); b.ins().jump(mb, &[]);
                b.switch_to_block(mb);  b.seal_block(mb); b.use_var(phi)
            }

            Expr::Match { scrut, arms, default, .. } => {
                let sv = self.emit_expr(b, scrut)?;
                let svt = self.val_ty(b, sv);
                let out = b.create_block(); let mut phi: Option<Variable> = None;
                let mut phi_ty: Option<ir::Type> = None;
                let mut next: Option<ir::Block> = None;
                for (pat, blk) in arms {
                    let this  = b.create_block(); let thenb = b.create_block(); let cont  = b.create_block();
                    if let Some(prev) = next.take() { b.switch_to_block(prev); b.seal_block(prev); b.ins().jump(this, &[]); }
                    else { b.ins().jump(this, &[]); }
                    b.switch_to_block(this); b.seal_block(this);
                    let hit_b1 = match pat {
                        Pattern::Int(n) => {
                            if svt != types::I32 { bail!("codegen internal: match scrutinee type mismatch (expect I32)"); }
                            let cn = b.ins().iconst(types::I32, *n as i64); b.ins().icmp(IntCC::Equal, sv, cn)
                        }
                        Pattern::Long(n) => {
                            if svt != types::I64 { bail!("codegen internal: match scrutinee type mismatch (expect I64)"); }
                            let cn = b.ins().iconst(types::I64, *n); b.ins().icmp(IntCC::Equal, sv, cn)
                        }
                        Pattern::Char(u) => {
                            if svt != types::I32 { bail!("codegen internal: match scrutinee type mismatch (expect I32)"); }
                            let cn = b.ins().iconst(types::I32, *u as i64); b.ins().icmp(IntCC::Equal, sv, cn)
                        }
                        Pattern::Bool(bt) => {
                            if svt != types::I8 { bail!("codegen internal: match scrutinee type mismatch (expect I8)"); }
                            let cn = b.ins().iconst(types::I8, if *bt {1} else {0}); b.ins().icmp(IntCC::Equal, sv, cn)
                        }
                        Pattern::Wild => { let one = b.ins().iconst(types::I8, 1); self.bool_i8_to_b1(b, one) }
                    };
                    b.ins().brif(hit_b1, thenb, &[], cont, &[]);
                    b.switch_to_block(thenb); b.seal_block(thenb);
                    let v = self.emit_block(b, blk)?;
                    if phi.is_none() {
                        let vt = self.val_ty(b, v);
                        phi = Some(b.declare_var(vt));
                        phi_ty = Some(vt);
                        b.def_var(phi.unwrap(), v);
                    } else {
                        let want = phi_ty.unwrap();
                        let vv = self.coerce_ir_numeric(b, v, want)?;
                        b.def_var(phi.unwrap(), vv);
                    }
                    b.ins().jump(out, &[]);
                    next = Some(cont);
                }
                if let Some(last) = next.take() {
                    b.switch_to_block(last); b.seal_block(last);
                    if let Some(db) = default {
                        let v = self.emit_block(b, db)?;
                        if phi.is_none() {
                            let vt = self.val_ty(b, v);
                            phi = Some(b.declare_var(vt));
                            phi_ty = Some(vt);
                            b.def_var(phi.unwrap(), v);
                        } else {
                            let want = phi_ty.unwrap();
                            let vv = self.coerce_ir_numeric(b, v, want)?;
                            b.def_var(phi.unwrap(), vv);
                        }
                        b.ins().jump(out, &[]);
                    } else {
                        if phi.is_none() {
                            phi = Some(b.declare_var(types::I32));
                            phi_ty = Some(types::I32);
                            let z = b.ins().iconst(types::I32, 0);
                            b.def_var(phi.unwrap(), z);
                        }
                        b.ins().jump(out, &[]);
                    }
                }
                b.switch_to_block(out); b.seal_block(out); b.use_var(phi.unwrap())
            }

            // 调用：支持 Name 与 Qualified；Name 情况下若命中重载集合，则按静态类型精确选择重载符号
            Expr::Call { callee, generics, args, .. } => {
                let (sym, fid) = match callee {
                    Callee::Name(name) => {
                        if !generics.is_empty() {
                            let mname = mangle_name(name, generics);
                            let fid  = match self.be.mono_func_ids.get(&mname) {
                                Some(fid) => *fid,
                                None => {
                                    let msg = format!("unknown monomorph function `{}`", mname);
                                    self.be.diag_err("CG0001", &msg);
                                    return Err(anyhow!(msg));
                                }
                            };
                            (mname, fid)
                        } else if self.be.templates.contains_key(name) {
                            // 隐式泛型：目前实现 arity=1 的常见形态（println<T>(x:T)）
                            let targs = self.infer_targs_for_simple_arity1(name, args)?;
                            let (mname, fid) = self.be.ensure_monomorph(name, &targs)?;
                            (mname, fid)
                        } else if let Some(over_syms) = self.be.overloads.get(name).cloned() {
                            // 2) 重载解析：用实参静态类型匹配唯一候选
                            let mut arg_tys: Vec<Ty> = Vec::with_capacity(args.len());
                            for i in 0..args.len() { arg_tys.push(self.static_ty_of_expr(&args[i])?); }

                            let mut hits: Vec<String> = Vec::new();
                            for s in over_syms {
                                if let Some((ptys, _ret)) = self.be.fn_sig.get(&s) {
                                    if ptys.len() != arg_tys.len() { continue; }
                                    let mut ok = true;
                                    for (i, (want, got)) in ptys.iter().zip(arg_tys.iter()).enumerate() {
                                        if want == got { continue; }
                                        if *want == Ty::Byte {
                                            if self.fits_byte_literal(&args[i]).is_some() { continue; }
                                        }
                                        ok = false; break;
                                    }
                                    if ok { hits.push(s.clone()); }
                                }
                            }
                            match hits.len() {
                                0 => {
                                    let msg = format!("no overload of `{}` matches given argument types", name);
                                    self.be.diag_err("CG0005", &msg);
                                    return Err(anyhow!(msg));
                                }
                                1 => {
                                    let s = hits.remove(0);
                                    let fid = *self.be.base_func_ids.get(&s).ok_or_else(|| anyhow!("missing FuncId for `{}`", s))?;
                                    (s, fid)
                                }
                                _ => {
                                    let msg = format!("ambiguous call to `{}`: {} candidates match", name, hits.len());
                                    self.be.diag_err("CG0005", &msg);
                                    return Err(anyhow!(msg));
                                }
                            }
                        } else if let Some(&fid) = self.be.base_func_ids.get(name) {
                            // 3) 普通非重载（比如 extern 打印函数）
                            (name.clone(), fid)
                        } else {
                            let msg = format!("unknown function in codegen `{}`", name);
                            self.be.diag_err("CG0001", &msg);
                            return Err(anyhow!(msg));
                        }
                    }
                    Callee::Qualified { trait_name, method } => {
                        if generics.is_empty() {
                            let msg = format!(
                                "qualified call `{}::{}` needs explicit type args, e.g. `{}::{}<Int>(...)`",
                                trait_name, method, trait_name, method
                            );
                            self.be.diag_err("CG0001", &msg);
                            return Err(anyhow!(msg));
                        }
                        // 按需单态化泛型 impl 方法
                        let (sym, _fid) = self.be.ensure_impl_monomorph(trait_name, generics, method)?;
                        let fid = match self.be.base_func_ids.get(&sym) {
                            Some(fid) => *fid,
                            None => {
                                let msg = format!(
                                    "unknown impl method symbol `{}` after monomorph; did you call declare_impls_from_program?",
                                    sym
                                );
                                self.be.diag_err("CG0003", &msg);
                                return Err(anyhow!(msg));
                            }
                        };
                        (sym, fid)
                    }
                };

                let (param_tys, ret_ty) = match self.be.fn_sig.get(&sym) {
                    Some(sig) => sig.clone(),
                    None => {
                        let msg = format!("missing signature for `{sym}`");
                        self.be.diag_err("CG0001", &msg);
                        return Err(anyhow!(msg));
                    }
                };

                if param_tys.len() != args.len() {
                    let msg = format!("function `{}` expects {} args, got {}", sym, param_tys.len(), args.len());
                    self.be.diag_err("CG0001", &msg);
                    return Err(anyhow!(msg));
                }

                let mut av = Vec::with_capacity(args.len());
                for (i, pt) in param_tys.iter().enumerate() {
                    let v0 = self.emit_expr(b, &args[i])?;
                    let vi = self.coerce_for_dst_from_expr(b, &args[i], v0, pt)?;
                    av.push(vi);
                }

                let fref = self.be.module.declare_func_in_func(fid, b.func);
                let call = b.ins().call(fref, &av);

                if matches!(ret_ty, Ty::Void) {
                    b.ins().iconst(types::I32, 0)
                } else {
                    b.inst_results(call)[0]
                }
            }

            Expr::Block { block: blk, .. } => self.emit_block(b, blk)?,
        })
    }
}