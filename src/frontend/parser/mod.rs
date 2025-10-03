use crate::frontend::ast::*;
use crate::frontend::span::{FileId, Span};
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