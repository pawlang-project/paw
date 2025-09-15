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
    for d in &sink.diags {
        let kind = match d.severity {
            Severity::Error => ReportKind::Error,
            Severity::Warning => ReportKind::Warning,
            Severity::Note => ReportKind::Advice,
        };

        // Primary anchor: if no span, use 0..0 (Ariadne requires a span)
        let primary_span: Span = d.span.clone().unwrap_or(0..0);
        let file_id = d.file.as_str();

        // IMPORTANT: don't reuse moved tuples; build fresh tuples each time
        let mut builder = Report::build(kind, (file_id, primary_span.clone()))
            .with_code(d.code)
            .with_message(&d.message)
            .with_label(
                Label::new((file_id, primary_span.clone()))
                    .with_message(&d.message)
                    .with_color(color_for(d.severity)),
            );

        // Secondary labels
        for (f, s, msg) in &d.labels {
            builder = builder.with_label(Label::new((f.as_str(), s.clone())).with_message(msg));
        }

        // Optional help/note
        if let Some(help) = &d.help {
            builder = builder.with_note(help);
        }

        // Build a multi-source provider: EXPECTS (file_id: &str, content: &str)
        let provider = sources(
            sources_map
                .files
                .iter()
                .map(|(k, v)| (k.as_str(), v.as_str())),
        );

        // Print report
        let _ = builder.finish().print(provider);
    }
}
