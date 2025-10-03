impl<'a> ExprGen<'a> {
    fn push(&mut self){ self.scopes.push(FastMap::default()); self.scopes_ty.push(FastMap::default()); }
    fn pop(&mut self){ self.scopes.pop(); self.scopes_ty.pop(); }

    fn coerce_for_dst_from_expr(
        &mut self,
        b:&mut FunctionBuilder,
        src_expr: &Expr,
        v_raw: ir::Value,
        dst:&Ty
    ) -> Result<ir::Value> {
        // Byte 字面量的特例（保持原有语义）
        if matches!(dst, Ty::Byte) {
            if let Some(u) = self.fits_byte_literal(src_expr) {
                return Ok(b.ins().iconst(types::I8, u as i64));
            }
        }
        let want = cl_ty(dst).expect("no void here");
        self.coerce_ir_numeric(b, v_raw, want)
    }

    fn emit_block(&mut self, b:&mut FunctionBuilder, blk:&Block)->Result<ir::Value>{
        self.push();
        for s in &blk.stmts {
            match s {
                Stmt::Let { name, ty, init, .. } => {
                    let v0 = self.emit_expr(b, init)?;
                    let v  = match ty {
                        Ty::Bool => { let b1 = self.bool_i8_to_b1(b, v0); self.bool_b1_to_i8(b, b1) }
                        Ty::Byte => { self.coerce_for_dst_from_expr(b, init, v0, ty)? }
                        _ => { self.coerce_for_dst_from_expr(b, init, v0, ty)? }
                    };
                    let var = self.declare_named(b, name, ty);
                    b.def_var(var, v);
                }
                Stmt::Assign { name, expr, .. } => {
                    let var = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                    let rhs = self.emit_expr(b, expr)?;
                    let rhs_fixed = self.coerce_for_dst_from_expr(b, expr, rhs, &ty)?;
                    b.def_var(var, rhs_fixed);
                }
                Stmt::Expr { expr, .. } => { let _ = self.emit_expr(b, expr)?; }
                Stmt::If { cond, then_b, else_b, .. } => {
                    let cv  = self.emit_expr(b, cond)?; let c1  = self.bool_i8_to_b1(b, cv);
                    let tbb = b.create_block(); let ebb = b.create_block(); let out = b.create_block();
                    b.ins().brif(c1, tbb, &[], ebb, &[]);
                    b.switch_to_block(tbb); b.seal_block(tbb); let _ = self.emit_block(b, then_b)?; b.ins().jump(out, &[]);
                    b.switch_to_block(ebb); b.seal_block(ebb); if let Some(eb) = else_b { let _ = self.emit_block(b, eb)?; } b.ins().jump(out, &[]);
                    b.switch_to_block(out); b.seal_block(out);
                }
                Stmt::While { cond, body, .. } => {
                    let hdr = b.create_block(); let bb  = b.create_block(); let out = b.create_block();
                    b.ins().jump(hdr, &[]); b.switch_to_block(hdr);
                    let cv = self.emit_expr(b, cond)?; let c  = self.bool_i8_to_b1(b, cv);
                    b.ins().brif(c, bb, &[], out, &[]);
                    b.switch_to_block(bb); b.seal_block(bb);
                    self.loop_stack.push((hdr, out)); let _ = self.emit_block(b, body)?; self.loop_stack.pop();
                    b.ins().jump(hdr, &[]); b.seal_block(hdr);
                    b.switch_to_block(out); b.seal_block(out);
                }
                Stmt::For { init, cond, step, body, .. } => {
                    if let Some(fi) = init {
                        match fi {
                            ForInit::Let { name, ty, init, .. } => {
                                let rhs = self.emit_expr(b, init)?;
                                let v = match ty {
                                    Ty::Bool => { let b1 = self.bool_i8_to_b1(b, rhs); self.bool_b1_to_i8(b, b1) }
                                    Ty::Byte => { self.coerce_for_dst_from_expr(b, init, rhs, ty)? }
                                    _ => { self.coerce_for_dst_from_expr(b, init, rhs, ty)? }
                                };
                                let var = self.declare_named(b, name, ty); b.def_var(var, v);
                            }
                            ForInit::Assign { name, expr, .. } => {
                                let (var, ty) = {
                                    let v  = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                                    (v, ty)
                                };
                                let rhs = self.emit_expr(b, expr)?; let rv = self.coerce_for_dst_from_expr(b, expr, rhs, &ty)?;
                                b.def_var(var, rv);
                            }
                            ForInit::Expr(e, ..) => { let _ = self.emit_expr(b, e)?; }
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
                            ForStep::Assign { name, expr, .. } => {
                                let (var, ty) = {
                                    let v  = self.lookup(name).ok_or_else(|| anyhow!("unknown var `{name}`"))?;
                                    let ty = self.lookup_ty(name).ok_or_else(|| anyhow!("no type for `{name}`"))?;
                                    (v, ty)
                                };
                                let rhs = self.emit_expr(b, expr)?; let rv = self.coerce_for_dst_from_expr(b, expr, rhs, &ty)?;
                                b.def_var(var, rv);
                            }
                            ForStep::Expr(e, ..) => { let _ = self.emit_expr(b, e)?; }
                        }
                    }

                    b.ins().jump(hdr, &[]); b.seal_block(hdr);
                    b.switch_to_block(out); b.seal_block(out);
                }
                Stmt::Break { .. } => {
                    let &(_, out) = self.loop_stack.last().ok_or_else(|| anyhow!("`break` outside loop"))?;
                    b.ins().jump(out, &[]); let cont = b.create_block(); b.switch_to_block(cont); b.seal_block(cont);
                }
                Stmt::Continue { .. } => {
                    let &(cont_tgt, _) = self.loop_stack.last().ok_or_else(|| anyhow!("`continue` outside loop"))?;
                    b.ins().jump(cont_tgt, &[]); let cont = b.create_block(); b.switch_to_block(cont); b.seal_block(cont);
                }
                Stmt::Return { expr: opt, .. } => {
                    match (self.fun_ret.clone(), opt) {
                        (Ty::Void, Some(e)) => { let _ = self.emit_expr(b, e)?; b.ins().return_(&[]); }
                        (ret_ty, Some(e)) => {
                            let raw = self.emit_expr(b, e)?;
                            let rv = self.coerce_for_dst_from_expr(b, e, raw, &ret_ty)?;
                            b.ins().return_(&[rv]);
                        }
                        (ret_ty, None) => {
                            match ret_ty {
                                Ty::Bool | Ty::Byte => { let z = b.ins().iconst(types::I8, 0);  b.ins().return_(&[z]); }
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
                    let cont = b.create_block(); b.switch_to_block(cont); b.seal_block(cont);
                }
            }
        }
        let out = match &blk.tail { Some(e) => self.emit_expr(b, e)?, None => b.ins().iconst(types::I32, 0) };
        self.pop();
        Ok(out)
    }

    #[inline]
    fn static_ty_of_block(&mut self, b: &Block) -> anyhow::Result<Ty> {
        if let Some(t) = &b.tail { self.static_ty_of_expr(t) } else { Ok(Ty::Int) }
    }
}