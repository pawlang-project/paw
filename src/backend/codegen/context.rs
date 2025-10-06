use crate::frontend::ast::*;
use crate::utils::fast::*;
use cranelift_codegen::isa::TargetIsa;
use target_lexicon::triple;

#[derive(Clone, Debug)]
pub struct StructLayout {
    pub size: u32,
    pub align: u32,
    pub fields: Vec<(String, u32 /*offset*/, Ty)>,
    pub field_offsets: FastMap<String, u32>,
}

#[derive(Clone, Debug)]
pub struct StructTpl {
    pub type_params: Vec<String>,
    pub fields: Vec<(String, Ty)>, // 字段类型可能含 Ty::Var
}

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

    // —— 结构体模板与布局缓存 —— //
    struct_templates: FastMap<String, StructTpl>,
    // key = Name<args...>
    struct_layouts: FastMap<String, StructLayout>,
}

impl CLBackend {
    pub fn new() -> Result<Self> {
        Self::new_for_target(None)
    }

    pub fn new_for_target(target: Option<&str>) -> Result<Self> {
        let mut fbuilder = settings::builder();
        fbuilder.set("is_pic", "true")?;

        let flags = Flags::new(fbuilder);

        let isa = if let Some(target) = target {
            create_isa_for_target(target)?
        } else {
            cranelift_native::builder()
                .map_err(|e| anyhow!("cranelift_native::builder(): {e}"))?
                .finish(flags)
                .map_err(|e| anyhow!("finish ISA: {e}"))?
        };

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
            struct_templates: FastMap::default(),
            struct_layouts: FastMap::default(),
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

    pub fn layout_for_app_ty(&mut self, name: &str, args: &[Ty]) -> Result<StructLayout> {
        let key = Self::key_of_app(name, args);
        if let Some(l) = self.struct_layouts.get(&key) { return Ok(l.clone()); }

        let tpl = self.struct_templates.get(name)
            .ok_or_else(|| anyhow!("unknown struct `{}` (no template)", name))?.clone();
        if tpl.type_params.len() != args.len() {
            return Err(anyhow!("struct `{}` expects {} type args, got {}", name, tpl.type_params.len(), args.len()));
        }
        // 替换形参为实参
        let subst = build_subst_map(&tpl.type_params, args);
        let mut lay = StructLayout { 
            size: 0, 
            align: 1, 
            fields: Vec::new(),
            field_offsets: FastMap::default() 
        };

        for (fname, fty_tpl) in &tpl.fields {
            let fty = subst_ty(fty_tpl, &subst);
            let (fsz, fal) = match &fty {
                Ty::Byte | Ty::Bool => (1,1),
                Ty::Int | Ty::Char  => (4,4),
                Ty::Long | Ty::String => (8,8),
                Ty::Float => (4,4), Ty::Double => (8,8),
                Ty::App { .. } => (8,8), // byref
                Ty::Void => (0,1),
                Ty::Var(_) => (8,8),
            };
            if fal > 0 && (lay.size % fal) != 0 { lay.size = ((lay.size + fal - 1) / fal) * fal; }
            let off = lay.size;
            lay.fields.push((fname.clone(), off, fty.clone()));
            lay.field_offsets.insert(fname.clone(), off);
            if fal > lay.align { lay.align = fal; }
            lay.size += fsz;
        }
        if lay.align > 0 && (lay.size % lay.align) != 0 { lay.size = ((lay.size + lay.align - 1) / lay.align) * lay.align; }

        self.struct_layouts.insert(key.clone(), lay.clone());
        Ok(lay)
    }

    fn key_of_app(name: &str, args: &[Ty]) -> String {
        if args.is_empty() { name.to_string() } else { format!("{}<{}>", name, args.iter().map(|t| crate::backend::mangle::mangle_ty(t)).collect::<Vec<_>>().join(",")) }
    }
}

fn create_isa_for_target(target: &str) -> Result<std::sync::Arc<dyn TargetIsa>> {
    use cranelift_codegen::isa::lookup;
    use cranelift_codegen::settings::Configurable;
    
    let mut fbuilder = settings::builder();
    fbuilder.set("is_pic", "true")?;
    
    // 根据目标平台设置特定的编译选项
    match target {
        "x86_64-pc-windows-gnu" | "x86_64-windows-gnu" => {
            fbuilder.set("use_colocated_libcalls", "false")?;
        }
        "x86_64-unknown-linux-gnu" | "x86_64-linux-gnu" => {
            fbuilder.set("use_colocated_libcalls", "false")?;
        }
        "x86_64-apple-darwin" | "x86_64-macos" => {
            fbuilder.set("use_colocated_libcalls", "false")?;
        }
        "aarch64-apple-darwin" | "aarch64-macos" => {
            fbuilder.set("use_colocated_libcalls", "false")?;
        }
        _ => {}
    }
    
    let flags = Flags::new(fbuilder);
    
    // 根据目标平台创建ISA
    match target {
        "x86_64-pc-windows-gnu" | "x86_64-windows-gnu" => {
            // Windows x64 - 使用COFF格式
            let isa = lookup(triple!("x86_64-pc-windows-gnu"))
                .map_err(|e| anyhow!("lookup x86_64-pc-windows-gnu: {e}"))?
                .finish(flags)
                .map_err(|e| anyhow!("finish ISA: {e}"))?;
            Ok(isa)
        }
        "x86_64-unknown-linux-gnu" | "x86_64-linux-gnu" => {
            // Linux x64 - 使用ELF格式
            let isa = lookup(triple!("x86_64-unknown-linux-gnu"))
                .map_err(|e| anyhow!("lookup x86_64-unknown-linux-gnu: {e}"))?
                .finish(flags)
                .map_err(|e| anyhow!("finish ISA: {e}"))?;
            Ok(isa)
        }
        "x86_64-apple-darwin" | "x86_64-macos" => {
            // macOS x64 - 使用Mach-O格式
            let isa = lookup(triple!("x86_64-apple-darwin"))
                .map_err(|e| anyhow!("lookup x86_64-apple-darwin: {e}"))?
                .finish(flags)
                .map_err(|e| anyhow!("finish ISA: {e}"))?;
            Ok(isa)
        }
        _ => {
            // 默认使用原生ISA
            let isa = cranelift_native::builder()
                .map_err(|e| anyhow!("cranelift_native::builder(): {e}"))?
                .finish(flags)
                .map_err(|e| anyhow!("finish ISA: {e}"))?;
            Ok(isa)
        }
    }
}
