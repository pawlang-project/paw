use anyhow::{anyhow, bail, Result};
use std::collections::HashMap;
use std::fmt;

use crate::backend::mangle::mangle_impl_method;
use crate::diag::DiagSink;
use crate::frontend::ast::*;
use crate::frontend::span::Span;
use crate::utils::fast::{FastMap, FastSet, SmallVec4};

/* 宏：需置顶，供各分片使用 */
macro_rules! tc_bail { ($self:ident, $code:expr, $span:expr, $($arg:tt)*) => {{
    $self.diags.error($code, $self.file_id, $span, format!($($arg)*));
    bail!(format!($($arg)*))
}}}
macro_rules! tc_err { ($self:ident, $code:expr, $span:expr, $($arg:tt)*) => {{
    $self.diags.error($code, $self.file_id, $span, format!($($arg)*));
}}}

include!("env.rs");
include!("scheme.rs");
include!("unify.rs");
include!("check.rs");
include!("resolve.rs");
include!("infer_expr.rs");
include!("literals.rs");
include!("util.rs");