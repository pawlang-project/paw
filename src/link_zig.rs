// src/link_zig.rs
use anyhow::{anyhow, bail, Context, Result};
use std::{
    env,
    fs,
    path::{Path, PathBuf},
    process::Command,
};

/// 使用 zig cc 跨平台链接：
/// - `obj_bytes`：Cranelift 产出的目标文件内容
/// - `out_obj`：要写入的 obj/o 文件路径
/// - `out_exe`：最终可执行文件路径
/// - `extra_c_or_obj_inputs`：其他输入（例如 "src/runtime/runtime.c" 或其他 .o/.obj/.a/.lib）
///
/// 环境变量：
/// - `ZIG`（可选）：zig 可执行路径；未设则在 PATH 中查找 `zig`
/// - `PAW_TARGET`（可选）：显式目标三元组，如 `x86_64-windows-gnu`、`aarch64-macos`、`x86_64-linux-gnu`
///   *Windows 上若未设置，则默认使用 `x86_64-windows-gnu`，以使用 zig 自带 ld.lld*
pub fn link_with_zig_cc(
    obj_bytes: &[u8],
    out_obj: impl AsRef<Path>,
    out_exe: impl AsRef<Path>,
    extra_c_or_obj_inputs: &[impl AsRef<Path>],
) -> Result<()> {
    let out_obj = out_obj.as_ref();
    let mut out_exe = out_exe.as_ref().to_path_buf();

    // 补后缀（Windows）
    if cfg!(target_os = "windows") && out_exe.extension().is_none() {
        out_exe.set_extension("exe");
    }

    // 确保目录存在 & 写入 obj
    if let Some(p) = out_obj.parent() {
        fs::create_dir_all(p).ok();
    }
    if let Some(p) = out_exe.parent() {
        fs::create_dir_all(p).ok();
    }
    fs::write(out_obj, obj_bytes).with_context(|| format!("write {}", out_obj.display()))?;

    // 找 zig
    let zig = locate_zig().ok_or_else(|| anyhow!("zig not found. Install Zig or set ZIG env var"))?;

    // 允许外部显式设定目标，否则 Windows 默认走 gnu 三元组（更不容易找外部 lld-link）
    let mut target = env::var("PAW_TARGET").ok();
    #[cfg(target_os = "windows")]
    if target.is_none() {
        target = Some("x86_64-windows-gnu".to_string());
    }

    let mut cmd = Command::new(&zig);
    cmd.arg("cc");

    // 目标
    if let Some(t) = target.as_deref() {
        cmd.args(["-target", t]);
    }

    // 主 obj
    cmd.arg(out_obj);

    // 额外输入（C 源、其他 obj、静态库等）
    for inp in extra_c_or_obj_inputs {
        cmd.arg(inp.as_ref());
    }

    // 不要强灌 -fuse-ld=lld，避免去找外部 lld-link.exe
    // 若需要查看详细命令，可临时打开 -v：
    // cmd.arg("-v");

    // 输出
    cmd.args(["-o", &out_exe.to_string_lossy()]);

    let status = cmd.status().with_context(|| format!("spawn: {:?}", cmd))?;
    if !status.success() {
        bail!("zig cc failed with status: {}", status);
    }
    Ok(())
}

/// 生成平台合适的 obj/exe 路径
pub fn default_paths(out_dir: impl AsRef<Path>, exe_stem: &str) -> (PathBuf, PathBuf) {
    let mut obj = out_dir.as_ref().join("out");
    if cfg!(target_os = "windows") {
        obj.set_extension("obj");
    } else {
        obj.set_extension("o");
    }

    let mut exe = out_dir.as_ref().join(exe_stem);
    if cfg!(target_os = "windows") {
        exe.set_extension("exe");
    }
    (obj, exe)
}

/// 查找 zig 可执行文件
fn locate_zig() -> Option<PathBuf> {
    if let Ok(p) = env::var("ZIG") {
        let pb = PathBuf::from(p);
        if pb.exists() {
            return Some(pb);
        }
    }
    // 尝试 PATH 上的 zig
    Command::new("zig").arg("--version").output().ok()?;
    Some(PathBuf::from("zig"))
}
