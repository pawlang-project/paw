// src/diag.rs
use std::fmt;

// 统一复用 frontend 的 Span / FileId，并沿用 crate::diag::Span 的访问路径
pub use crate::frontend::span::{FileId, Span};

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
