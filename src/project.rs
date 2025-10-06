// src/project.rs
use anyhow::{Context, Result};
use serde::Deserialize;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Clone, Copy, Debug)]
pub enum BuildProfile {
    Dev,
    Release,
}

#[derive(Debug, Default, Deserialize)]
struct PawToml {
    #[serde(default)]
    package: Package,
    #[serde(default)]
    module: ModuleTable, // 支持 [module] 表
}

#[derive(Debug, Default, Deserialize)]
struct Package {
    #[serde(default)]
    name: String,
    #[serde(default)]
    version: Option<String>, // 暂时不用，但允许写上不报错
    #[serde(default)]
    entry: Option<String>, // 入口文件，默认为 main.paw
}

#[derive(Debug, Default, Deserialize)]
struct ModuleTable {
    // 你的写法：[module] modules = ["std"]
    #[serde(default)]
    modules: Vec<String>,
}

/// 工程描述
#[derive(Debug, Clone)]
pub struct Project {
    /// 工程根目录（Paw.toml 所在目录）
    pub root: PathBuf,
    /// 包名（用于可执行文件名）
    pub name: String,
    /// 入口：<root>/main.paw
    pub entry: PathBuf,
    /// 额外的模块目录（相对 root）
    pub module_dirs: Vec<PathBuf>,
}

impl Project {
    /// 搜索根：用于 import 解析
    ///
    /// 约定：
    /// 1) 首位一定是工程根（root）
    /// 2) 之后把 [module].modules 里列出的目录依次追加（若存在）
    pub fn search_roots(&self) -> Vec<PathBuf> {
        let mut v = Vec::with_capacity(1 + self.module_dirs.len());
        v.push(self.root.clone());
        v.extend(self.module_dirs.iter().cloned());
        v
    }
}

/// 从当前工作目录加载工程：
/// - root = 当前工作目录
/// - 读取 <root>/Paw.toml（可选）；读取失败会使用默认名
/// - entry = <root>/main.paw
pub fn load_from_cwd() -> Result<Project> {
    let root = std::env::current_dir().context("current_dir() failed")?;
    load_from_dir(&root)
}

/// 从指定目录加载工程
pub fn load_from_dir(root: &Path) -> Result<Project> {
    let paw_toml = root.join("Paw.toml");
    let (name, module_dirs) = match fs::read_to_string(&paw_toml) {
        Ok(s) => {
            match toml::from_str::<PawToml>(&s) {
                Ok(cfg) => {
                    let pkg_name = if cfg.package.name.trim().is_empty() {
                        default_pkg_name_from_dir(root)
                    } else {
                        sanitize_pkg_name(&cfg.package.name)
                    };
                    let dirs = cfg
                        .module
                        .modules
                        .into_iter()
                        .map(|m| root.join(m))
                        .collect::<Vec<_>>();
                    (pkg_name, dirs)
                }
                Err(e) => {
                    // 项目加载警告，暂时保持直接输出
                    eprintln!(
                        "warning: 解析 `{}` 失败：{}",
                        paw_toml.display(),
                        format!("parse `{}` as TOML failed: {e}", paw_toml.display())
                    );
                    (default_pkg_name_from_dir(root), Vec::new())
                }
            }
        }
        Err(_) => {
            // 没有 Paw.toml；完全默认
            (default_pkg_name_from_dir(root), Vec::new())
        }
    };

    let entry = if let Ok(toml_content) = fs::read_to_string(root.join("Paw.toml")) {
        if let Ok(config) = toml::from_str::<PawToml>(&toml_content) {
            if let Some(entry_file) = config.package.entry {
                root.join(entry_file)
            } else {
                root.join("main.paw")
            }
        } else {
            root.join("main.paw")
        }
    } else {
        root.join("main.paw")
    };
    
    Ok(Project {
        root: root.to_path_buf(),
        name,
        entry,
        module_dirs,
    })
}

fn default_pkg_name_from_dir(root: &Path) -> String {
    // 工程根目录名为空时回退到 "app"
    let raw = root
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("app")
        .trim();
    if raw.is_empty() {
        "app".to_string()
    } else {
        sanitize_pkg_name(raw)
    }
}

fn sanitize_pkg_name(s: &str) -> String {
    // 仅保留字母数字与下划线；首字符不是字母/下划线则前置 'p'
    let mut out = String::with_capacity(s.len() + 1);
    let mut chars = s.chars();
    if let Some(c0) = chars.next() {
        if c0.is_ascii_alphabetic() || c0 == '_' {
            out.push(c0);
        } else {
            out.push('p');
            if c0.is_ascii_alphanumeric() || c0 == '_' {
                out.push(c0);
            }
        }
    } else {
        out.push('p');
    }
    for ch in chars {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            out.push(ch);
        } else {
            out.push('_');
        }
    }
    out
}
