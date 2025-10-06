// src/diag.rs
use std::fmt;
use std::ops::Range;

// 统一复用 frontend 的 Span / FileId，并沿用 crate::diag::Span 的访问路径
pub use crate::frontend::span::Span;

#[derive(Clone, Debug)]
pub enum Severity {
    Error,
    Warning,
    Note,
}

#[derive(Clone, Debug)]
pub struct Diagnostic {
    pub code: String,
    pub file_id: String,       // 用于显示的“逻辑文件名/路径”字符串
    pub span: Option<Span>,    // 现在是 frontend::span::Span
    pub message: String,
    pub severity: Severity,
}

#[derive(Default, Clone)]
pub struct DiagSink {
    buf: Vec<Diagnostic>,
}

impl DiagSink {
    #[inline]
    pub fn new() -> Self { Self { buf: Vec::new() } }

    /// 记录 error（与现有调用保持兼容）
    pub fn error<S: Into<String>>(
        &mut self,
        code: &str,
        file_id: &str,
        span: Option<Span>,
        msg: S,
    ) {
        self.buf.push(Diagnostic {
            code: code.to_string(),
            file_id: file_id.to_string(),
            span,
            message: msg.into(),
            severity: Severity::Error,
        });
    }

    pub fn warn<S: Into<String>>(
        &mut self,
        code: &str,
        file_id: &str,
        span: Option<Span>,
        msg: S,
    ) {
        self.buf.push(Diagnostic {
            code: code.to_string(),
            file_id: file_id.to_string(),
            span,
            message: msg.into(),
            severity: Severity::Warning,
        });
    }

    pub fn note<S: Into<String>>(
        &mut self,
        code: &str,
        file_id: &str,
        span: Option<Span>,
        msg: S,
    ) {
        self.buf.push(Diagnostic {
            code: code.to_string(),
            file_id: file_id.to_string(),
            span,
            message: msg.into(),
            severity: Severity::Note,
        });
    }

    #[inline] pub fn is_empty(&self) -> bool { self.buf.is_empty() }
    #[inline] pub fn len(&self) -> usize { self.buf.len() }
    #[inline] pub fn iter(&self) -> impl Iterator<Item = &Diagnostic> { self.buf.iter() }
    #[inline] pub fn into_vec(self) -> Vec<Diagnostic> { self.buf }

    /// 把另一批诊断条目追加进来
    #[inline]
    pub fn append_from(&mut self, mut other: Vec<Diagnostic>) {
        self.buf.append(&mut other);
    }
}

// 可选：便于调试/日志
impl fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let sev = match self.severity {
            Severity::Error => "error",
            Severity::Warning => "warning",
            Severity::Note => "note",
        };
        if let Some(sp) = self.span {
            write!(
                f,
                "[{} {}] {} @{}:{}..{}: {}",
                sev, self.code, self.file_id, sp.file.0, sp.start, sp.end, self.message
            )
        } else {
            write!(f, "[{} {}] {}: {}", sev, self.code, self.file_id, self.message)
        }
    }
}

/* =============================
 * SourceMap: 文件注册与行列换算
 * ============================= */

#[derive(Default, Clone)]
pub struct SourceMap {
    files: Vec<(String, String, Vec<usize>)>, // (file_id_display, source, line_starts)
}

impl SourceMap {
    pub fn new() -> Self { Self { files: Vec::new() } }

    /// 注册文件，返回逻辑 FileId（与 frontend::span::FileId 的内部 usize 对齐）
    pub fn add_file(&mut self, file_display: String, source: String) -> crate::frontend::span::FileId {
        let mut starts = Vec::new();
        starts.push(0);
        for (i, b) in source.as_bytes().iter().enumerate() {
            if *b == b'\n' { starts.push(i + 1); }
        }
        let id = self.files.len();
        self.files.push((file_display, source, starts));
        crate::frontend::span::FileId(id)
    }

    /// 通过 FileId 取得（显示名, 源码）
    pub fn get(&self, id: crate::frontend::span::FileId) -> Option<(&str, &str)> {
        self.files.get(id.0).map(|(n, s, _)| (n.as_str(), s.as_str()))
    }

    /// 把字节偏移转换为 (line, col)，均为 1-based
    pub fn line_col(&self, id: crate::frontend::span::FileId, byte_off: usize) -> Option<(usize, usize)> {
        let (_, src, starts) = self.files.get(id.0)?;
        if starts.is_empty() { return Some((1, 1)); }
        let mut lo = 0usize;
        let mut hi = starts.len();
        while lo + 1 < hi {
            let mid = (lo + hi) / 2;
            if starts[mid] <= byte_off { lo = mid; } else { hi = mid; }
        }
        let line_idx = lo; // 0-based 行号
        let col = byte_off.saturating_sub(starts[line_idx]) + 1;
        Some((line_idx + 1, col))
    }

    /// 提供一个源码切片，扩展到整行范围，返回 (display, line_str, caret_col)
    pub fn line_slice_and_caret(&self, sp: Span) -> Option<(&str, &str, usize)> {
        let (name, src) = self.get(sp.file)?;
        let (_, _, starts) = self.files.get(sp.file.0)?;
        let (line, col) = self.line_col(sp.file, sp.start)?;
        let line_start = starts[line - 1];
        let line_end = if line < starts.len() { starts[line] - 1 } else { src.len() };
        let line_end = line_end.min(src.len());
        let line_str = &src[line_start..line_end];
        Some((name, line_str, col))
    }

    /// 导出按 FileId 顺序的显示名列表（供后端 codegen 诊断使用）
    pub fn file_names(&self) -> Vec<String> {
        self.files.iter().map(|(n, _, _)| n.clone()).collect()
    }
}

/* =============================
 * 彩色渲染（基于 ariadne）
 * ============================= */

pub fn render_diagnostics_colored(diags: &[Diagnostic], sm: &SourceMap) {
    use ariadne::{Color, Label, Report, ReportKind, sources};

    // ariadne::sources 期望 (Id, S) 且 S: AsRef<str>
    let mut srcs: Vec<(String, String)> = Vec::new();
    for (name, src, _) in sm.files.iter() {
        srcs.push((name.clone(), src.clone()));
    }

    for d in diags {
        let kind = match d.severity {
            Severity::Error => ReportKind::Error,
            Severity::Warning => ReportKind::Warning,
            Severity::Note => ReportKind::Advice,
        };

        // 选择用于 Report 的文件键：优先使用 span 所在文件在 SourceMap 的显示名
        let (report_file_key, loc_range): (String, Range<usize>) = if let Some(sp) = d.span {
            let (name, _src) = sm.get(sp.file).unwrap_or((d.file_id.as_str(), ""));
            (name.to_string(), sp.range())
        } else {
            (d.file_id.clone(), 0..0)
        };

        let mut builder = Report::build(kind, (report_file_key.clone(), loc_range))
            .with_code(d.code.clone());

        if let Some(sp) = d.span {
            let range: Range<usize> = sp.range();
            builder = builder.with_label(
                Label::new((report_file_key, range))
                    .with_color(Color::Red)
                    .with_message(d.message.clone()),
            );
        }

        let report = builder.finish();
        // 每次渲染构造一次 cache（Vec 克隆轻量且安全）
        let _ = report.eprint(sources(srcs.clone()));
    }
}
