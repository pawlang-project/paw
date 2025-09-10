// src/link_zig.rs
use anyhow::{bail, Context, Result};
use std::{
    env,
    fs,
    path::{Path, PathBuf},
    process::Command,
};

/// 写入 obj 并用 zig cc 链接成可执行文件。
/// - `obj_bytes`: Cranelift ObjectModule::finish() 的字节
/// - `out_obj`:   目标文件路径（.o 或 .obj）
/// - `out_exe`:   可执行文件路径（自动补 .exe on Windows）
/// - `target`:    可选 Zig target 三元组（例如 "x86_64-windows-msvc" / "aarch64-macos" / "x86_64-linux-gnu"）
pub fn link_with_zig_cc(
    obj_bytes: &[u8],
    out_obj: impl AsRef<Path>,
    out_exe: impl AsRef<Path>,
    target: Option<&str>,
    extra_args: &[&str],
) -> Result<()> {
    let out_obj = out_obj.as_ref();
    let mut out_exe = out_exe.as_ref().to_path_buf();

    if cfg!(target_os = "windows") && out_exe.extension().is_none() {
        out_exe.set_extension("exe");
    }
    if let Some(p) = out_obj.parent() { fs::create_dir_all(p).ok(); }
    if let Some(p) = out_exe.parent() { fs::create_dir_all(p).ok(); }
    fs::write(out_obj, obj_bytes).with_context(|| format!("write {}", out_obj.display()))?;

    // 1) 定位 zig（允许用 ZIG 环境变量覆盖）
    let zig = locate_zig().ok_or_else(|| anyhow::anyhow!(
        "zig not found. Install Zig or set ZIG=/path/to/zig\n\
        see https://ziglang.org/learn/getting-started/ for more information"
    ))?;

    // 2) 组装命令：zig cc [ -target ... ] <obj> -o <exe> [extra]
    let mut cmd = Command::new(&zig);
    cmd.arg("cc");
    if let Some(t) = target {
        cmd.args(["-target", t]);
    }
    cmd.arg(path(out_obj))
        .args(["-o", &*path(&out_exe)]);
    // Windows：优先用 lld（zig 自带，通常不必显式指定；这里给出示例）
    if cfg!(target_os = "windows") {
        cmd.arg("-fuse-ld=lld");
    }
    // 附加自定义参数（例如 -static, 额外库等）
    for &a in extra_args { cmd.arg(a); }

    let status = cmd.status().with_context(|| format!("spawn: {:?}", cmd))?;
    if !status.success() {
        bail!("zig cc failed with exit code {}", status);
    }
    Ok(())
}

/// 生成默认 obj/exe 路径（跨平台扩展名）
/// 例：let (obj, exe) = default_paths("build", "pawlang");
pub fn default_paths(out_dir: impl AsRef<Path>, exe_stem: &str) -> (PathBuf, PathBuf) {
    let mut obj = out_dir.as_ref().join("out");
    if cfg!(target_os = "windows") { obj.set_extension("obj"); } else { obj.set_extension("o"); }

    let mut exe = out_dir.as_ref().join(exe_stem);
    if cfg!(target_os = "windows") { exe.set_extension("exe"); }
    (obj, exe)
}

fn locate_zig() -> Option<PathBuf> {
    if let Ok(p) = env::var("ZIG") {
        let pb = PathBuf::from(p);
        if pb.exists() { return Some(pb); }
    }
    which("zig")
}

fn which(bin: &str) -> Option<PathBuf> {
    Command::new(bin).arg("--version").output().ok()?;
    Some(PathBuf::from(bin))
}

fn path(p: &Path) -> String { p.to_string_lossy().into_owned() }
