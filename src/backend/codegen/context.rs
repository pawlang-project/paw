#[derive(Clone, Debug)]
pub enum GConst {
    I8(u8),
    I32(i32),
    I64(i64),
    F32(f32),
    F64(f64),
}

#[derive(Clone)]
struct ImplMethodTpl {
    trait_name: String,
    type_params: Vec<String>,   // 从 trait_args 中抽取的去重 type vars（出现顺序）
    trait_args_tpl: Vec<Ty>,    // impl 头部里写的 trait 实参模板（可能含 Ty::Var）
    method_name: String,
    params: Vec<(String, Ty)>,
    ret: Ty,
    body: Block,
}

pub struct CLBackend {
    pub module: ObjectModule,

    // name -> (Ty, const)
    pub globals_val: FastMap<String, (Ty, GConst)>,
    str_pool: FastMap<String, DataId>,

    // 符号名(已mangle) -> (参数类型列表, 返回类型)
    fn_sig: FastMap<String, (Vec<Ty>, Ty)>,

    // —— 单态化管理（自由函数） —— //
    templates: FastMap<String, FunDecl>,            // 基名 -> 模板
    mono_declared: FastSet<String>,                 // 已声明专门化符号(已mangle)
    mono_defined: FastSet<String>,                  // 已定义专门化符号(已mangle)
    mono_func_ids: FastMap<String, FuncId>,         // 专门化符号 -> FuncId
    mono_specs: FastMap<String, (String, Vec<Ty>)>, // 专门化符号 -> (基名, 类型实参)

    // —— 已声明函数符号（包含：extern 原名、impl 降解名、重载mangle名、单态化名） -> FuncId —— //
    base_func_ids: FastMap<String, FuncId>,

    // —— 重载索引：源代码里的“基名” -> 本模块所有重载符号名 —— //
    overloads: FastMap<String, Vec<String>>,
    declared_symbols: FastSet<String>,

    // —— impl 模板（泛型 impl 方法） —— //
    impl_templates: FastMap<(String, String), ImplMethodTpl>, // (trait_name, method) -> 模板

    // —— 可选：诊断收集器 —— //
    diag: Option<Rc<RefCell<DiagSink>>>,
    file_names: Vec<String>,
}

impl CLBackend {
    pub fn new() -> Result<Self> {
        let mut fbuilder = settings::builder();
        fbuilder.set("is_pic", "true")?;

        let flags = Flags::new(fbuilder);

        let isa = cranelift_native::builder()
            .map_err(|e| anyhow!("cranelift_native::builder(): {e}"))?
            .finish(flags)
            .map_err(|e| anyhow!("finish ISA: {e}"))?;

        let obj = ObjectBuilder::new(isa, "paw_obj".to_string(), default_libcall_names())?;
        Ok(Self {
            module: ObjectModule::new(obj),
            globals_val: FastMap::default(),
            str_pool: FastMap::default(),
            fn_sig: FastMap::default(),
            templates: FastMap::default(),
            mono_declared: FastSet::default(),
            mono_defined: FastSet::default(),
            mono_func_ids: FastMap::default(),
            mono_specs: FastMap::default(),
            base_func_ids: FastMap::default(),
            overloads: FastMap::default(),
            declared_symbols: FastSet::default(),
            impl_templates: FastMap::default(),
            diag: None,
            file_names: Vec::new(),
        })
    }

    fn diag_err(&self, code: &'static str, msg: impl AsRef<str>) {
        if let Some(d) = &self.diag {
            let file_id_str = self.file_names.get(0).cloned().unwrap_or_else(|| "<codegen>".to_string());
            d.borrow_mut().error(code, &file_id_str, None, msg.as_ref().to_string());
        }
    }

    fn diag_err_span(&self, code: &'static str, span: crate::frontend::span::Span, msg: impl AsRef<str>) {
        if let Some(d) = &self.diag {
            let file_id_str = self.file_names.get(span.file.0).cloned().unwrap_or_else(|| "<codegen>".to_string());
            d.borrow_mut().error(code, &file_id_str, Some(span), msg.as_ref().to_string());
        }
    }

    pub fn set_diag(&mut self, sink: Rc<RefCell<DiagSink>>) {
        self.diag = Some(sink);
    }

    pub fn set_file_names(&mut self, names: Vec<String>) { self.file_names = names; }
}