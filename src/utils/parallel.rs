//! 并行处理工具模块
//! 使用 Rayon 实现编译器的并行处理

use rayon::prelude::*;
use crate::frontend::ast::*;
use crate::diag::{SourceMap, DiagSink};
use anyhow::{Context, Result};
use std::path::PathBuf;
use std::fs;

/// 并行读取文件内容
pub fn read_files_parallel(file_paths: &[PathBuf]) -> Result<Vec<(PathBuf, String)>> {
    let results: Result<Vec<_>> = file_paths
        .par_iter()
        .map(|path| {
            let content = fs::read_to_string(path)
                .with_context(|| format!("Failed to read file: {}", path.display()))?;
            Ok((path.clone(), content))
        })
        .collect();

    results
}

/// 并行处理导入展开 - 简化版本，避免借用检查问题
pub fn expand_imports_parallel(
    root: &Program,
    roots: &[PathBuf],
    sm: &mut SourceMap,
    diags: &mut DiagSink,
) -> Result<Program> {
    // 对于简单的单文件项目，直接使用串行版本
    // 并行处理需要更复杂的架构设计
    crate::middle::passes::expand_imports_with_roots(root, roots, sm, diags)
}

/// 并行处理多个编译任务
pub fn compile_multiple_programs_parallel(
    programs: &[Program],
    file_names: &[String],
    target: Option<&str>,
) -> Result<Vec<Vec<u8>>> {
    // 并行编译多个程序
    let results: Result<Vec<_>> = programs
        .par_iter()
        .zip(file_names.par_iter())
        .map(|(prog, file_name)| {
            crate::backend::codegen::compile_program_for_target(
                prog, 
                None, 
                vec![file_name.clone()], 
                target
            )
        })
        .collect();

    results
}

/// 并行处理字符串操作
pub fn process_strings_parallel(strings: &[String]) -> Vec<String> {
    strings
        .par_iter()
        .map(|s| s.to_uppercase())
        .collect()
}

/// 并行处理数字计算
pub fn compute_parallel(numbers: &[i32]) -> i32 {
    numbers
        .par_iter()
        .sum()
}

// 辅助函数（从 passes.rs 复制）
fn resolve_double_colon_import(spec: &str, roots: &[PathBuf]) -> Result<PathBuf> {
    let parts: Vec<&str> = spec.split("::").collect();
    if parts.is_empty() {
        return Err(anyhow::anyhow!("empty import spec"));
    }

    for root in roots {
        let mut path = root.clone();
        for part in &parts {
            path.push(part);
        }
        path.set_extension("paw");
        if path.exists() {
            return Ok(path);
        }
    }

    Err(anyhow::anyhow!("import not found: {}", spec))
}

fn canonicalize_lossy(path: &PathBuf) -> Result<PathBuf> {
    Ok(path.canonicalize().unwrap_or_else(|_| path.clone()))
}
