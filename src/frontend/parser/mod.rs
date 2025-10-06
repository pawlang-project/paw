use crate::frontend::ast::*;
use crate::frontend::span::{FileId, Span};
use crate::diag::DiagSink;
use anyhow::{anyhow, bail, Result};
use pest::iterators::Pair;
use pest::Parser;

include!("program.rs");
include!("item.rs");
include!("stmt.rs");
include!("expr.rs");
include!("pattern.rs");
include!("types.rs");
include!("util.rs");

#[derive(pest_derive::Parser)]
#[grammar = "./grammar/grammar.pest"]
pub struct PawParser;

/// 带诊断写入的解析入口：语法错误会写入 DiagSink，并返回 Err
pub fn parse_program_with_diags(src: &str, file: FileId, file_display: &str, diags: &mut DiagSink) -> Result<Program> {
    match PawParser::parse(Rule::program, src) {
        Ok(mut pairs) => {
            let root = pairs.next().ok_or_else(|| anyhow!("empty program"))?;
            debug_assert_eq!(root.as_rule(), Rule::program);
            let mut items = Vec::new();
            for it in root.into_inner() {
                match it.as_rule() {
                    Rule::item => {
                        let inner = it.into_inner().next().ok_or_else(|| anyhow!("empty item"))?;
                        items.push(build_item(inner, file)?);
                    }
                    Rule::EOI => {}
                    other => return Err(anyhow!("program: unexpected node: {:?}", other)),
                }
            }
            Ok(Program { items })
        }
        Err(e) => {
            use pest::error::{InputLocation, ErrorVariant};
            // 将 pest 错误定位转换为 Span（字节偏移）
            let (start, end) = match e.location {
                InputLocation::Pos(p) => (p, p + 1),
                InputLocation::Span((s, t)) => (s, t),
            };
            let span = Span { file, start, end };
            // 提取期望项，构造简洁原因
            let msg = match &e.variant {
                ErrorVariant::ParsingError { positives, .. } => {
                    let expect = positives
                        .iter()
                        .map(|r| format!("{r:?}"))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("expected {expect}")
                }
                ErrorVariant::CustomError { message } => {
                    format!("{message}")
                }
            };
            diags.error("P0001", file_display, Some(span), msg);
            Err(anyhow!(e))
        }
    }
}