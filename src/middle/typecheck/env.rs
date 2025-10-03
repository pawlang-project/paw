/* ===================== 强类型键 ===================== */
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct TraitKey(pub String);

impl From<&str> for TraitKey {
    fn from(s: &str) -> Self { TraitKey(s.to_string()) }
}
impl From<String> for TraitKey {
    fn from(s: String) -> Self { TraitKey(s) }
}
impl fmt::Display for TraitKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result { f.write_str(&self.0) }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct ImplKey {
    pub trait_key: TraitKey,
    pub args: Box<[Ty]>,
}
impl ImplKey {
    #[inline]
    pub fn new(tr: &str, args: &[Ty]) -> Self {
        ImplKey { trait_key: TraitKey::from(tr), args: args.to_vec().into_boxed_slice() }
    }
}
impl fmt::Display for ImplKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.args.is_empty() {
            write!(f, "{}<>", self.trait_key)
        } else {
            write!(f, "{}<", self.trait_key)?;
            for (i, a) in self.args.iter().enumerate() {
                if i > 0 { write!(f, ", ")?; }
                write!(f, "{a}")?;
            }
            write!(f, ">")
        }
    }
}

/* ===================== trait / impl 环境 ===================== */

#[derive(Default, Clone)]
pub struct TraitEnv {
    pub decls: FastMap<TraitKey, TraitDecl>, // 强类型键 + 快表
}
impl TraitEnv {
    #[inline]
    pub fn arity(&self, name: &str) -> Option<usize> {
        self.decls.get(&TraitKey::from(name)).map(|d| d.type_params.len())
    }
    #[inline]
    pub fn get(&self, name: &str) -> Option<&TraitDecl> {
        self.decls.get(&TraitKey::from(name))
    }
}

#[derive(Default, Clone)]
pub struct ImplEnv {
    map: FastSet<ImplKey>, // 强类型键 + 快集合
}
impl ImplEnv {
    fn insert(&mut self, tr: &str, args: &[Ty]) -> Result<()> {
        let key = ImplKey::new(tr, args);
        if !self.map.insert(key.clone()) {
            bail!("duplicate impl `{}`", key);
        }
        Ok(())
    }
    #[inline]
    pub fn has_impl(&self, tr: &str, args: &[Ty]) -> bool {
        self.map.contains(&ImplKey::new(tr, args))
    }
}