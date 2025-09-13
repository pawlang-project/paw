use crate::ast::*;
use anyhow::{anyhow, bail, Result};
use pest::iterators::Pair;
use pest::Parser;

#[derive(pest_derive::Parser)]
#[grammar = "grammar/grammar.pest"] // 若文件在 grammar/ 目录，请改为 "grammar/grammar.pest"
pub struct PawParser;

/* ================================
 * 入口
 * ================================ */
pub fn parse_program(src: &str) -> Result<Program> {
    let pairs = PawParser::parse(Rule::program, src)?;
    let mut items = Vec::new();
    for p in pairs {
        if p.as_rule() == Rule::program {
            for it in p.into_inner() {
                if it.as_rule() == Rule::item {
                    items.push(build_item(it.into_inner().next().unwrap())?);
                }
            }
        }
    }
    Ok(Program { items })
}

/* ================================
 * item
 * ================================ */
fn build_item(p: Pair<Rule>) -> Result<Item> {
    Ok(match p.as_rule() {
        Rule::let_decl => {
            let mut it = p.into_inner();
            let kw = it.next().unwrap(); // KW_LET | KW_CONST
            let name = it.next().unwrap().as_str().to_string();
            let ty = build_ty(it.next().unwrap())?;
            let init = build_expr(it.next().unwrap())?;
            Item::Global {
                name,
                ty,
                init,
                is_const: kw.as_rule() == Rule::KW_CONST,
            }
        }
        Rule::fun_decl => build_fun_item(p)?,
        Rule::extern_fun => build_extern_fun_item(p)?,
        Rule::import_decl => {
            let mut it = p.into_inner();
            let _kw = it.next().ok_or_else(|| anyhow!("import_decl missing `import`"))?;
            let lit = it.next().ok_or_else(|| anyhow!("import_decl missing string"))?;
            if lit.as_rule() != Rule::string_lit {
                bail!("import_decl: expect string_lit");
            }
            Item::Import(unescape_string(lit.as_str()))
        }
        Rule::trait_decl => build_trait_decl(p)?,
        Rule::impl_decl => build_impl_decl(p)?,
        other => return Err(anyhow!("unexpected item: {:?}", other)),
    })
}

/* ================================
 * fun / extern
 * ================================ */
fn build_fun_item(p: Pair<Rule>) -> Result<Item> {
    let mut name: Option<String> = None;
    let mut type_params: Vec<String> = Vec::new();
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;
    let mut where_bounds: Vec<WherePred> = Vec::new();
    let mut body: Option<Block> = None;

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_FNs => {}
            Rule::ident => name = Some(child.as_str().to_string()),
            Rule::ty_params => type_params = build_ty_params(child)?,
            Rule::param_list => params = build_params(child)?,
            Rule::ty => ret = Some(build_ty(child)?),
            Rule::where_clause => where_bounds = build_where_clause(child)?,
            Rule::block => body = Some(build_block(child)?),
            _ => {}
        }
    }

    let name = name.ok_or_else(|| anyhow!("fun_decl: missing name"))?;
    let ret = ret.ok_or_else(|| anyhow!("fun_decl `{}` missing return type", &name))?;
    let body = body.ok_or_else(|| anyhow!("fun_decl `{}` missing body", &name))?;

    Ok(Item::Fun(FunDecl {
        name,
        type_params,
        params,
        ret,
        where_bounds,
        body,
        is_extern: false,
    }))
}

fn build_extern_fun_item(p: Pair<Rule>) -> Result<Item> {
    let mut name: Option<String> = None;
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_EXTERN | Rule::KW_FNs => {}
            Rule::ident => name = Some(child.as_str().to_string()),
            Rule::param_list => params = build_params(child)?,
            Rule::ty => ret = Some(build_ty(child)?),
            _ => {}
        }
    }

    let name = name.ok_or_else(|| anyhow!("extern_fun: missing name"))?;
    let ret = ret.ok_or_else(|| anyhow!("extern_fun `{}` missing return type", &name))?;

    Ok(Item::Fun(FunDecl {
        name,
        type_params: vec![],
        params,
        ret,
        where_bounds: vec![],
        body: Block { stmts: vec![], tail: None },
        is_extern: true,
    }))
}

/* ================================
 * trait / impl / where
 * ================================ */
fn build_trait_decl(p: Pair<Rule>) -> Result<Item> {
    let mut name: Option<String> = None;
    let mut type_params: Vec<String> = Vec::new();
    let mut items: Vec<TraitMethodSig> = Vec::new();

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_TRAIT => {}
            Rule::ident => {
                if name.is_none() {
                    name = Some(child.as_str().to_string());
                }
            }
            // 关键：从 ty_params 里枚举 ty_ident
            Rule::ty_params => {
                for id in child.into_inner() {
                    if id.as_rule() == Rule::ty_ident {
                        type_params.push(id.as_str().to_string());
                    }
                }
            }
            Rule::trait_item => items.push(build_trait_item(child)?),
            _ => {}
        }
    }

    let name = name.ok_or_else(|| anyhow!("trait_decl: missing name"))?;
    Ok(Item::Trait(TraitDecl { name, type_params, items }))
}


fn build_trait_item(p: Pair<Rule>) -> Result<TraitMethodSig> {
    let mut name: Option<String> = None;
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_FNs => {}
            Rule::ident => name = Some(child.as_str().to_string()),
            Rule::param_list => params = build_params(child)?,
            Rule::ty => ret = Some(build_ty(child)?),
            _ => {}
        }
    }

    let name = name.ok_or_else(|| anyhow!("trait_item: missing name"))?;
    let ret = ret.ok_or_else(|| anyhow!("trait_item `{}` missing return type", &name))?;
    Ok(TraitMethodSig { name, params, ret })
}

fn build_impl_decl(p: Pair<Rule>) -> Result<Item> {
    let mut trait_name: Option<String> = None;
    let mut trait_args: Vec<Ty> = Vec::new();
    let mut items: Vec<ImplMethod> = Vec::new();

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_IMPL => {}
            Rule::ident => {
                if trait_name.is_none() {
                    trait_name = Some(child.as_str().to_string());
                }
            }
            // 关键：从 ty_args 里把所有 ty 都取出来
            Rule::ty_args => {
                for t in child.into_inner() {
                    if t.as_rule() == Rule::ty {
                        trait_args.push(build_ty(t)?);
                    }
                }
            }
            Rule::impl_item => items.push(build_impl_item(child)?),
            _ => {}
        }
    }

    let trait_name = trait_name.ok_or_else(|| anyhow!("impl_decl: missing trait name"))?;
    if trait_args.is_empty() {
        // 之前报错就是走到这里
        bail!("impl `{}` missing type arguments", trait_name);
    }
    Ok(Item::Impl(ImplDecl { trait_name, trait_args, items }))
}

fn build_impl_item(p: Pair<Rule>) -> Result<ImplMethod> {
    let mut name: Option<String> = None;
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;
    let mut body: Option<Block> = None;

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_FNs => {}
            Rule::ident => name = Some(child.as_str().to_string()),
            Rule::param_list => params = build_params(child)?,
            Rule::ty => ret = Some(build_ty(child)?),
            Rule::block => body = Some(build_block(child)?),
            _ => {}
        }
    }

    let name = name.ok_or_else(|| anyhow!("impl_item: missing name"))?;
    let ret = ret.ok_or_else(|| anyhow!("impl_item `{}` missing return type", &name))?;
    let body = body.ok_or_else(|| anyhow!("impl_item `{}` missing body", &name))?;
    Ok(ImplMethod { name, params, ret, body })
}

fn build_where_clause(p: Pair<Rule>) -> Result<Vec<WherePred>> {
    let mut preds = Vec::new();
    for child in p.into_inner() {
        if child.as_rule() == Rule::where_pred {
            preds.push(build_where_pred(child)?);
        }
    }
    Ok(preds)
}

fn build_where_pred(p: Pair<Rule>) -> Result<WherePred> {
    let mut it = p.into_inner();
    let ty = build_ty(it.next().ok_or_else(|| anyhow!("where_pred: missing ty"))?)?;
    let mut bounds = Vec::new();
    for b in it {
        if b.as_rule() == Rule::bound {
            bounds.push(build_bound_as_traitref(b)?);
        }
    }
    Ok(WherePred { ty, bounds })
}

fn build_bound_as_traitref(p: Pair<Rule>) -> Result<TraitRef> {
    let mut it = p.into_inner();
    let name = it
        .next()
        .ok_or_else(|| anyhow!("bound: missing ident"))?
        .as_str()
        .to_string();
    let mut args = Vec::new();
    for t in it {
        if t.as_rule() == Rule::ty {
            args.push(build_ty(t)?);
        }
    }
    Ok(TraitRef { name, args })
}

/* ================================
 * 参数与类型
 * ================================ */
fn build_params(p: Pair<Rule>) -> Result<Vec<(String, Ty)>> {
    let mut v = Vec::new();
    for x in p.into_inner() {
        let mut it = x.into_inner(); // ident ":" ty
        let name = it.next().unwrap().as_str().to_string();
        let ty = build_ty(it.next().unwrap())?;
        v.push((name, ty));
    }
    Ok(v)
}

fn build_ty_params(p: Pair<Rule>) -> Result<Vec<String>> {
    let mut v = Vec::new();
    for x in p.into_inner() {
        if x.as_rule() == Rule::ty_ident {
            v.push(x.as_str().to_string());
        }
    }
    Ok(v)
}

fn build_ty_args(p: Pair<Rule>) -> Result<Vec<Ty>> {
    let mut v = Vec::new();
    for x in p.into_inner() {
        if x.as_rule() == Rule::ty {
            v.push(build_ty(x)?);
        }
    }
    Ok(v)
}

fn build_ty(p: Pair<Rule>) -> Result<Ty> {
    Ok(match p.as_rule() {
        Rule::ty | Rule::ty_prim => {
            let inner = p.into_inner().next().ok_or_else(|| anyhow!("empty type node"))?;
            build_ty(inner)?
        }
        Rule::ty_var => Ty::Var(p.as_str().to_string()),
        Rule::ty_app => {
            let mut it = p.into_inner();
            let head = it.next().ok_or_else(|| anyhow!("ty_app: missing head"))?;
            let name = match head.as_rule() {
                Rule::ident => head.as_str().to_string(),
                other => return Err(anyhow!("ty_app head not ident: {:?}", other)),
            };
            let mut args = Vec::<Ty>::new();
            if let Some(list) = it.next() {
                for a in list.into_inner() {
                    args.push(build_ty(a)?);
                }
            }
            Ty::App { name, args }
        }
        Rule::KW_Int    => Ty::Int,
        Rule::KW_Long   => Ty::Long,
        Rule::KW_Bool   => Ty::Bool,
        Rule::KW_String => Ty::String,
        Rule::KW_Double => Ty::Double,
        Rule::KW_Float  => Ty::Float,
        Rule::KW_Char   => Ty::Char,
        Rule::KW_Void   => Ty::Void,
        Rule::ident => Ty::App { name: p.as_str().to_string(), args: vec![] },
        other => {
            let mut it = p.into_inner();
            if let Some(q) = it.next() {
                build_ty(q)?
            } else {
                return Err(anyhow!("unexpected ty rule: {:?}", other));
            }
        }
    })
}

/* ================================
 * block / stmt
 * ================================ */
fn build_block(p: Pair<Rule>) -> Result<Block> {
    let mut stmts = Vec::new();
    let mut tail = None;
    for x in p.into_inner() {
        match x.as_rule() {
            Rule::stmt => stmts.push(build_stmt(x.into_inner().next().unwrap())?),
            Rule::tail_expr => {
                let e = build_expr(x.into_inner().next().unwrap())?;
                tail = Some(Box::new(e));
            }
            _ => {}
        }
    }
    Ok(Block { stmts, tail })
}

fn build_stmt(p: Pair<Rule>) -> Result<Stmt> {
    Ok(match p.as_rule() {
        Rule::let_decl => {
            let mut it = p.into_inner();
            let kw = it.next().unwrap();
            let name = it.next().unwrap().as_str().to_string();
            let ty = build_ty(it.next().unwrap())?;
            let init = build_expr(it.next().unwrap())?;
            Stmt::Let {
                name,
                ty,
                init,
                is_const: kw.as_rule() == Rule::KW_CONST,
            }
        }
        Rule::assign_stmt => {
            let mut it = p.into_inner();
            let name = it.next().unwrap().as_str().to_string();
            let expr = build_expr(it.next().unwrap())?;
            Stmt::Assign { name, expr }
        }
        Rule::while_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next();
            let cond = build_expr(it.next().unwrap())?;
            let body = build_block(it.next().unwrap())?;
            Stmt::While { cond, body }
        }
        Rule::for_stmt => build_for_stmt(p)?,
        Rule::if_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next();
            let cond = build_expr(it.next().unwrap())?;
            let then_b = build_block(it.next().unwrap())?;
            let else_b = it.next().map(build_block).transpose()?;
            Stmt::If { cond, then_b, else_b }
        }
        Rule::break_stmt => Stmt::Break,
        Rule::continue_stmt => Stmt::Continue,
        Rule::return_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next();
            let expr = it.next().map(build_expr).transpose()?;
            Stmt::Return(expr)
        }
        Rule::expr_stmt => {
            let e = build_expr(p.into_inner().next().unwrap())?;
            Stmt::Expr(e)
        }
        r => return Err(anyhow!("build_stmt: unexpected rule {:?}", r)),
    })
}

fn build_for_stmt(p: Pair<Rule>) -> Result<Stmt> {
    let mut init: Option<ForInit> = None;
    let mut cond: Option<Expr> = None;
    let mut step: Option<ForStep> = None;
    let mut body: Option<Block> = None;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::for_init => init = Some(build_for_init(x)?),
            Rule::for_step => step = Some(build_for_step(x)?),
            Rule::expr => cond = Some(build_expr(x)?),
            Rule::block => body = Some(build_block(x)?),
            _ => {}
        }
    }
    let body = body.ok_or_else(|| anyhow!("for_stmt: missing body"))?;
    Ok(Stmt::For { init, cond, step, body })
}

fn build_for_init(p: Pair<Rule>) -> Result<ForInit> {
    let mut it = p.into_inner();
    let first = it.next().unwrap();
    Ok(match first.as_rule() {
        Rule::KW_LET | Rule::KW_CONST => {
            let is_const = first.as_rule() == Rule::KW_CONST;
            let name = it.next().unwrap().as_str().to_string();
            let ty = build_ty(it.next().unwrap())?;
            let init = build_expr(it.next().unwrap())?;
            ForInit::Let { name, ty, init, is_const }
        }
        Rule::ident => {
            let name = first.as_str().to_string();
            let expr = build_expr(it.next().unwrap())?;
            ForInit::Assign { name, expr }
        }
        Rule::expr => ForInit::Expr(build_expr(first)?),
        r => return Err(anyhow!("unexpected for_init: {:?}", r)),
    })
}

fn build_for_step(p: Pair<Rule>) -> Result<ForStep> {
    let mut it = p.into_inner();
    let first = it.next().unwrap();
    Ok(match first.as_rule() {
        Rule::ident => {
            let name = first.as_str().to_string();
            let expr = build_expr(it.next().unwrap())?;
            ForStep::Assign { name, expr }
        }
        Rule::expr => ForStep::Expr(build_expr(first)?),
        r => return Err(anyhow!("unexpected for_step: {:?}", r)),
    })
}

/* ================================
 * 表达式（含限定名调用）
 * ================================ */

fn build_if_expr(p: Pair<Rule>) -> Result<Expr> {
    let mut it = p.into_inner();
    let _if = it.next();
    let cond = build_expr(it.next().unwrap())?;
    let then_b = build_block(it.next().unwrap())?;
    let _else = it.next();
    let else_b = build_block(it.next().unwrap())?;
    Ok(Expr::If { cond: Box::new(cond), then_b, else_b })
}

fn build_match_expr(p: Pair<Rule>) -> Result<Expr> {
    let mut scrut: Option<Expr> = None;
    let mut arms: Vec<(Pattern, Block)> = Vec::new();
    let mut default: Option<Block> = None;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::expr => scrut = Some(build_expr(x)?),
            Rule::match_arms => {
                for a in x.into_inner() {
                    match a.as_rule() {
                        Rule::match_arm => {
                            let mut ii = a.into_inner();
                            let pat = build_pattern(ii.next().unwrap())?;
                            let blk = build_block(ii.next().unwrap())?;
                            arms.push((pat, blk));
                        }
                        Rule::match_default => {
                            let mut ii = a.into_inner();
                            let blk = build_block(ii.next().unwrap())?;
                            default = Some(blk);
                        }
                        _ => {}
                    }
                }
            }
            _ => {}
        }
    }
    let scrut = scrut.ok_or_else(|| anyhow!("match: missing scrutinee"))?;
    Ok(Expr::Match { scrut: Box::new(scrut), arms, default })
}

fn build_pattern(p: Pair<Rule>) -> Result<Pattern> {
    Ok(match p.as_rule() {
        Rule::pattern => {
            let mut inner = p.into_inner();
            if let Some(q) = inner.next() {
                build_pattern(q)?
            } else {
                Pattern::Wild
            }
        }
        Rule::long_lit => Pattern::Long(parse_long_lit(p.as_str())?),
        Rule::int_lit => parse_int_pattern(p.as_str())?,
        Rule::char_lit => Pattern::Char(parse_char_lit(p.as_str())?),
        Rule::bool_lit => Pattern::Bool(p.as_str() == "true"),
        r => return Err(anyhow!("unexpected pattern: {:?}", r)),
    })
}

fn build_expr(p: Pair<Rule>) -> Result<Expr> {
    Ok(match p.as_rule() {
        Rule::expr => build_expr(p.into_inner().next().unwrap())?,

        Rule::logic_or | Rule::logic_and | Rule::equality | Rule::compare | Rule::add | Rule::mult => {
            fold_binary(p)?
        }

        Rule::unary => {
            let mut it = p.into_inner();
            let first = it.next().unwrap();
            match first.as_rule() {
                Rule::OP_NOT => Expr::Unary { op: UnOp::Not, rhs: Box::new(build_expr(it.next().unwrap())?) },
                Rule::OP_SUB => Expr::Unary { op: UnOp::Neg, rhs: Box::new(build_expr(it.next().unwrap())?) },
                _ => build_expr(first)?,
            }
        }

        // 核心：后缀（函数调用），支持 qident（Trait::method）
        Rule::postfix => {
            let mut it = p.into_inner();

            // 读取头
            let head = it.next().unwrap();
            let mut callee_opt: Option<Callee> = None;
            let mut base_expr: Option<Expr> = None;

            match head.as_rule() {
                // name_ref 统一了 ident 和 qident
                Rule::name_ref => {
                    let node = head.into_inner().next().unwrap();
                    match node.as_rule() {
                        Rule::ident => {
                            // 普通标识符：既可以后接调用，也可以单独作为变量引用
                            callee_opt = Some(Callee::Name(node.as_str().to_string()));
                        }
                        Rule::qident => {
                            // 限定名（Trait::method）只允许出现在调用位置
                            // 如果后面没有 call_suffix，会在下面做专门的报错
                            let s = node.as_str(); // e.g. "Eq::eq"
                            let mut parts = s.split("::");
                            let trait_name = parts.next().unwrap_or("").to_string();
                            let method = parts.last().unwrap_or("").to_string();
                            callee_opt = Some(Callee::Qualified { trait_name, method });
                        }
                        other => return Err(anyhow!("name_ref unexpected: {:?}", other)),
                    }
                }
                // 其他原子表达式（group/block/字面量/又一个 postfix 等）
                _ => {
                    base_expr = Some(build_expr(head)?);
                }
            }

            // 依次消费零个或多个 call_suffix
            let mut out_call: Option<Expr> = None;
            for suf in it {
                if suf.as_rule() != Rule::call_suffix {
                    return Err(anyhow!("postfix: unexpected suffix {:?}", suf.as_rule()));
                }
                // call_suffix = ty_args? "(" arg_list? ")"
                let mut generics: Vec<Ty> = Vec::new();
                let mut args: Vec<Expr> = Vec::new();

                let mut ii = suf.into_inner();
                if let Some(first) = ii.next() {
                    match first.as_rule() {
                        Rule::ty_args => {
                            generics = build_ty_args(first)?;
                            if let Some(maybe_args) = ii.next() {
                                if maybe_args.as_rule() == Rule::arg_list {
                                    args = build_arg_list(maybe_args)?;
                                }
                            }
                        }
                        Rule::arg_list => {
                            args = build_arg_list(first)?;
                        }
                        r => return Err(anyhow!("call_suffix: unexpected {:?}", r)),
                    }
                }

                // 组装本次“调用”表达式
                let callee = if let Some(c) = &callee_opt {
                    c.clone()
                } else {
                    // 不是 name_ref 开头：只允许之前的 head 被解析为 Var(name)
                    match base_expr.take() {
                        Some(Expr::Var(name)) => Callee::Name(name),
                        _ => return Err(anyhow!("call on non-ident expression")),
                    }
                };

                out_call = Some(Expr::Call { callee, generics, args });
                // 一旦形成调用，后续再接的后缀都必须基于这次调用（不再允许把上一个结果当“函数名”）
                callee_opt = None;
            }

            // —— 无任何 call_suffix 的情况：把“头部”直接作为表达式返回 —— //
            if let Some(e) = out_call {
                e
            } else if let Some(c) = callee_opt {
                match c {
                    Callee::Name(n) => Expr::Var(n), // 变量引用（例如 `x`）
                    Callee::Qualified { .. } => {
                        return Err(anyhow!(
                    "qualified path cannot be used as a value; call it like `Trait::method<...>(...)`"
                ));
                    }
                }
            } else if let Some(be) = base_expr {
                be // 比如 `(a+b)` / `{ ... }` / 字面量 等
            } else {
                return Err(anyhow!("postfix head missing"));
            }
        }

        // 其它原子
        Rule::group      => build_expr(p.into_inner().next().unwrap())?,
        Rule::if_expr    => build_if_expr(p)?,
        Rule::match_expr => build_match_expr(p)?,
        Rule::int_lit    => parse_int_expr(p.as_str())?,
        Rule::long_lit   => { let n: i64 = parse_long_lit(p.as_str())?; Expr::Long(n) }
        Rule::float_lit  => Expr::Double(parse_float_lit(p.as_str())?),
        Rule::char_lit   => Expr::Char(parse_char_lit(p.as_str())?),
        Rule::bool_lit   => Expr::Bool(p.as_str() == "true"),
        Rule::string_lit => Expr::Str(unescape_string(p.as_str())),
        Rule::ident      => Expr::Var(p.as_str().to_string()),
        Rule::block      => Expr::Block(build_block(p)?),

        // 包装层兜底：继续向里钻
        other => {
            let mut it = p.clone().into_inner();
            if let Some(q) = it.next() {
                build_expr(q)?
            } else {
                return Err(anyhow!("unexpected expr rule: {:?}", other));
            }
        }
    })
}

fn build_arg_list(p: Pair<Rule>) -> Result<Vec<Expr>> {
    let mut args = Vec::new();
    for ae in p.into_inner() {
        args.push(build_expr(ae)?);
    }
    Ok(args)
}

fn fold_binary(p: Pair<Rule>) -> Result<Expr> {
    use BinOp::*;
    let mut it = p.into_inner();
    let mut lhs = build_expr(it.next().unwrap())?;
    while let Some(op) = it.next() {
        let rhs = build_expr(it.next().unwrap())?;
        let bop = match op.as_rule() {
            Rule::OP_ADD => Add,
            Rule::OP_SUB => Sub,
            Rule::OP_MUL => Mul,
            Rule::OP_DIV => Div,
            Rule::OP_LT => Lt,
            Rule::OP_LE => Le,
            Rule::OP_GT => Gt,
            Rule::OP_GE => Ge,
            Rule::OP_EQ => Eq,
            Rule::OP_NE => Ne,
            Rule::OP_AND => And,
            Rule::OP_OR => Or,
            r => return Err(anyhow!("binary op {:?} not expected", r)),
        };
        lhs = Expr::Binary { op: bop, lhs: Box::new(lhs), rhs: Box::new(rhs) };
    }
    Ok(lhs)
}

/* ================================
 * 字面量解析
 * ================================ */
fn parse_int_expr(s: &str) -> Result<Expr> {
    let v = s.parse::<i128>()?;
    if v >= i32::MIN as i128 && v <= i32::MAX as i128 {
        Ok(Expr::Int(v as i32))
    } else if v >= i64::MIN as i128 && v <= i64::MAX as i128 {
        Ok(Expr::Long(v as i64))
    } else {
        bail!("integer literal out of i64 range: {}", s);
    }
}

fn parse_int_pattern(s: &str) -> Result<Pattern> {
    let v = s.parse::<i128>()?;
    if v >= i32::MIN as i128 && v <= i32::MAX as i128 {
        Ok(Pattern::Int(v as i32))
    } else if v >= i64::MIN as i128 && v <= i64::MAX as i128 {
        Ok(Pattern::Long(v as i64))
    } else {
        bail!("pattern integer out of i64 range: {}", s);
    }
}

fn parse_float_lit(s: &str) -> Result<f64> { Ok(s.parse::<f64>()?) }

fn parse_long_lit(s: &str) -> Result<i64> {
    let ns = s.strip_suffix('L')
        .or_else(|| s.strip_suffix('l'))
        .ok_or_else(|| anyhow!("invalid long literal suffix: {}", s))?;
    Ok(ns.parse::<i64>()?)
}

fn parse_char_lit(s: &str) -> Result<u32> {
    let bytes = s.as_bytes();
    if bytes.len() < 3 || bytes[0] != b'\'' || bytes[bytes.len() - 1] != b'\'' {
        bail!("invalid char literal: {}", s);
    }
    let inner = &s[1..s.len() - 1];
    let ch = if inner.starts_with('\\') {
        let rest = &inner[1..];
        match rest {
            "'" => '\'',
            "\\" => '\\',
            "n" => '\n',
            "r" => '\r',
            "t" => '\t',
            "0" => '\0',
            _ => {
                if let Some(hex) = rest.strip_prefix("u{").and_then(|t| t.strip_suffix('}')) {
                    let v = u32::from_str_radix(hex, 16)
                        .map_err(|_| anyhow!("invalid unicode escape in char: {}", s))?;
                    char::from_u32(v).unwrap_or('\u{FFFD}')
                } else {
                    rest.chars().next().unwrap_or('\u{FFFD}')
                }
            }
        }
    } else {
        inner.chars().next().ok_or_else(|| anyhow!("empty char"))?
    };
    Ok(ch as u32)
}

fn unescape_string(s: &str) -> String {
    let bytes = s.as_bytes();
    let mut out = String::new();
    let mut i = 1; // skip opening "
    while i + 1 < bytes.len() {
        let c = bytes[i] as char;
        if c == '"' { break; }
        if c == '\\' {
            i += 1;
            let e = bytes[i] as char;
            match e {
                'n' => out.push('\n'),
                't' => out.push('\t'),
                '\\' => out.push('\\'),
                '"' => out.push('"'),
                'r' => out.push('\r'),
                '0' => out.push('\0'),
                _ => out.push(e),
            }
        } else {
            out.push(c);
        }
        i += 1;
    }
    out
}
