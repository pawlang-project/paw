// src/passes.rs
use crate::backend::mangle::mangle_impl_method;
use crate::frontend::ast::*;
use crate::frontend::span::FileId;
use crate::frontend::parser;
use crate::diag::{SourceMap, DiagSink};
use anyhow::{anyhow, Context, Result};
use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};

/// ===============================
/// 1) 把 impl 方法降解为自由函数
///    例如：impl Eq<Int> { pub fn eq(x:Int,y:Int)->Bool {...} }
///    生成自由函数：__impl_Eq$Int__eq(x:Int,y:Int)->Bool
///    - 新函数 FunDecl.vis 继承 ImplMethod.vis
///    - span 继承方法定义处的 span
///    - 仅处理 ImplItem::Method；关联类型 ImplItem::AssocType 忽略
/// ===============================
pub fn declare_impls_from_program(mut p: Program) -> Program {
    // 收集已有自由函数名
    let mut existing: HashSet<String> = p
        .items
        .iter()
        .filter_map(|it| if let Item::Fun(f, _) = it { Some(f.name.clone()) } else { None })
        .collect();

    let mut extra: Vec<Item> = Vec::new();

    for it in &p.items {
        if let Item::Impl(id, _impl_span) = it {
            for item in &id.items {
                match item {
                    ImplItem::Method(m) => {
                        let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);
                        if existing.insert(sym.clone()) {
                            // 降解：用 impl 方法体/签名构造一个自由函数；span/vis 使用方法定义的属性
                            let f = FunDecl {
                                vis: m.vis,                  // 继承可见性
                                name: sym,
                                type_params: vec![],         // 这里认为已单态化/或库层不暴露
                                params: m.params.clone(),
                                ret: m.ret.clone(),
                                where_bounds: vec![],        // 降解后不带 where
                                body: m.body.clone(),
                                is_extern: false,
                                span: m.span,                // 继承方法 span
                            };
                            extra.push(Item::Fun(f, m.span));
                        }
                    }
                    ImplItem::ExternMethod(_) => {
                        // Extern methods are handled separately
                    }
                    ImplItem::AssocType(_a) => {
                        // 关联类型不会降解为函数：跳过
                    }
                }
            }
            // 这里保留原 impl 壳，仅额外补自由函数
        }
    }

    p.items.extend(extra);
    p
}

/// ===============================
/// 2) import 展开（仅支持 **双冒号** 模块路径）
///    - 入口 A：expand_imports_with_loader（单个搜索根，一般传工程根）
///    - 入口 B：expand_imports_with_roots（多个搜索根）
///    规则：`a::b::c` 解析为 `<root>/a/b/c.paw`
/// ===============================

/// 单一搜索根（通常传工程根目录）
pub fn expand_imports_with_loader(root: &Program, project_root: &Path, sm: &mut SourceMap, diags: &mut DiagSink) -> Result<Program> {
    expand_imports_with_roots(root, &[project_root.to_path_buf()], sm, diags)
}

/// 多搜索根（如果将来需要附加额外目录可使用此入口）
pub fn expand_imports_with_roots(root: &Program, roots: &[PathBuf], sm: &mut SourceMap, diags: &mut DiagSink) -> Result<Program> {
    let mut visited_files: HashSet<PathBuf> = HashSet::new();
    expand_prog_recursive(root, roots, &mut visited_files, sm, diags)
}

/// 递归展开 Program（去重防环）
fn expand_prog_recursive(
    prog: &Program,
    roots: &[PathBuf],
    visited: &mut HashSet<PathBuf>,
    sm: &mut SourceMap,
    diags: &mut DiagSink,
) -> Result<Program> {
    let mut out_items: Vec<Item> = Vec::new();

    for it in &prog.items {
        match it {
            Item::Import(spec, import_span) => {
                // 仅允许 "a::b::c" 形式
                let path = resolve_double_colon_import(spec, roots)
                    .with_context(|| format!("while importing `{}`", spec))?;

                let cano = canonicalize_lossy(&path)?;
                if !visited.insert(cano.clone()) {
                    // 已导入过，跳过（防环 & 去重）
                    continue;
                }

                let src = fs::read_to_string(&path)
                    .with_context(|| format!("read_to_string({})", path.display()))?;

                // 注册文件并解析子文件
                let fid = sm.add_file(path.display().to_string(), src.clone());
                let sub = parser::parse_program_with_diags(&src, fid, &path.display().to_string(), diags)
                    .with_context(|| format!("parse `{}` failed", path.display()))?;

                // 递归展开子 Program
                let sub_expanded = expand_prog_recursive(&sub, roots, visited, sm, diags)?;
                out_items.extend(sub_expanded.items);

                let _ = import_span; // 如需保留 import，可在此 push 回去
            }
            _ => out_items.push(it.clone()),
        }
    }

    Ok(Program { items: out_items })
}

/// 解析 `import "a::b::c";`：
/// - 严格禁止旧写法与任何路径分隔符（'.'、'/'、'\\'）
/// - 仅接受由 `::` 分隔的若干合法标识符段
/// - 映射为 a/b/c.paw，并按 roots 顺序查找
fn resolve_double_colon_import(spec: &str, roots: &[PathBuf]) -> Result<PathBuf> {
    // 拒绝旧写法与路径分隔符
    if spec.contains('.') || spec.contains('/') || spec.contains('\\') {
        return Err(anyhow!(
            "invalid import `{}`: only double-colon module paths are allowed, e.g. \"std::prelude\"",
            spec
        ));
    }
    if !is_double_colon_mod_path(spec) {
        return Err(anyhow!(
            "invalid module path `{}`: use double-colon identifiers like \"foo::bar\"",
            spec
        ));
    }

    // "a::b::c" -> "a/b/c.paw"
    let rel = PathBuf::from(spec.replace("::", "/")).with_extension("paw");

    for base in roots {
        let cand = base.join(&rel);
        if cand.is_file() {
            return Ok(cand);
        }
    }

    Err(anyhow!(
        "cannot resolve import `{}` (looked for `{}`) in roots: {}",
        spec,
        rel.display(),
        format_roots(roots),
    ))
}

/// 判断是否为合法的 a::b::c 模块路径（每段都是合法标识符）
fn is_double_colon_mod_path(s: &str) -> bool {
    if s.is_empty() {
        return false;
    }
    // 不允许以 "::" 开头/结尾，也不允许出现空段
    if s.starts_with("::") || s.ends_with("::") {
        return false;
    }
    s.split("::").all(|seg| {
        if seg.is_empty() {
            return false;
        }
        let mut it = seg.chars();
        match it.next() {
            Some(c0) if c0.is_ascii_alphabetic() || c0 == '_' => {
                it.all(|ch| ch.is_ascii_alphanumeric() || ch == '_')
            }
            _ => false,
        }
    })
}

/// 规范化 canonicalize（用于去重）
fn canonicalize_lossy(p: &Path) -> Result<PathBuf> {
    let c = std::fs::canonicalize(p)
        .with_context(|| format!("canonicalize({})", p.display()))?;
    Ok(c)
}

fn format_roots(roots: &[PathBuf]) -> String {
    let mut s = String::new();
    for (i, r) in roots.iter().enumerate() {
        if i > 0 {
            s.push_str(", ");
        }
        s.push_str(&r.display().to_string());
    }
    s
}
