// src/ty_intern.rs
use crate::interner::symbol::Sym;
use std::collections::HashMap;

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct TyId(u32);

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Type {
    Int, Long, Byte, Bool, String, Double, Float, Char, Void,
    Var(Sym),
    App { name: Sym, args: Box<[TyId]> },
}

pub struct TyInterner {
    types: Vec<Type>,
    map: HashMap<Type, TyId>,
}

impl Default for TyInterner {
    fn default() -> Self {
        Self { types: Vec::new(), map: HashMap::new() }
    }
}
impl TyInterner {
    pub fn new() -> Self { Self::default() }

    pub fn get(&self, id: TyId) -> &Type { &self.types[id.0 as usize] }

    fn intern_owned(&mut self, ty: Type) -> TyId {
        if let Some(&id) = self.map.get(&ty) { return id; }
        let id = TyId(self.types.len() as u32);
        self.types.push(ty.clone());
        self.map.insert(ty, id);
        id
    }

    // 原子类型（复用同一个实例）
    pub fn int(&mut self)    -> TyId { self.intern_owned(Type::Int) }
    pub fn long(&mut self)   -> TyId { self.intern_owned(Type::Long) }
    pub fn byte(&mut self) -> TyId { self.intern_owned(Type::Byte) }
    pub fn bool(&mut self)   -> TyId { self.intern_owned(Type::Bool) }
    pub fn string(&mut self) -> TyId { self.intern_owned(Type::String) }
    pub fn double(&mut self) -> TyId { self.intern_owned(Type::Double) }
    pub fn float(&mut self)  -> TyId { self.intern_owned(Type::Float) }
    pub fn char_(&mut self)  -> TyId { self.intern_owned(Type::Char) }
    pub fn void(&mut self)   -> TyId { self.intern_owned(Type::Void) }

    pub fn var(&mut self, v: Sym) -> TyId {
        self.intern_owned(Type::Var(v))
    }

    pub fn app(&mut self, name: Sym, args: &[TyId]) -> TyId {
        self.intern_owned(Type::App { name, args: args.into() })
    }
}

/// ===== 显示/调试辅助（可选） =====
impl Type {
    pub fn fmt_with<'a>(&'a self, syms: &'a crate::interner::symbol::SymbolInterner, tys: &'a TyInterner) -> impl std::fmt::Display + 'a {
        struct D<'a> { me: &'a Type, syms: &'a crate::interner::symbol::SymbolInterner, tys: &'a TyInterner }
        impl<'a> std::fmt::Display for D<'a> {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                match self.me {
                    Type::Int => write!(f, "Int"),
                    Type::Long => write!(f, "Long"),
                    Type::Byte => write!(f, "Byte"),
                    Type::Bool => write!(f, "Bool"),
                    Type::String => write!(f, "String"),
                    Type::Double => write!(f, "Double"),
                    Type::Float => write!(f, "Float"),
                    Type::Char => write!(f, "Char"),
                    Type::Void => write!(f, "Void"),
                    Type::Var(s) => write!(f, "{}", self.syms.resolve(*s)),
                    Type::App { name, args } => {
                        write!(f, "{}<", self.syms.resolve(*name))?;
                        for (i, a) in args.iter().enumerate() {
                            if i > 0 { write!(f, ", ")?; }
                            write!(f, "{}", self.tys.get(*a).fmt_with(self.syms, self.tys))?;
                        }
                        write!(f, ">")
                    }
                }
            }
        }
        D { me: self, syms, tys }
    }
}
