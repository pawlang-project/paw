//! 源位置信息（供 ariadne/DiagSink 使用）

use std::ops::Range;

/// 逻辑文件 ID（与 DiagSink / ariadne 的文件缓存一一对应）
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct FileId(pub usize);

impl FileId {
    pub const DUMMY: FileId = FileId(usize::MAX);
}

/// 半开区间 [start, end)，单位为字节偏移
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Span {
    pub file: FileId,
    pub start: usize,
    pub end: usize,
}

impl Span {
    pub const DUMMY: Span = Span { file: FileId::DUMMY, start: 0, end: 0 };

    #[inline]
    pub fn range(&self) -> Range<usize> { self.start..self.end }

    #[inline]
    pub fn merge(a: Span, b: Span) -> Span {
        debug_assert_eq!(a.file, b.file, "cannot merge spans across files");
        Span { file: a.file, start: a.start.min(b.start), end: a.end.max(b.end) }
    }
}

/// 可选的小工具包装（当前 AST 未使用；保留以便后续渐进式迁移）
/// 例如：`Sp { node: Expr::Int{..}, span }`
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Sp<T> {
    pub node: T,
    pub span: Span,
}

#[inline]
pub fn sp<T>(node: T, span: Span) -> Sp<T> { Sp { node, span } }
