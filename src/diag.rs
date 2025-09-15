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
