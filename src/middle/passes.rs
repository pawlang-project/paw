// src/passes.rs
use crate::backend::mangle::mangle_impl_method;
use crate::frontend::ast::*;
use crate::frontend::span::FileId;
use crate::parser;
use anyhow::{anyhow, Context, Result};
use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};

/// ===============================
/// 1) 把 impl 方法降解为自由函数
///    例如：impl Eq<Int> { fn eq(x:Int,y:Int)->Bool {...} }
///    生成一个自由函数：__impl_Eq$Int__eq(x:Int,y:Int)->Bool
///    注意：为新生成的 FunDecl / Item 赋上合适的 span（沿用 impl 方法的 span）
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
        if let Item::Impl(id, impl_span) = it {
            for m in &id.items {
                let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);
                if existing.insert(sym.clone()) {
                    // 降解：用 impl 方法体/签名构造一个自由函数；span 使用方法定义的 span
                    let f = FunDecl {
                        name: sym,
                        type_params: vec![],
                        params: m.params.clone(),
                        ret: m.ret.clone(),
                        where_bounds: vec![],
                        body: m.body.clone(),
                        is_extern: false,
                        span: m.span, // 方法定义处的 span
                    };
                    extra.push(Item::Fun(f, m.span));
                }
            }

            // 如果你希望把 impl 自身也转为一个“标记性” item，可在此处额外推入；
            // 但通常 passes 只是“降解方法”，不移除 impl 壳；这里保持原状：既保留原 impl，又额外补自由函数。
            let _ = impl_span; // 仅为强调：此 span 若将来要改策略可用上
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
pub fn expand_imports_with_loader(root: &Program, project_root: &Path) -> Result<Program> {
    expand_imports_with_roots(root, &[project_root.to_path_buf()])
}

/// 多搜索根（如果将来需要附加额外目录可使用此入口）
pub fn expand_imports_with_roots(root: &Program, roots: &[PathBuf]) -> Result<Program> {
    let mut visited_files: HashSet<PathBuf> = HashSet::new();
    expand_prog_recursive(root, roots, &mut visited_files)
}

/// 递归展开 Program（去重防环）
fn expand_prog_recursive(
    prog: &Program,
    roots: &[PathBuf],
    visited: &mut HashSet<PathBuf>,
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

                // 解析子文件。若你维护了 FileId 映射，请换成真实 FileId；这里先用 DUMMY。
                let sub = parser::parse_program(&src, FileId::DUMMY)
                    .with_context(|| format!("parse `{}` failed", path.display()))?;

                // 递归展开子 Program
                let sub_expanded = expand_prog_recursive(&sub, roots, visited)?;
                out_items.extend(sub_expanded.items);

                let _ = import_span; // 如需把 import 保留在输出里，可在此处 push(Item::Import(spec.clone(), *import_span))
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
