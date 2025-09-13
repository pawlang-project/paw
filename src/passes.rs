use crate::ast::*;
use crate::mangle::mangle_impl_method;
use crate::parser;
use anyhow::{anyhow, Context, Result};
use std::collections::HashSet;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

/// 递归展开 Import，并以 **entry_dir** 作为首要基准目录。
/// 建议从 main 里调用 `expand_imports_with_base(root, entry_dir)`.
pub fn expand_imports(root: &Program) -> Result<Program> {
    // 兼容旧接口：用当前工作目录作为基准（可能找不到 std/）
    let cwd = std::env::current_dir().context("get current_dir failed")?;
    expand_with_base(root, &cwd)
}

/// 推荐：传入“入口文件”的目录作为基准
pub fn expand_imports_with_base(root: &Program, entry_dir: &Path) -> Result<Program> {
    expand_with_base(root, entry_dir)
}

fn expand_with_base(root: &Program, base_dir: &Path) -> Result<Program> {
    let mut out = Vec::<Item>::new();
    let mut seen = HashSet::<String>::new();

    for it in &root.items {
        match it {
            Item::Import(path_str) => {
                let path = resolve_import(base_dir, path_str)
                    .with_context(|| format!("while importing `{}`", path_str))?;
                expand_file(&path, &mut out, &mut seen)
                    .with_context(|| format!("while importing `{}`", path_str))?;
            }
            other => out.push(other.clone()),
        }
    }

    Ok(Program { items: out })
}

/// 实际展开某个已解析到的文件
fn expand_file(path: &Path, out: &mut Vec<Item>, seen: &mut HashSet<String>) -> Result<()> {
    let canon = fs::canonicalize(path)
        .with_context(|| format!("canonicalize({})", path.display()))?;
    let key = canon.to_string_lossy().to_string();
    if !seen.insert(key) {
        // 已展开过：防重复/防环
        return Ok(());
    }

    let src = fs::read_to_string(&canon)
        .with_context(|| format!("read_to_string({})", canon.display()))?;
    let prog = parser::parse_program(&src)
        .with_context(|| format!("parse `{}` failed", canon.display()))?;

    let this_base = canon.parent().unwrap_or(Path::new(".")).to_path_buf();

    for it in &prog.items {
        match it {
            Item::Import(p) => {
                let sub = resolve_import(&this_base, p)
                    .with_context(|| format!("while importing `{}`", p))?;
                expand_file(&sub, out, seen)
                    .with_context(|| format!("while importing `{}`", p))?;
            }
            other => out.push(other.clone()),
        }
    }
    Ok(())
}

/// 解析 import 路径：
/// - 若为绝对路径，则直接检查存在性；
/// - 若为相对路径，则在“搜索根”中依次尝试：
///   1) 发起 import 的文件所在目录（base_dir）
///   2) 当前工作目录
///   3) 环境变量 PAW_IMPORT_PATH 中的若干目录（; 分隔，Unix 可用 :）
///   4) 环境变量 PAW_STDLIB 指向的目录
///   5) 编译时的工程根（CARGO_MANIFEST_DIR）及其子目录 `paw/`
fn resolve_import(base_dir: &Path, p: &str) -> Result<PathBuf> {
    let want = Path::new(p);

    // 绝对路径
    if want.is_absolute() {
        if want.exists() {
            return Ok(want.to_path_buf());
        }
        return Err(anyhow!("absolute import not found: {}", want.display()));
    }

    // 构造搜索根
    let mut roots: Vec<PathBuf> = Vec::new();
    roots.push(base_dir.to_path_buf()); // 入口/当前文件目录
    if let Ok(cwd) = env::current_dir() {
        roots.push(cwd);
    }

    // PAW_IMPORT_PATH：;（Windows）或 :（Unix）分隔
    if let Ok(s) = env::var("PAW_IMPORT_PATH") {
        let sep = if cfg!(windows) { ';' } else { ':' };
        for part in s.split(sep) {
            let part = part.trim();
            if !part.is_empty() {
                roots.push(PathBuf::from(part));
            }
        }
    }

    // PAW_STDLIB：通常指向 std 目录所在的根
    if let Ok(stdlib) = env::var("PAW_STDLIB") {
        roots.push(PathBuf::from(stdlib));
    }

    // 工程根（编译时），常见布局：<repo>/paw、<repo>/std
    if let Some(manifest) = option_env!("CARGO_MANIFEST_DIR") {
        let m = PathBuf::from(manifest);
        roots.push(m.join("paw"));
        roots.push(m.join("std"));
        roots.push(m);
    }

    // 依次尝试
    let mut tried = Vec::<PathBuf>::new();
    for r in roots {
        let cand = r.join(want);
        tried.push(cand.clone());
        if cand.exists() {
            return Ok(cand);
        }
    }

    // 打印尝试过的路径，方便定位
    let mut msg = String::new();
    msg.push_str(&format!("cannot resolve import `{p}`; tried:\n"));
    for t in tried {
        msg.push_str(&format!("  - {}\n", t.display()));
    }
    Err(anyhow!(msg))
}

/* ========= （可选）把 impl 方法降解为自由函数 ========= */

pub fn declare_impls_from_program(mut p: Program) -> Program {
    let mut existing: HashSet<String> = p.items.iter().filter_map(|it| {
        if let Item::Fun(f) = it { Some(f.name.clone()) } else { None }
    }).collect();

    let mut extra: Vec<Item> = Vec::new();

    for it in &p.items {
        if let Item::Impl(id) = it {
            for m in &id.items {
                let sym = mangle_impl_method(&id.trait_name, &id.trait_args, &m.name);
                if existing.insert(sym.clone()) {
                    extra.push(Item::Fun(FunDecl {
                        name: sym,
                        type_params: vec![],
                        params: m.params.clone(),
                        ret: m.ret.clone(),
                        where_bounds: vec![],
                        body: m.body.clone(),
                        is_extern: false,
                    }.into()));
                }
            }
        }
    }
    p.items.extend(extra);
    p
}
