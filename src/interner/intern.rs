use crate::frontend::ast;
use crate::interner::symbol::{Sym, SymbolInterner};
use crate::interner::ty_intern::{TyId, TyInterner};
// 你现有的 AST（仍然是 String/Ty 枚举）

pub struct InternCtx {
    pub syms: SymbolInterner,
    pub tys: TyInterner,
}

impl InternCtx {
    pub fn new() -> Self { Self { syms: SymbolInterner::new(), tys: TyInterner::new() } }

    pub fn sym(&mut self, s: &str) -> Sym { self.syms.intern(s) }

    /// 把 AST 的 Ty（带 String）转换为 intern 后的 TyId
    pub fn ast_ty_to_id(&mut self, t: &ast::Ty) -> TyId {
        use ast::Ty::*;
        match t {
            Int    => self.tys.int(),
            Long   => self.tys.long(),
            Byte => self.tys.byte(),
            Bool   => self.tys.bool(),
            String => self.tys.string(),
            Double => self.tys.double(),
            Float  => self.tys.float(),
            Char   => self.tys.char_(),
            Void   => self.tys.void(),
            Var(v) => {
                let s = self.sym(v);
                self.tys.var(s)
            }
            App { name, args } => {
                let name_sym = self.sym(name);
                let arg_ids: Vec<TyId> = args.iter().map(|a| self.ast_ty_to_id(a)).collect();
                self.tys.app(name_sym, &arg_ids)
            }
        }
    }
}
