impl CLBackend {
    pub fn set_globals_from_program(&mut self, prog: &Program) {
        self.globals_val.clear();
        for it in &prog.items {
            if let Item::Global { name, ty, init, is_const, .. } = it {
                if !*is_const { continue; }
                match (ty, init) {
                    (Ty::Byte,   Expr::Int   { value: n, .. }) => { self.globals_val.insert(name.clone(), (Ty::Byte,  GConst::I8(*n as u8))); }
                    (Ty::Int,    Expr::Int   { value: n, .. }) => { self.globals_val.insert(name.clone(), (Ty::Int,   GConst::I32(*n))); }
                    (Ty::Long,   Expr::Long  { value: n, .. }) => { self.globals_val.insert(name.clone(), (Ty::Long,  GConst::I64(*n))); }
                    (Ty::Long,   Expr::Int   { value: n, .. }) => { self.globals_val.insert(name.clone(), (Ty::Long,  GConst::I64(*n as i64))); }
                    (Ty::Bool,   Expr::Bool  { value: b, .. }) => { self.globals_val.insert(name.clone(), (Ty::Bool,  GConst::I8(if *b {1} else {0}))); }
                    (Ty::Char,   Expr::Char  { value: u, .. }) => { self.globals_val.insert(name.clone(), (Ty::Char,  GConst::I32(*u as i32))); }
                    (Ty::Float,  Expr::Float { value: x, .. }) => { self.globals_val.insert(name.clone(), (Ty::Float, GConst::F32(*x))); }
                    (Ty::Double, Expr::Double{ value: x, .. }) => { self.globals_val.insert(name.clone(), (Ty::Double,GConst::F64(*x))); }
                    (Ty::String, Expr::Str   { .. })           => { /* 字符串常量单独驻留 */ }
                    _ => {}
                }
            }
        }
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