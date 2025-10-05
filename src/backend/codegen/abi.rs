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
        Ty::App { .. } => Some(types::I64), // MVP：所有应用类型按指针（I64）传递/返回
        Ty::Void   => None,
        Ty::Var(_) => None,
    }
}