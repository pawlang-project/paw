use anyhow::{anyhow, bail, Result};
use cranelift_codegen::{
    ir::{self, condcodes::{FloatCC, IntCC}, types, AbiParam, InstBuilder},
    settings,
};
use cranelift_codegen::settings::{Configurable, Flags};
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift_module::{default_libcall_names, DataDescription as DataContext, DataId, FuncId, Linkage, Module};
use cranelift_native;
use cranelift_object::{ObjectBuilder, ObjectModule};

use std::cell::RefCell;
use std::rc::Rc;

use crate::backend::mangle::{mangle_impl_method, mangle_name};
use crate::frontend::ast::*;
use crate::frontend::span::Span;
use crate::utils::fast::{FastMap, FastSet};
use crate::DiagSink;

// 顺序很重要：类型→ABI→单态化工具→上下文→声明→lower→emit
include!("abi.rs");
include!("mono.rs");     // 用于 mangle/单态化工具
include!("context.rs");  // 结构体/构造/诊断（依赖 mono.rs 的 ImplMethodTpl? 已在此文件内定义）
include!("data.rs");     // 常量池/全局常量注入（impl CLBackend）
include!("declare.rs");  // 声明阶段（依赖 abi.rs/mangle）
include!("lower/block.rs");
include!("lower/expr.rs");
include!("emit.rs");