use crate::ast::*;
use anyhow::{anyhow, bail, Result};
use pest::iterators::Pair;
use pest::Parser;

#[derive(pest_derive::Parser)]
#[grammar = "grammar/grammar.pest"]
pub struct PawParser;

// ------------------------
// 入口
// ------------------------

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

// ------------------------
// item / 顶层
// ------------------------

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
            let _kw = it
                .next()
                .ok_or_else(|| anyhow!("import_decl missing `import`"))?;
            let lit = it
                .next()
                .ok_or_else(|| anyhow!("import_decl missing string"))?;
            if lit.as_rule() != Rule::string_lit {
                return Err(anyhow!(
                    "import_decl: expected string_lit, got {:?}",
                    lit.as_rule()
                ));
            }
            Item::Import(unescape_string(lit.as_str()))
        }
        Rule::trait_decl => build_trait_decl(p)?,
        Rule::impl_decl => build_impl_decl(p)?,
        r => return Err(anyhow!("unexpected item: {:?}", r)),
    })
}

// ------------------------
// 函数声明 / 外部函数
// ------------------------

fn build_fun_item(p: Pair<Rule>) -> Result<Item> {
    let mut name: Option<String> = None;
    let mut type_params: Vec<String> = Vec::new();
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;
    let mut where_bounds: Vec<WherePred> = Vec::new();
    let mut body: Option<Block> = None;

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_FNs => { /* ignore */ }
            Rule::ident => name = Some(child.as_str().to_string()),
            Rule::ty_params => type_params = build_ty_params(child)?,
            Rule::param_list => params = build_params(child)?,
            Rule::ty => ret = Some(build_ty(child)?),
            Rule::where_clause => where_bounds = build_where_clause(child)?,
            Rule::block => body = Some(build_block(child)?),
            _ => { /* 忽略符号 */ }
        }
    }

    let name = name.ok_or_else(|| anyhow!("fun_decl: missing name"))?;
    let ret = ret.ok_or_else(|| anyhow!("fun_decl: missing return ty for `{}`", name))?;
    let body = body.ok_or_else(|| anyhow!("fun_decl: missing body for `{}`", name))?;

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
            Rule::KW_EXTERN | Rule::KW_FNs => { /* ignore */ }
            Rule::ident => name = Some(child.as_str().to_string()),
            Rule::param_list => params = build_params(child)?,
            Rule::ty => ret = Some(build_ty(child)?),
            _ => { /* ignore */ }
        }
    }

    let name = name.ok_or_else(|| anyhow!("extern_fun: missing name"))?;
    let ret = ret.ok_or_else(|| anyhow!("extern_fun: missing return ty for `{}`", name))?;

    Ok(Item::Fun(FunDecl {
        name,
        type_params: Vec::new(),
        params,
        ret,
        where_bounds: Vec::new(),
        body: Block { stmts: vec![], tail: None },
        is_extern: true,
    }))
}

// ------------------------
// trait / impl / where
// ------------------------

fn build_trait_decl(p: Pair<Rule>) -> Result<Item> {
    // trait_decl = { KW_TRAIT ~ ident ~ ty_params? ~ "{" ~ trait_item* ~ "}" }
    let mut name: Option<String> = None;
    let mut tparams: Vec<String> = Vec::new();
    let mut items: Vec<TraitMethodSig> = Vec::new();

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_TRAIT => {}
            Rule::ident => { if name.is_none() { name = Some(child.as_str().to_string()); } }
            Rule::ty_params => tparams = build_ty_params(child)?,
            Rule::trait_item => items.push(build_trait_item(child)?),
            _ => {}
        }
    }

    let name = name.ok_or_else(|| anyhow!("trait_decl: missing name"))?;
    Ok(Item::Trait(TraitDecl { name, type_params: tparams, items }))
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
    let ret = ret.ok_or_else(|| anyhow!("trait_item: missing return ty for `{}`", name))?;
    Ok(TraitMethodSig { name, params, ret })
}

fn build_impl_decl(p: Pair<Rule>) -> Result<Item> {
    // impl_decl = { KW_IMPL ~ ident ~ ty_args ~ "{" ~ impl_item* ~ "}" }
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
            Rule::ty_args => trait_args = build_ty_args(child)?,
            Rule::impl_item => items.push(build_impl_item(child)?),
            _ => {}
        }
    }

    let trait_name = trait_name.ok_or_else(|| anyhow!("impl_decl: missing trait name"))?;
    if trait_args.is_empty() {
        return Err(anyhow!("impl_decl: missing trait type arguments"));
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
    let ret = ret.ok_or_else(|| anyhow!("impl_item: missing return ty for `{}`", name))?;
    let body = body.ok_or_else(|| anyhow!("impl_item: missing body for `{}`", name))?;

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
    // where_pred = ty ":" bound ("+" ~ bound)*
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
    // bound = ident ("<" ty ("," ty)* ">")?
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

// ------------------------
// 参数列表 / 类型
// ------------------------

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
        // 解开一层 wrapper
        Rule::ty | Rule::ty_prim => {
            let inner = p.into_inner().next().ok_or_else(|| anyhow!("empty type node"))?;
            build_ty(inner)?
        }

        // 类型变量
        Rule::ty_var => Ty::Var(p.as_str().to_string()),

        // 泛型类型应用：ident 后面若干 ty
        Rule::ty_app => {
            let mut name: Option<String> = None;
            let mut args: Vec<Ty> = Vec::new();
            for ch in p.into_inner() {
                match ch.as_rule() {
                    Rule::ident => {
                        if name.is_none() { name = Some(ch.as_str().to_string()); }
                    }
                    Rule::ty => args.push(build_ty(ch)?),
                    _ => {}
                }
            }
            let name = name.ok_or_else(|| anyhow!("ty_app missing head ident"))?;
            Ty::App { name, args }
        }

        // 原生关键字类型
        Rule::KW_Int    => Ty::Int,
        Rule::KW_Long   => Ty::Long,
        Rule::KW_Bool   => Ty::Bool,
        Rule::KW_String => Ty::String,
        Rule::KW_Double => Ty::Double,
        Rule::KW_Float  => Ty::Float,
        Rule::KW_Char   => Ty::Char,
        Rule::KW_Void   => Ty::Void,

        // 兜底：有些场景可能出现简单 ident 当类型构造（0 实参）
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

// ------------------------
// block / stmt
// ------------------------

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
            let _kw = it.next(); // KW_WHILE
            let cond = build_expr(it.next().unwrap())?;
            let body = build_block(it.next().unwrap())?;
            Stmt::While { cond, body }
        }

        Rule::for_stmt => build_for_stmt(p)?,

        Rule::if_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next(); // KW_IF
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
    // for "(" for_init? ";" expr? ";" for_step? ")" block
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
            _ => { /* 忽略符号 */ }
        }
    }
    let body = body.ok_or_else(|| anyhow!("for_stmt: missing body block"))?;
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
        r => return Err(anyhow!("unexpected for_init node: {:?}", r)),
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
        r => return Err(anyhow!("unexpected for_step node: {:?}", r)),
    })
}

// ------------------------
// 表达式
// ------------------------

fn build_if_expr(p: Pair<Rule>) -> Result<Expr> {
    let mut it = p.into_inner();
    let _if = it.next(); // KW_IF
    let cond = build_expr(it.next().unwrap())?;
    let then_b = build_block(it.next().unwrap())?;
    let _else = it.next(); // KW_ELSE
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
                            let mut ii = a.into_inner(); // pattern, block
                            let pat = build_pattern(ii.next().unwrap())?;
                            let blk = build_block(ii.next().unwrap())?;
                            arms.push((pat, blk));
                        }
                        Rule::match_default => {
                            let mut ii = a.into_inner(); // block
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
    let scrut = scrut.ok_or_else(|| anyhow!("match: missing scrutinee expr"))?;
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
        Rule::int_lit  => parse_int_pattern(p.as_str())?,
        Rule::char_lit => Pattern::Char(parse_char_lit(p.as_str())?),
        Rule::bool_lit => Pattern::Bool(p.as_str() == "true"),
        r => return Err(anyhow!("unexpected pattern node: {:?}", r)),
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

        // 核心：安全调用后缀 + 限定名 callee
        Rule::postfix => {
            let mut it = p.into_inner();
            let head = it.next().unwrap(); // primary_head 的某个备选
            // 如果是 name_ref，提取 Callee；否则当作普通表达式
            let mut pending_callee: Option<Callee> = None;
            let mut e_opt: Option<Expr> = None;

            if head.as_rule() == Rule::name_ref {
                pending_callee = Some(build_callee_from_name_ref(head)?);
            } else {
                e_opt = Some(build_expr(head)?);
            }

            let mut did_call = false;
            for suf in it {
                match suf.as_rule() {
                    Rule::call_suffix => {
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
                                _ => {}
                            }
                        }

                        if let Some(callee) = pending_callee.take() {
                            // name_ref 后紧跟一次调用；不允许再链式追加
                            if did_call {
                                return Err(anyhow!("chained calls after qualified/name callee are not supported"));
                            }
                            did_call = true;
                            e_opt = Some(Expr::Call { callee, generics, args });
                        } else if let Some(Expr::Var(name)) = e_opt.take() {
                            // 兼容旧语义：ident(...)，按自由函数调用
                            let callee = Callee::Name(name);
                            if did_call {
                                return Err(anyhow!("chained calls after value are not supported"));
                            }
                            did_call = true;
                            e_opt = Some(Expr::Call { callee, generics, args });
                        } else {
                            return Err(anyhow!("call on non-name (only ident or Trait::method allowed)"));
                        }
                    }
                    _ => unreachable!(),
                }
            }

            // 没有任何调用后缀：
            // - 如果是 ident：作为变量名返回
            // - 如果是限定名（Trait::method）：不允许裸出现
            if let Some(callee) = pending_callee {
                match callee {
                    Callee::Name(n) => Expr::Var(n),
                    Callee::Qualified { .. } => {
                        return Err(anyhow!("bare qualified name like `Trait::method` is not an expression; call it with `(...)`"))
                    }
                }
            } else {
                e_opt.expect("postfix head should produce an expression")
            }
        }

        Rule::group   => build_expr(p.into_inner().next().unwrap())?,
        Rule::if_expr => build_if_expr(p)?,
        Rule::match_expr => build_match_expr(p)?,

        // 字面量
        Rule::int_lit   => parse_int_expr(p.as_str())?,
        Rule::long_lit  => Expr::Long(parse_long_lit(p.as_str())?),
        Rule::float_lit => Expr::Double(parse_float_lit(p.as_str())?),
        Rule::char_lit  => Expr::Char(parse_char_lit(p.as_str())?),
        Rule::bool_lit  => Expr::Bool(p.as_str() == "true"),
        Rule::string_lit=> Expr::Str(unescape_string(p.as_str())),

        // 其它原子（出现在非 name_ref 的场合）
        Rule::ident => Expr::Var(p.as_str().to_string()),
        Rule::block => Expr::Block(build_block(p)?),

        r => {
            let mut it = p.into_inner();
            if let Some(q) = it.next() {
                build_expr(q)?
            } else {
                return Err(anyhow!("unexpected expr rule: {:?}", r));
            }
        },
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
            Rule::OP_LT  => Lt,
            Rule::OP_LE  => Le,
            Rule::OP_GT  => Gt,
            Rule::OP_GE  => Ge,
            Rule::OP_EQ  => Eq,
            Rule::OP_NE  => Ne,
            Rule::OP_AND => And,
            Rule::OP_OR  => Or,
            r => return Err(anyhow!("binary op {:?} not expected", r)),
        };
        lhs = Expr::Binary { op: bop, lhs: Box::new(lhs), rhs: Box::new(rhs) };
    }
    Ok(lhs)
}

// ------------------------
// name_ref / callee
// ------------------------

fn build_callee_from_name_ref(p: Pair<Rule>) -> Result<Callee> {
    debug_assert_eq!(p.as_rule(), Rule::name_ref);
    // name_ref = qident | ident
    let inner = p.into_inner().next().ok_or_else(|| anyhow!("empty name_ref"))?;
    match inner.as_rule() {
        Rule::ident => Ok(Callee::Name(inner.as_str().to_string())),
        Rule::qident => {
            // qident 是原子 (@)，没有子节点；用字符串按 "::" 拆
            let s = inner.as_str();
            let parts: Vec<&str> = s.split("::").collect();
            if parts.len() != 2 {
                return Err(anyhow!("qualified name must be `Trait::method`, got `{}`", s));
            }
            Ok(Callee::Qualified {
                trait_name: parts[0].to_string(),
                method: parts[1].to_string(),
            })
        }
        _ => Err(anyhow!("invalid name_ref inner {:?}", inner.as_rule())),
    }
}

// ------------------------
// 辅助：字面量解析
// ------------------------

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

fn parse_float_lit(s: &str) -> Result<f64> {
    let v = s.parse::<f64>()?;
    Ok(v)
}

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
        if c == '"' {
            break;
        }
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
