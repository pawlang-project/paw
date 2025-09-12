// src/link_zig.rs
use anyhow::{Context, Result};
use std::env;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PawTarget {
    WinGnuX64,
    LinuxGnuX64,
    MacosX64,
    MacosArm64,
}

impl PawTarget {
    pub fn from_env_or_default() -> Self {
        if let Ok(t) = env::var("PAW_TARGET") {
            return Self::parse(&t);
        }
        if cfg!(target_os = "windows") {
            Self::WinGnuX64
        } else if cfg!(target_os = "macos") && cfg!(target_arch = "aarch64") {
            Self::MacosArm64
        } else if cfg!(target_os = "macos") {
            Self::MacosX64
        } else {
            Self::LinuxGnuX64
        }
    }

    pub fn parse(s: &str) -> Self {
        match s {
            "x86_64-windows-gnu" => Self::WinGnuX64,
            "x86_64-linux-gnu" => Self::LinuxGnuX64,
            "x86_64-macos" | "x86_64-apple-darwin" => Self::MacosX64,
            "aarch64-macos" | "aarch64-apple-darwin" => Self::MacosArm64,
            _ => Self::LinuxGnuX64,
        }
    }

    pub fn zig_triple(&self) -> &'static str {
        match self {
            Self::WinGnuX64 => "x86_64-windows-gnu",
            Self::LinuxGnuX64 => "x86_64-linux-gnu",
            Self::MacosX64 => "x86_64-macos",
            Self::MacosArm64 => "aarch64-macos",
        }
    }

    pub fn rust_triple(&self) -> &'static str {
        match self {
            Self::WinGnuX64 => "x86_64-pc-windows-gnu",
            Self::LinuxGnuX64 => "x86_64-unknown-linux-gnu",
            Self::MacosX64 => "x86_64-apple-darwin",
            Self::MacosArm64 => "aarch64-apple-darwin",
        }
    }

    pub fn obj_ext(&self) -> &'static str {
        match self {
            Self::WinGnuX64 => "obj",
            _ => "o",
        }
    }

    pub fn exe_ext(&self) -> &'static str {
        match self {
            Self::WinGnuX64 => "exe",
            _ => "",
        }
    }

    pub fn triple_dir(&self) -> &'static str {
        // 用于 build/<triple>/ 子目录名
        self.zig_triple()
    }
}

pub struct LinkInput {
    /// 编译器后端产出的对象文件（一个或多个，必须与 target 匹配）
    pub obj_files: Vec<String>,
    /// 输出可执行文件路径
    pub out_exe: String,
    /// 目标平台
    pub target: PawTarget,
}

/// 根据 out_dir 与 exe_name 产生默认路径：
/// - 对象文件：<out_dir>/out   （不带扩展名；调用方决定加 .o/.obj）
/// - 可执行：  <out_dir>/<exe_name>[.exe]
pub fn default_paths(out_dir: &str, exe_name: &str) -> (PathBuf, PathBuf) {
    let obj_path = PathBuf::from(format!("{}/out.obj", out_dir));

    // 根据操作系统加后缀
    let mut exe = exe_name.to_string();
    if cfg!(target_os = "windows") && !exe.ends_with(".exe") {
        exe.push_str(".exe");
    }

    let exe_path = PathBuf::from(format!("{}/{}", out_dir, exe));
    (obj_path, exe_path)
}

/// ===== 工具函数 =====

fn which_on_path(cand: &str) -> Option<PathBuf> {
    let path = env::var_os("PATH")?;
    for dir in env::split_paths(&path) {
        let p = dir.join(cand);
        if p.is_file() {
            return Some(p);
        }
    }
    None
}

fn find_zig_path() -> Result<PathBuf> {
    // 1) 优先环境变量
    if let Ok(p) = env::var("ZIG_BIN").or_else(|_| env::var("ZIG")) {
        let pb = PathBuf::from(p);
        if pb.is_file() {
            return Ok(pb);
        }
    }
    // 2) PATH
    #[cfg(windows)]
    {
        if let Some(p) = which_on_path("zig.exe") {
            return Ok(p);
        }
        if let Some(p) = which_on_path("zig") {
            return Ok(p);
        }
        // 3) 常见安装位置
        let candidates = [
            r"C:\ProgramData\chocolatey\bin\zig.exe".to_string(),
            format!(r"{}\scoop\apps\zig\current\zig.exe", env::var("USERPROFILE").unwrap_or_default()),
        ];
        for c in candidates {
            let p = PathBuf::from(c);
            if p.is_file() {
                return Ok(p);
            }
        }
    }
    #[cfg(target_os = "macos")]
    {
        if let Some(p) = which_on_path("zig") {
            return Ok(p);
        }
        for c in ["/opt/homebrew/bin/zig", "/usr/local/bin/zig"] {
            let p = PathBuf::from(c);
            if p.is_file() {
                return Ok(p);
            }
        }
    }
    #[cfg(all(unix, not(target_os = "macos")))]
    {
        if let Some(p) = which_on_path("zig") {
            return Ok(p);
        }
        for c in ["/usr/bin/zig", "/usr/local/bin/zig"] {
            let p = PathBuf::from(c);
            if p.is_file() {
                return Ok(p);
            }
        }
    }
    anyhow::bail!("找不到 zig；请安装或设置环境变量 ZIG_BIN 指向 zig 可执行文件")
}

fn zig_cmd() -> Result<Command> {
    let zig = find_zig_path()?;
    let cmd = Command::new(zig);
    Ok(cmd)
}

fn run(cmd: &mut Command) -> Result<()> {
    if env::var("PAW_VERBOSE").ok().as_deref() == Some("1") {
        eprintln!("$ {:?}", cmd);
    }
    let status = cmd.status()?;
    if !status.success() {
        anyhow::bail!("command failed with status: {status}");
    }
    Ok(())
}

fn canon<P: AsRef<Path>>(p: P) -> Result<OsString> {
    Ok(std::fs::canonicalize(&p)
        .with_context(|| format!("canonicalize({})", p.as_ref().display()))?
        .into_os_string())
}

/// 在若干候选位置查找 libpawrt.a：
/// 优先级：
/// 1) 环境变量 PAWRT_LIB 指定的绝对路径
/// 2) ./deps/<zig-triple>/libpawrt.a
fn resolve_pawrt_lib(target: PawTarget) -> Result<PathBuf> {
    if let Ok(p) = env::var("PAWRT_LIB") {
        let pb = PathBuf::from(p);
        if pb.is_file() {
            return Ok(pb);
        } else {
            anyhow::bail!("PAWRT_LIB 指向的文件不存在：{}", pb.display());
        }
    }

    // 2) deps/<triple>/libpawrt.a
    let mut cand = PathBuf::from("deps");
    cand.push(target.rust_triple());
    cand.push("libpawrt.a");
    if cand.is_file() {
        return Ok(cand);
    }

    anyhow::bail!("未找到 libpawrt.a。请先构建运行时静态库，或设置环境变量 PAWRT_LIB 指向它")
}

/// ===== 对外：链接函数 =====

/// 纯对象 + 自动附带 libpawrt.a → 生成可执行文件
pub fn link_with_zig(inp: &LinkInput) -> Result<()> {
    // 确保输出目录存在
    let out_exe = PathBuf::from(&inp.out_exe);
    if let Some(parent) = out_exe.parent() {
        std::fs::create_dir_all(parent)
            .with_context(|| format!("create_dir_all({})", parent.display()))?;
    }

    // 解析 libpawrt.a
    let pawrt = resolve_pawrt_lib(inp.target)?;

    let mut cmd = zig_cmd()?;
    cmd.arg("cc")
        .arg("-target")
        .arg(inp.target.zig_triple());

    // 平台差异参数 & 系统库
    match inp.target {
        PawTarget::WinGnuX64 => {
            cmd.arg("-Wl,--subsystem,console");
            cmd.args(["-lunwind", "-lgcc_eh", "-lgcc"]);
            // 视环境需要补充的系统库（按需增减）
            cmd.args(["-lws2_32", "-ladvapi32", "-luserenv", "-lntdll", "-lshell32"]);
        }
        PawTarget::LinuxGnuX64 => {
            // 常见原生库（不同发行版可能已自动引入）
            cmd.args(["-lpthread", "-ldl", "-lrt"]);
        }
        PawTarget::MacosX64 | PawTarget::MacosArm64 => {
            cmd.arg("-mmacosx-version-min=11.0");
            // cmd.arg("-Wl,-adhoc_codesign");
            // cmd.arg("-Wl,-no_fixup_chains");
        }
    }

    // 输入对象
    for o in &inp.obj_files {
        cmd.arg(canon(o)?);
    }

    // 附带运行时静态库
    cmd.arg(canon(&pawrt)?);

    // 输出可执行
    cmd.arg("-o").arg(&out_exe);

    if env::var("PAW_VERBOSE").ok().as_deref() == Some("1") {
        eprintln!("$ {:?}", cmd);
    }

    run(&mut cmd).context("link_with_zig: zig cc link step")?;
    Ok(())
}

/// 兼容：一次性把对象们（含其它库）全部丢进来并自动附带 libpawrt.a
pub fn link_many_with_zig(obj_files: &[String], out_exe: &str, target: PawTarget) -> Result<()> {
    let inp = LinkInput {
        obj_files: obj_files.to_vec(),
        out_exe: out_exe.to_string(),
        target,
    };
    link_with_zig(&inp)
}

/// 保留：如果你还有需要把 C 源编译成对象，可使用本函数；
/// 虽然我们已切换到 Rust 运行时，但此工具函数对其它 C 源仍然有用。
pub fn compile_c_to_obj(c_src: &str, out_obj: &Path, target: PawTarget) -> Result<()> {
    if let Some(parent) = out_obj.parent() {
        std::fs::create_dir_all(parent)
            .with_context(|| format!("create_dir_all({})", parent.display()))?;
    }

    let mut cmd = zig_cmd()?;
    cmd.arg("cc")
        .arg("-target")
        .arg(target.zig_triple())
        .arg("-c")
        .arg(c_src)
        .arg("-o")
        .arg(out_obj);

    if matches!(target, PawTarget::MacosX64 | PawTarget::MacosArm64) {
        cmd.arg("-mmacosx-version-min=11.0");
    }

    run(&mut cmd).context("compile_c_to_obj: zig cc -c")?;
    Ok(())
}
