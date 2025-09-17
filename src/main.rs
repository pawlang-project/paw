// src/main.rs
use anyhow::{Context, Result};
use std::fs;
use std::path::Path;
use std::{env, process};

use std::cell::RefCell;
use std::rc::Rc;

mod project;
mod frontend;
mod middle;
mod backend;
mod interner;
mod utils;
mod diag;

use backend::link_zig::{link_with_zig, LinkInput, PawTarget};
use crate::backend::codegen;
use crate::frontend::parser;
use frontend::ast::Program;
use middle::passes;
use middle::typecheck::typecheck_program;
use project::{BuildProfile, Project};

// 使用紧凑渲染器
use crate::diag::{DiagSink, RenderSources, render_compact};

/// 读取 entry 源码 -> 解析 -> 以工程根目录为搜索根展开 import
fn load_and_expand_program(entry: &Path, project_root: &Path) -> Result<Program> {
    let src = fs::read_to_string(entry)
        .with_context(|| format!("read_to_string({}) failed", entry.display()))?;

    let root = parser::parse_program(&src)
        .with_context(|| format!("parse `{}` failed", entry.display()))?;

    let merged = passes::expand_imports_with_loader(&root, project_root)
        .context("expand imports failed")?;

    Ok(merged)
}

/// 把完整 Program 编译为 object bytes
///
/// - `file_id`: 类型检查阶段的“主文件名”，用入口路径字符串即可
/// - `diag`:    共享诊断（typecheck / codegen 共用）
fn build_object(prog: &Program, file_id: &str, diag: Rc<RefCell<DiagSink>>) -> Result<Vec<u8>> {
    // 先类型检查（错误写入 diag，同时返回 Err）
    {
        // 注意作用域，确保可变借用在进入 codegen 前释放
        let mut d = diag.borrow_mut();
        let _ = typecheck_program(prog, file_id, &mut *d)?;
    }

    // 后端生成对象文件（带同一个 diag，用于 codegen 期错误）
    codegen::compile_program(prog, Some(diag))
}

fn main() -> Result<()> {
    // 仅支持：pawc build dev | pawc build release
    let args = env::args().skip(1).collect::<Vec<_>>();
    if args.len() != 2 || args[0].as_str() != "build" {
        eprintln!("usage: pawc build <dev|release>");
        process::exit(1);
    }

    let profile = match args[1].as_str() {
        "dev" => BuildProfile::Dev,
        "release" => BuildProfile::Release,
        _ => {
            eprintln!("usage: pawc build <dev|release>");
            process::exit(1);
        }
    };

    // 1) 载入工程
    let proj: Project = project::load_from_cwd()
        .context("failed to load project (Paw.toml or defaults)")?;

    // 2) 解析 + 展开 import
    let entry = proj.entry.clone(); // e.g. <root>/main.paw
    let program = load_and_expand_program(&entry, &proj.root)?;

    // 3) 准备诊断收集器（typecheck + codegen 共用）
    let diag = Rc::new(RefCell::new(DiagSink::default()));
    let file_id = entry.to_string_lossy().to_string();

    // 4) 编译（失败则渲染紧凑样式并退出）
    let obj_bytes = match build_object(&program, &file_id, diag.clone()) {
        Ok(obj) => obj,
        Err(e) => {
            // 渲染需要的源：至少把入口文件放进来
            let mut srcs = RenderSources::default();
            if let Ok(text) = fs::read_to_string(&entry) {
                srcs.insert(file_id.clone(), text);
            }
            // TODO: 如需把 import 的各源文件也渲染出来，可在 passes 中暴露源表，在此一并 insert

            render_compact(&diag.borrow(), &srcs);

            eprintln!("compilation failed: {e:#}");
            process::exit(1);
        }
    };

    // 5) 输出目录
    let profile_dir = match profile {
        BuildProfile::Dev => "dev",
        BuildProfile::Release => "release",
    };
    let out_dir = proj.root.join("build").join(profile_dir);
    fs::create_dir_all(&out_dir)
        .with_context(|| format!("create_dir_all({})", out_dir.display()))?;

    // 6) 写对象文件与可执行文件
    let target = PawTarget::from_env_or_default();
    let obj_ext = target.obj_ext();
    let exe_ext = target.exe_ext();

    let obj_path = out_dir.join(format!("out.{}", obj_ext));
    fs::write(&obj_path, &obj_bytes)
        .with_context(|| format!("write object `{}` failed", obj_path.display()))?;

    let exe_stem = if proj.name.trim().is_empty() {
        "app".to_string()
    } else {
        proj.name.clone()
    };
    let exe_file = if exe_ext.is_empty() {
        exe_stem.clone()
    } else {
        format!("{}.{}", exe_stem, exe_ext)
    };
    let exe_path = out_dir.join(exe_file);

    // 7) 链接
    let inp = LinkInput {
        obj_files: vec![obj_path.to_string_lossy().to_string()],
        out_exe: exe_path.to_string_lossy().to_string(),
        target,
    };
    link_with_zig(&inp).context("link step failed")?;

    eprintln!("OK: {} -> {}", entry.display(), exe_path.display());
    Ok(())
}
