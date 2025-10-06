// src/main.rs
use anyhow::{Context, Result};
use std::fs;
use std::path::Path;
use std::process;
use std::time::Instant;

use std::cell::RefCell;
use std::rc::Rc;
// 性能监控宏在 performance 模块中定义

mod project;
mod frontend;
mod middle;
mod backend;
mod utils;
mod diag;
mod cli;

use backend::link_zig::{link_with_zig, LinkInput, PawTarget};
use crate::backend::codegen;
use crate::cli::{CliArgs, Command, BuildProfile as CliBuildProfile};
use crate::frontend::parser;
use frontend::ast::Program;
use middle::passes;
use middle::typecheck::typecheck_program;
use project::{BuildProfile, Project};

use crate::diag::{DiagSink, SourceMap, render_diagnostics_colored};
use crate::cli::{ProgressBar, OutputFormatter};
use crate::utils::parallel;

/// 读取 entry 源码 -> 解析 -> 以工程根目录为搜索根展开 import -> impl 降解
fn load_and_expand_program(entry: &Path, project_root: &Path, sm: &mut SourceMap, diags: &mut DiagSink) -> Result<Program> {
    let src = fs::read_to_string(entry)
        .with_context(|| format!("read_to_string({}) failed", entry.display()))?;

    // 注册入口文件，提供真实 FileId
    let fid = sm.add_file(entry.display().to_string(), src.clone());
    let root = parser::parse_program_with_diags(&src, fid, &entry.display().to_string(), diags)
        .with_context(|| format!("parse `{}` failed", entry.display()))?;

    // 使用并行版本展开 import（a::b::c -> <root>/a/b/c.paw）
    let merged = parallel::expand_imports_parallel(&root, &[project_root.to_path_buf()], sm, diags)
        .context("expand imports failed")?;

    // 将 impl 方法降解为自由函数，便于后续类型检查/后端处理
    let lowered = passes::declare_impls_from_program(merged);

    Ok(lowered)
}

/// 把完整 Program 编译为 object bytes
///
/// - `file_id`: 类型检查阶段的"主文件名"，用入口路径字符串即可
/// - `diag`:    共享诊断（typecheck / codegen 共用）
/// - `target`:  目标平台（用于交叉编译）
fn build_object(prog: &Program, file_id: &str, diag: Rc<RefCell<DiagSink>>, sm: &SourceMap, target: Option<&str>) -> Result<Vec<u8>> {
    // 先类型检查（错误写入 diag，同时返回 Err）
    {
        perf_timer!("typecheck");
        // 注意作用域，确保可变借用在进入 codegen 前释放
        let mut d = diag.borrow_mut();
        let _ = typecheck_program(prog, file_id, &mut *d)?;
    }

    // 后端生成对象文件（带同一个 diag，用于 codegen 期错误）
    {
        perf_timer!("codegen");
        let result = codegen::compile_program_for_target(prog, Some(diag), sm.file_names(), target);
        result
    }
}

fn main() -> Result<()> {
    let start_time = Instant::now();
    
    // 解析命令行参数
    let cli_args = match CliArgs::parse() {
        Ok(args) => args,
        Err(err) => {
            eprintln!("Error: {}", err);
            CliArgs::print_usage_error();
            process::exit(1);
        }
    };

    // 处理不同的命令
    match cli_args.command {
        Command::Help => {
            CliArgs::print_help();
            return Ok(());
        }
        Command::ListTargets => {
            CliArgs::print_supported_targets();
            return Ok(());
        }
        Command::Build { profile, target, quiet, input_file } => {
            // 转换CLI的BuildProfile到项目的BuildProfile
            let build_profile = match profile {
                CliBuildProfile::Dev => BuildProfile::Dev,
                CliBuildProfile::Release => BuildProfile::Release,
            };

    // 1) 载入工程
    let mut progress = if !quiet {
        Some(ProgressBar::new(20, 4, "Compiling".to_string()))
    } else {
        None
    };

    if let Some(ref mut p) = progress {
        p.update(1);
    }
    
    let proj: Project = project::load_from_cwd()
        .context("failed to load project (Paw.toml or defaults)")?;

    // 2) 提前准备诊断收集器（parser/import/typecheck/codegen 共用）
    let diag = Rc::new(RefCell::new(DiagSink::default()));

    // 3) 解析 + 展开 import + impl 降解
    if let Some(ref mut p) = progress {
        p.update(2);
    }
    
    let entry = if let Some(input_file) = input_file {
        std::path::PathBuf::from(input_file)
    } else {
        proj.entry.clone() // e.g. <root>/main.paw
    };
    let mut sm = SourceMap::new();
    // 使用局部 DiagSink 收集解析/导入阶段的错误，避免与共享 RefCell 交错借用
    let program = {
        let mut local_diags = DiagSink::new();
        match load_and_expand_program(&entry, &proj.root, &mut sm, &mut local_diags) {
            Ok(p) => {
                // 合并到共享 diag（不可变借用，不会冲突）
                diag.borrow_mut().append_from(local_diags.into_vec());
                if let Some(ref mut prog) = progress {
                    prog.update(3);
                }
                p
            }
            Err(e) => {
                let v = local_diags.into_vec();
                if v.is_empty() {
                    eprintln!("Parse failed: {}", e);
                } else {
                    eprintln!("Parse errors:");
                    render_diagnostics_colored(&v, &sm);
                }
                process::exit(1);
            }
        }
    };
    let file_id = entry.to_string_lossy().to_string();

    // 4) 编译（失败则打印收集到的诊断并退出）
    let target_triple = target.as_deref();
    
    let obj_bytes = match build_object(&program, &file_id, diag.clone(), &sm, target_triple) {
        Ok(obj) => {
            if let Some(ref mut prog) = progress {
                prog.update(4);
                prog.finish();
            }
            obj
        },
        Err(e) => {
            let d = diag.borrow();
            let v = d.iter().cloned().collect::<Vec<_>>();
            if v.is_empty() {
                eprintln!("Compilation failed: {}", e);
            } else {
                eprintln!("Compilation errors:");
                render_diagnostics_colored(&v, &sm);
            }
            process::exit(1);
        }
    };

            // 5) 输出目录（按平台和配置分离，像Rust一样）
            let profile_dir = match build_profile {
                BuildProfile::Dev => "debug",
                BuildProfile::Release => "release",
            };
    
    // 确定目标平台
    let paw_target = if let Some(t) = target {
        PawTarget::parse(&t)
    } else {
        PawTarget::from_env_or_default()
    };
    
    // 创建平台特定的目录结构：build/{profile}/{target}/
    let target_triple = paw_target.rust_triple();
    let out_dir = proj.root.join("target").join(profile_dir).join(target_triple);
    fs::create_dir_all(&out_dir)
        .with_context(|| format!("create_dir_all({})", out_dir.display()))?;

    // 6) 写对象文件与可执行文件
    let obj_ext = paw_target.obj_ext();
    let exe_ext = paw_target.exe_ext();

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
        target: paw_target,
    };
    link_with_zig(&inp).context("link step failed")?;

    let duration = start_time.elapsed();
    let formatter = OutputFormatter::new();
    
            if quiet {
                eprintln!("{}", exe_path.display());
            } else {
                // 相对路径展示
                let entry_rel = entry.strip_prefix(&proj.root).unwrap_or(&entry);
                let exe_rel = exe_path.strip_prefix(&proj.root).unwrap_or(&exe_path);
                // 可执行体大小（人类可读）
                let exe_size_bytes = fs::metadata(&exe_path).map(|m| m.len()).unwrap_or(0);
                let human_size = OutputFormatter::human_size(exe_size_bytes);
                // 目标三元组
                let triple = paw_target.rust_triple();
                
                formatter.success(
                    profile_dir,
                    triple,
                    entry_rel,
                    exe_rel,
                    duration.as_secs_f64(),
                    &human_size
                );
            }
            Ok(())
        }
    }
}
