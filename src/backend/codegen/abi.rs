fn cl_ty(t: &Ty) -> Option<ir::Type> {
    match t {
        Ty::Byte   => Some(types::I8),
        Ty::Int    => Some(types::I32),
        Ty::Long   => Some(types::I64),
        Ty::Bool   => Some(types::I8),
        Ty::Char   => Some(types::I32),
        Ty::Float  => Some(types::F32),
        Ty::Double => Some(types::F64),
        Ty::String => Some(types::I64),
        Ty::App { name, args } if (name == "Box" || name == "Rc" || name == "Arc") && args.len() == 1 => {
            Some(types::I64)
        }
        Ty::Void   => None,
        Ty::Var(_) | Ty::App { .. } => None,
    }
}