// src/symbol.rs
use std::collections::HashMap;

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct Sym(u32);

pub struct SymbolInterner {
    map: HashMap<String, Sym>,
    // 反向索引，用于调试/打印
    vec: Vec<String>,
}

impl Default for SymbolInterner {
    fn default() -> Self {
        Self { map: HashMap::new(), vec: Vec::new() }
    }
}

impl SymbolInterner {
    pub fn new() -> Self { Self::default() }

    /// 把 &str 实习成 Sym
    pub fn intern(&mut self, s: &str) -> Sym {
        if let Some(&sym) = self.map.get(s) { return sym; }
        let id = Sym(self.vec.len() as u32);
        self.vec.push(s.to_string());
        self.map.insert(self.vec[id.0 as usize].clone(), id);
        id
    }

    /// 从 Sym 取回 &str（仅借用，零拷贝）
    pub fn resolve(&self, sym: Sym) -> &str {
        &self.vec[sym.0 as usize]
    }
}
