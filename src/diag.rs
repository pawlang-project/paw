//! Lightweight diagnostics + colored source rendering via Ariadne 0.5.
//! - Collect with `DiagSink` during parse/typecheck/codegen
//! - Render once at the end with `render_all`
//
// Cargo.toml:
// [dependencies]
// ariadne = "0.5"

use std::collections::HashMap;
use std::ops::Range;

use ariadne::{sources, Color, Label, Report, ReportKind};

/// Byte-span inside a source file.
pub type Span = Range<usize>;

#[derive(Clone, Copy, Debug)]
pub enum Severity {
    Error,
    Warning,
    Note,
}

/// One diagnostic message.
#[derive(Debug, Clone)]
pub struct Diag {
    /// Stable machine-readable code (e.g. "E1001")
    pub code: &'static str,
    pub severity: Severity,
    /// Primary file id (display name)
    pub file: String,
    /// Primary span (optional)
    pub span: Option<Span>,
    /// Human message
    pub message: String,
    /// Extra labels pointing to other files/spans
    pub labels: Vec<(String, Span, String)>,
    /// Optional help/note text
    pub help: Option<String>,
}

impl Diag {
    /// Add a secondary label (file, span, message)
    pub fn with_label(mut self, file: impl Into<String>, span: Span, msg: impl Into<String>) -> Self {
        self.labels.push((file.into(), span, msg.into()));
        self
    }

    /// Add a help/note paragraph
    pub fn with_help(mut self, help: impl Into<String>) -> Self {
        self.help = Some(help.into());
        self
    }
}

/// In-memory sink to collect diagnostics during compilation.
#[derive(Default, Debug)]
pub struct DiagSink {
    pub diags: Vec<Diag>,
}

impl DiagSink {
    /// Record an error
    pub fn error(
        &mut self,
        code: &'static str,
        file: impl Into<String>,
        span: Option<Span>,
        msg: impl Into<String>,
    ) {
        self.diags.push(Diag {
            code,
            severity: Severity::Error,
            file: file.into(),
            span,
            message: msg.into(),
            labels: vec![],
            help: None,
        });
    }

    /// Record a warning
    pub fn warn(
        &mut self,
        code: &'static str,
        file: impl Into<String>,
        span: Option<Span>,
        msg: impl Into<String>,
    ) {
        self.diags.push(Diag {
            code,
            severity: Severity::Warning,
            file: file.into(),
            span,
            message: msg.into(),
            labels: vec![],
            help: None,
        });
    }

    /// Record a note/advice
    pub fn note(
        &mut self,
        code: &'static str,
        file: impl Into<String>,
        span: Option<Span>,
        msg: impl Into<String>,
    ) {
        self.diags.push(Diag {
            code,
            severity: Severity::Note,
            file: file.into(),
            span,
            message: msg.into(),
            labels: vec![],
            help: None,
        });
    }

    /// Whether there is any error.
    pub fn has_error(&self) -> bool {
        self.diags.iter().any(|d| matches!(d.severity, Severity::Error))
    }
}

/// Provide sources (filename -> content) for rendering.
#[derive(Default)]
pub struct RenderSources {
    pub files: HashMap<String, String>,
}

impl RenderSources {
    /// Build from a single (name, content) pair.
    pub fn single(name: impl Into<String>, src: impl Into<String>) -> Self {
        let mut files = HashMap::new();
        files.insert(name.into(), src.into());
        Self { files }
    }

    /// Insert/replace a file source.
    pub fn insert(&mut self, name: impl Into<String>, src: impl Into<String>) {
        self.files.insert(name.into(), src.into());
    }
}

/// Map severity to a color for the primary label.
fn color_for(sev: Severity) -> Color {
    match sev {
        Severity::Error => Color::Red,
        Severity::Warning => Color::Yellow,
        Severity::Note => Color::Blue,
    }
}

/// Render all diagnostics with Ariadne 0.5.
/// Safe to call multiple times; it does not mutate `sink`.
pub fn render_all(sink: &DiagSink, sources_map: &RenderSources) {
    use ariadne::{sources, Color, Label, Report, ReportKind};

    for d in &sink.diags {
        let kind = match d.severity {
            Severity::Error => ReportKind::Error,
            Severity::Warning => ReportKind::Warning,
            Severity::Note => ReportKind::Advice,
        };

        // 主定位 span，没有就用 0..0
        let primary_span: Span = d.span.clone().unwrap_or(0..0);

        // 统一使用 String 作为 FileId，避免 &str 生命周期问题
        let file_id: String = d.file.clone();

        // 每次使用都 clone，一次性拥有，不会 move 冲突
        let mut builder = Report::build(kind, (file_id.clone(), primary_span.clone()))
            .with_code(d.code)
            .with_message(&d.message)
            .with_label(
                Label::new((file_id.clone(), primary_span.clone()))
                    .with_message(&d.message)
                    .with_color(match d.severity {
                        Severity::Error => Color::Red,
                        Severity::Warning => Color::Yellow,
                        Severity::Note => Color::Blue,
                    }),
            );

        // 次级 labels（也用 String 拷贝，保持 Id 类型一致）
        for (f, s, msg) in &d.labels {
            builder = builder.with_label(Label::new((f.clone(), s.clone())).with_message(msg));
        }

        if let Some(help) = &d.help {
            builder = builder.with_note(help);
        }

        // 关键：把源文件映射克隆为 (String, String)，满足 'static 要求
        let provider = sources(
            sources_map
                .files
                .iter()
                .map(|(k, v)| (k.clone(), v.clone())),
        );

        // 打印；忽略打印错误（例如找不到文件）
        let _ = builder.finish().print(provider);
    }
}

/// 紧凑样式渲染。
/// - 仅高亮主 label（d.file + d.span），次级 labels 以 note 形式列出。
/// - 不依赖 ariadne，纯文本输出，颜色交给终端（可自行加 ANSI）。
pub fn render_compact(sink: &DiagSink, sources: &RenderSources) {
    for d in &sink.diags {
        eprintln!("Caused by:");

        // 取源文本
        let Some(src) = sources.files.get(&d.file) else {
            // 没有源文本也尽量给出位置与信息
            if let Some(sp) = &d.span {
                eprintln!("  --> {}:{}:{}", d.file, 1, sp.start + 1);
            } else {
                eprintln!("  --> {}:1:1", d.file);
            }
            eprintln!("   |");
            eprintln!("   = {}", d.message);
            continue;
        };

        // 计算行列与行起始偏移
        let (line_no, col_no, line_start_off) = byte_to_line_col(src, d.span.as_ref().map(|s| s.start).unwrap_or(0));
        let line_str = line_at(src, line_no);

        // 计算插针长度（限定在当前行内）
        let (caret_len, col0_based) = {
            let start = d.span.as_ref().map(|s| s.start).unwrap_or(line_start_off);
            let end   = d.span.as_ref().map(|s| s.end).unwrap_or(start);
            let line_end_off = line_start_off + line_str.len();
            let s = start.clamp(line_start_off, line_end_off);
            let e = end.clamp(s, line_end_off);
            let len = std::cmp::max(1, e.saturating_sub(s));
            (len, col_no.saturating_sub(1))
        };

        // 构造箭头行：在列前填空格，然后 '^' + 若干 '-'
        let mut underline = String::new();
        // 注意：为避免制表符错位，这里把 '\t' 展开为 4 个空格（行文本与下划线都这样做）
        let (line_detab, col_detab) = detab_with_col(line_str, col0_based, 4);
        let caret_len_detab = caret_len; // 近似：不展开多宽字符，只对齐空格/制表符
        let mut pad = String::new();
        for _ in 0..col_detab { pad.push(' '); }
        underline.push('^');
        for _ in 1..caret_len_detab { underline.push('-'); }

        // 打印块
        eprintln!("  --> {}:{}:{}", d.file, line_no, col_no);
        eprintln!("   |");
        eprintln!("{:>3} | {}", line_no, line_detab);
        eprintln!("   | {}{}", pad, underline);
        eprintln!("   |");
        // 主消息
        eprintln!("   = {}", d.message);

        // 次级 labels（若有）
        for (lf, lsp, lmsg) in &d.labels {
            if let Some(lsrc) = sources.files.get(lf) {
                let (ll, lc, _) = byte_to_line_col(lsrc, lsp.start);
                eprintln!("   = note: {} (at {}:{}:{})", lmsg, lf, ll, lc);
            } else {
                eprintln!("   = note: {} (at {}:{}..{})", lmsg, lf, lsp.start, lsp.end);
            }
        }

        // 可选 help
        if let Some(h) = &d.help {
            eprintln!("   = help: {}", h);
        }
    }
}

fn byte_to_line_col(src: &str, byte_off: usize) -> (usize /*1-based line*/, usize /*1-based col*/, usize /*line_start_off*/) {
    // 找到包含 byte_off 的那一行
    let mut line_no = 1usize;
    let mut cur_start = 0usize;
    let mut last_nl = 0usize;

    // 线性扫描（文本不大时足够；需要的话可缓存行起点）
    for (i, ch) in src.char_indices() {
        if i >= byte_off { break; }
        if ch == '\n' {
            line_no += 1;
            cur_start = i + 1;
            last_nl = i;
        }
    }
    let col = byte_off.saturating_sub(cur_start) + 1;
    (line_no, col, cur_start)
}

fn line_at<'a>(src: &'a str, line_no_1: usize) -> &'a str {
    src.lines().nth(line_no_1.saturating_sub(1)).unwrap_or("")
}

fn detab_with_col(line: &str, col0: usize, tabw: usize) -> (String, usize) {
    // 把行内 '\t' 展开成 tabw 个空格；同时计算展开后 caret 前的列数
    let mut out = String::with_capacity(line.len() + 8);
    let mut cur_col = 0usize;
    let mut caret_col = 0usize;

    for (i, ch) in line.chars().enumerate() {
        if ch == '\t' {
            let add = tabw;
            for _ in 0..add { out.push(' '); }
            if i < col0 { cur_col += add; caret_col = cur_col; } else { cur_col += add; }
        } else {
            out.push(ch);
            if i < col0 { cur_col += 1; caret_col = cur_col; } else { cur_col += 1; }
        }
    }
    if col0 >= line.chars().count() { caret_col = cur_col; }
    (out, caret_col)
}

