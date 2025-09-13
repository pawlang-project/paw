// src/main.rs
use anyhow::{Context, Result};
use std::fs;
use std::path::{Path, PathBuf};
use std::{env, process};

mod ast;
mod typecheck;
mod codegen;
mod mangle;
mod parser;
mod passes;
mod link_zig;

use ast::{Item, Program};
use codegen::CLBackend;
use typecheck::typecheck_program;
use link_zig::{PawTarget, LinkInput, link_with_zig};

/// 从入口文件读取源码 -> 解析 -> 以“入口文件所在目录”为基准展开 import
fn load_and_expand_program(entry: &str) -> Result<Program> {
    // 读入口文件源码
    let src = fs::read_to_string(entry)
        .with_context(|| format!("read_to_string({entry}) failed"))?;
    // 解析
    let root = parser::parse_program(&src)
        .with_context(|| format!("parse `{entry}` failed"))?;

    // 以入口文件所在目录为首次搜索根
    let base_dir = Path::new(entry)
        .parent()
        .unwrap_or(Path::new("."))
        .to_path_buf();

    // 展开 import（内部会再附加其它搜索根：cwd / 环境变量等）
    let merged = passes::expand_imports_with_base(&root, &base_dir)
        .context("expand imports failed")?;

    Ok(merged)
}

/// 把完整 Program 编译为目标文件（Object bytes）
fn build_object(prog: &Program) -> Result<Vec<u8>> {
    // 0) 类型检查（能早发现 where / trait / impl 问题）
    let _ = typecheck_program(prog)?;

    // 1) 后端初始化 & 可内联常量
    let mut be = CLBackend::new()?;
    be.set_globals_from_program(prog);

    // 2) 声明所有“非泛型/extern”函数
    //    注意这里收集到的是 **值** 的 Vec<FunDecl>
    let funs: Vec<_> = prog.items.iter().filter_map(|it| {
        if let Item::Fun(f) = it { Some(f.clone()) } else { None }
    }).collect();
    let base_ids = be.declare_fns(&funs)?;

    // 3) 声明所有 impl 方法符号（__impl_{Trait}${Args}__{method}）
    be.declare_impls_from_program(prog)?;

    // 4) 扫描显式类型实参的泛型调用，声明对应单态化符号
    be.declare_mono_from_program(prog)?;

    // 5) 定义普通（非泛型、非 extern）函数体
    for f in &funs { be.define_fn(f, &base_ids)?; }

    // 6) 定义 impl 方法体
    be.define_impls_from_program(prog)?;

    // 7) 定义已声明的单态化实例函数体
    be.define_mono_from_program(prog, &base_ids)?;

    // 8) 产出 object bytes
    be.finish()
}

fn main() -> Result<()> {
    // 用法：pawc <input.paw> [-o out] [--target <triple>] [-c]
    let mut args = env::args().skip(1).collect::<Vec<_>>();
    if args.is_empty() {
        eprintln!("usage: pawc <input.paw> [-o OUT] [--target <triple>] [-c]");
        process::exit(1);
    }

    // 解析选项
    let mut out_path: Option<String> = None;
    let mut compile_only = false;
    let mut target_override: Option<PawTarget> = None;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-o" => {
                if i + 1 >= args.len() {
                    anyhow::bail!("`-o` requires an output file name");
                }
                out_path = Some(args[i + 1].clone());
                args.drain(i..=i + 1);
            }
            "-c" => {
                compile_only = true;
                args.remove(i);
            }
            "--target" => {
                if i + 1 >= args.len() {
                    anyhow::bail!("`--target` requires a triple, e.g. x86_64-unknown-linux-gnu");
                }
                target_override = Some(PawTarget::parse(&args[i + 1]));
                args.drain(i..=i + 1);
            }
            _ => i += 1,
        }
    }
    let entry = &args[0];

    // 1) 读取/解析/展开 import
    let program = load_and_expand_program(entry)?;

    // 2) 编译为 object bytes
    let obj_bytes = build_object(&program)?;

    // 3) 决定目标平台与输出路径
    let target = target_override.unwrap_or_else(PawTarget::from_env_or_default);
    let triple_dir = target.triple_dir();
    let obj_ext = target.obj_ext();
    let exe_ext = target.exe_ext();

    // 默认输出目录：build/<triple>/
    let out_dir = PathBuf::from(format!("build/{}", triple_dir));
    fs::create_dir_all(&out_dir)
        .with_context(|| format!("create_dir_all({})", out_dir.display()))?;

    // 默认对象路径：build/<triple>/out.<ext>
    let default_obj_path = out_dir.join(format!("out.{}", obj_ext));

    // 默认可执行名：入口文件名（不含扩展）
    let entry_stem = Path::new(entry)
        .file_stem()
        .map(|s| s.to_string_lossy().to_string())
        .unwrap_or_else(|| "a".to_string());
    let default_exe_name = if exe_ext.is_empty() {
        entry_stem.clone()
    } else {
        format!("{}.{}", entry_stem, exe_ext)
    };
    let default_exe_path = out_dir.join(default_exe_name);

    if compile_only {
        // 仅写对象
        let obj_path = PathBuf::from(out_path.unwrap_or_else(|| default_obj_path.to_string_lossy().to_string()));
        fs::write(&obj_path, &obj_bytes)
            .with_context(|| format!("write object `{}` failed", obj_path.display()))?;
        eprintln!("OK (compile only): {} -> {}", entry, obj_path.display());
        return Ok(());
    }

    // 4) 链接：先把对象写到 build 目录，再调用 zig cc
    let obj_path = default_obj_path; // 链接流程统一使用默认对象位置
    fs::write(&obj_path, &obj_bytes)
        .with_context(|| format!("write object `{}` failed", obj_path.display()))?;

    let exe_path = PathBuf::from(out_path.unwrap_or_else(|| default_exe_path.to_string_lossy().to_string()));

    let inp = LinkInput {
        obj_files: vec![obj_path.to_string_lossy().to_string()],
        out_exe: exe_path.to_string_lossy().to_string(),
        target,
    };

    link_with_zig(&inp).context("link step failed")?;

    eprintln!("OK: {} -> {}", entry, exe_path.display());
    Ok(())
}
