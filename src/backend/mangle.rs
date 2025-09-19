// src/mangle.rs
use crate::frontend::ast::*;

/// 把类型转为搅拌后的字符串（和 typecheck/codegen 保持一致）
pub fn mangle_ty(t: &Ty) -> String {
    match t {
        Ty::Int    => "Int".into(),
        Ty::Long   => "Long".into(),
        Ty::Byte => "Byte".into(),
        Ty::Bool   => "Bool".into(),
        Ty::String => "String".into(),
        Ty::Double => "Double".into(),
        Ty::Float  => "Float".into(),
        Ty::Char   => "Char".into(),
        Ty::Void   => "Void".into(),
        Ty::Var(n) => format!("Var({})", n),
        Ty::App { name, args } => {
            if args.is_empty() {
                name.clone()
            } else {
                let parts: Vec<String> = args.iter().map(mangle_ty).collect();
                format!("{}<{}>", name, parts.join(","))
            }
        }
    }
}

/// 普通函数的专门化名： base$T1,T2,...
pub fn mangle_name(base: &str, args: &[Ty]) -> String {
    if args.is_empty() {
        base.to_string()
    } else {
        let parts: Vec<String> = args.iter().map(mangle_ty).collect();
        format!("{}${}", base, parts.join(","))
    }
}

/// impl 方法名：__impl_Trait$T1,T2__method
pub fn mangle_impl_method(trait_name: &str, trait_args: &[Ty], method: &str) -> String {
    if trait_args.is_empty() {
        format!("__impl_{}__{}", trait_name, method)
    } else {
        let parts: Vec<String> = trait_args.iter().map(mangle_ty).collect();
        format!("__impl_{}${}__{}", trait_name, parts.join(","), method)
    }
}
