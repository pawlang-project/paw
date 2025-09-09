use anyhow::{bail, Context, Result};
use std::{env, fs, path::{Path, PathBuf}, process::Command};

/// 将 `obj_bytes` 写到 `out_obj`，并链接为可执行文件 `out_exe`。
/// - 默认自动选择链接器；也可通过环境变量 `PAW_LINKER` 指定（例如：`clang` / `cc` / `clang-cl` / `link.exe`）。
pub fn link_object_bytes(obj_bytes: &[u8], out_obj: impl AsRef<Path>, out_exe: impl AsRef<Path>) -> Result<()> {
    let out_obj = out_obj.as_ref();
    let out_exe = out_exe.as_ref();

    if let Some(parent) = out_obj.parent() {
        fs::create_dir_all(parent).ok();
    }
    if let Some(parent) = out_exe.parent() {
        fs::create_dir_all(parent).ok();
    }

    fs::write(out_obj, obj_bytes).with_context(|| format!("write object to {}", out_obj.display()))?;

    // 允许用户强制指定链接器
    if let Ok(linker) = env::var("PAW_LINKER") {
        run_linker(&linker, out_obj, out_exe)
            .with_context(|| format!("failed to link with PAW_LINKER={}", linker))?;
        return Ok(());
    }

    // 自动选择：按平台给出候选命令（依次尝试，直到成功）
    let candidates: Vec<LinkCmd> = if cfg!(target_os = "windows") {
        vec![
            // 1) clang（推荐，若装了 LLVM + lld）
            LinkCmd::clang(out_obj, out_exe),
            // 2) clang-cl（MSVC 风格命令行）
            LinkCmd::clang_cl(out_obj, out_exe),
            // 3) link.exe（需要在 VS 开发者命令提示符环境中）
            LinkCmd::msvc_link(out_obj, out_exe),
        ]
    } else {
        vec![
            // Unix-like: 优先 clang，其次 cc
            LinkCmd::clang(out_obj, out_exe),
            LinkCmd::cc(out_obj, out_exe),
        ]
    };

    let mut last_err: Option<anyhow::Error> = None;
    for cmd in candidates {
        match run_linker_cmd(&cmd) {
            Ok(()) => return Ok(()),
            Err(e) => last_err = Some(e),
        }
    }

    if let Some(e) = last_err {
        bail!("linking failed on this platform. Try setting PAW_LINKER.\nLast error: {e:#}");
    } else {
        bail!("no linker candidates to try");
    }
}

fn run_linker(linker: &str, obj: &Path, exe: &Path) -> Result<()> {
    let cmd = LinkCmd::custom(linker, obj, exe);
    run_linker_cmd(&cmd)
}

fn run_linker_cmd(cmd: &LinkCmd) -> Result<()> {
    let mut c = Command::new(&cmd.prog);
    c.args(&cmd.args);
    let status = c.status().with_context(|| format!("spawn `{}`", cmd.prog))?;
    if !status.success() {
        bail!("`{}` exited with {}", cmd.to_string(), status);
    }
    Ok(())
}

struct LinkCmd {
    prog: String,
    args: Vec<String>,
}
impl LinkCmd {
    fn to_string(&self) -> String {
        format!("{} {}", self.prog, self.args.join(" "))
    }

    fn clang(obj: &Path, exe: &Path) -> Self {
        // clang <obj> -o <exe>
        Self {
            prog: "clang".into(),
            args: vec![
                path(obj),
                "-o".into(),
                path(exe),
            ],
        }
    }
    fn cc(obj: &Path, exe: &Path) -> Self {
        // cc <obj> -o <exe>
        Self {
            prog: "cc".into(),
            args: vec![
                path(obj),
                "-o".into(),
                path(exe),
            ],
        }
    }
    fn clang_cl(obj: &Path, exe: &Path) -> Self {
        // clang-cl <obj> /Fe:<exe>
        Self {
            prog: "clang-cl".into(),
            args: vec![
                path(obj),
                format!("/Fe:{}", path(exe)),
            ],
        }
    }
    fn msvc_link(obj: &Path, exe: &Path) -> Self {
        // link.exe <obj> /OUT:<exe> /SUBSYSTEM:CONSOLE
        Self {
            prog: "link.exe".into(),
            args: vec![
                path(obj),
                format!("/OUT:{}", path(exe)),
                "/SUBSYSTEM:CONSOLE".into(),
            ],
        }
    }
    fn custom(linker: &str, obj: &Path, exe: &Path) -> Self {
        // 粗略假定 POSIX 风格：<linker> <obj> -o <exe>
        // 如需自定义复杂参数，请直接设置 PAW_LINKER 并自行在 shell 包装。
        Self {
            prog: linker.into(),
            args: vec![path(obj), "-o".into(), path(exe)],
        }
    }
}

fn path(p: &Path) -> String {
    p.to_string_lossy().into_owned()
}
