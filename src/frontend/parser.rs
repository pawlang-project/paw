use crate::frontend::ast::*;
use anyhow::{anyhow, bail, Result};
use pest::iterators::Pair;
use pest::Parser;

#[derive(pest_derive::Parser)]
#[grammar = "./grammar/grammar.pest"]
pub struct PawParser;

/* ================================
 * program / item 顶层入口
 * ================================ */
pub fn parse_program(src: &str) -> Result<Program> {
    let mut items = Vec::new();
    let mut pairs = PawParser::parse(Rule::program, src)?;
    let root = pairs.next().ok_or_else(|| anyhow!("empty program"))?;
    debug_assert_eq!(root.as_rule(), Rule::program);

    for it in root.into_inner() {
        match it.as_rule() {
            Rule::item => {
                let inner = it.into_inner().next().ok_or_else(|| anyhow!("empty item"))?;
                items.push(build_item(inner)?);
            }
            Rule::EOI => { /* ignore */ }
            other => return Err(anyhow!("program: unexpected node: {:?}", other)),
        }
    }
    Ok(Program { items })
}

fn build_item(p: Pair<Rule>) -> Result<Item> {
    match p.as_rule() {
        Rule::fun_decl    => build_fun_item(p),
        Rule::extern_fun  => build_extern_fun_item(p),
        Rule::import_decl => build_import_item(p),
        Rule::let_decl    => build_global_item(p),
        Rule::trait_decl  => build_trait_decl(p).map(Item::Trait),
        Rule::impl_decl   => build_impl_decl(p).map(Item::Impl),
        other => Err(anyhow!("item: unsupported top-level rule: {:?}", other)),
    }
}

/* ================================
 * fun_decl / extern_fun / import / global
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
            Rule::KW_FN => {}
            Rule::ident        => name = Some(child.as_str().to_string()),
            Rule::ty_params    => type_params = build_ty_params(child)?,
            Rule::param_list   => params = build_params(child)?,
            Rule::ty           => ret = Some(build_ty(child)?),
            Rule::where_clause => where_bounds = build_where_clause(child)?,
            Rule::block        => body = Some(build_block(child)?),
            r => return Err(anyhow!("fun_decl: unexpected piece {:?}", r)),
        }
    }

    let name = name.ok_or_else(|| anyhow!("fun_decl: missing name"))?;
    let ret  = ret.ok_or_else(|| anyhow!("fun_decl `{}` missing return type", &name))?;
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
            Rule::KW_EXTERN | Rule::KW_FN => {}
            Rule::ident      => name = Some(child.as_str().to_string()),
            Rule::param_list => params = build_params(child)?,
            Rule::ty         => ret = Some(build_ty(child)?),
            // 分号是字面量，pest 不会作为子节点返回，这里无需匹配
            _other => {}
        }
    }
    let name = name.ok_or_else(|| anyhow!("extern_fun: missing name"))?;
    let ret  = ret.ok_or_else(|| anyhow!("extern_fun `{}` missing return type", &name))?;
    Ok(Item::Fun(FunDecl {
        name,
        type_params: vec![],
        params,
        ret,
        where_bounds: vec![],
        body: Block { stmts: vec![], tail: None }, // 外部函数无函数体
        is_extern: true,
    }))
}

fn build_import_item(p: Pair<Rule>) -> Result<Item> {
    for x in p.into_inner() {
        if x.as_rule() == Rule::string_lit {
            return Ok(Item::Import(unescape_string(x.as_str())));
        }
    }
    Err(anyhow!("import: missing string path"))
}

fn build_global_item(p: Pair<Rule>) -> Result<Item> {
    // let_decl = (KW_LET|KW_CONST) ident ":" ty "=" expr ";"
    let mut it = p.into_inner();
    let kw = it.next().ok_or_else(|| anyhow!("let_decl: missing kw"))?;
    let is_const = kw.as_rule() == Rule::KW_CONST;
    let name = it.next().ok_or_else(|| anyhow!("let_decl: missing ident"))?.as_str().to_string();
    let ty   = build_ty(it.next().ok_or_else(|| anyhow!("let_decl: missing type"))?)?;
    let init = build_expr(it.next().ok_or_else(|| anyhow!("let_decl: missing init expr"))?)?;
    Ok(Item::Global { name, ty, init, is_const })
}

/* ================================
 * where 子句
 * ================================ */
fn build_where_clause(p: Pair<Rule>) -> Result<Vec<WherePred>> {
    let mut preds = Vec::new();
    for child in p.into_inner() {
        if child.as_rule() != Rule::where_item { continue; }
        let mut it = child.into_inner();
        let first = it.next().ok_or_else(|| anyhow!("where_item empty"))?;
        match first.as_rule() {
            // where_pred = ty ":" bound ("+" bound)*
            Rule::where_pred => preds.push(build_where_pred(first)?),

            // 简写：直接写一个 bound（如 PairEq<T,U>）
            // 使用占位类型 __Self 表示“谓词约束”
            Rule::bound => {
                let tref = build_bound_as_traitref(first)?;
                preds.push(WherePred {
                    ty: Ty::App { name: "__Self".to_string(), args: vec![] },
                    bounds: vec![tref],
                });
            }
            r => return Err(anyhow!("where_item: unexpected {:?}", r)),
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
    // bound = ident ("<" ~ ty ~ ("," ~ ty)* ~ ">")?
    let mut it = p.into_inner();
    let head = it.next().ok_or_else(|| anyhow!("bound: missing head"))?;
    let name = match head.as_rule() {
        Rule::ident => head.as_str().to_string(),
        // 若将来扩到 qident，在此放宽：
        // Rule::qident => head.as_str().to_string(),
        other => return Err(anyhow!("bound head not ident: {:?}", other)),
    };
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
            // ty_app = ident "<" ty ("," ty)* ">"
            let mut it = p.into_inner();
            let head = it.next().ok_or_else(|| anyhow!("ty_app: missing head ident"))?;
            let name = match head.as_rule() {
                Rule::ident => head.as_str().to_string(),
                other => return Err(anyhow!("ty_app head not ident: {:?}", other)),
            };
            let mut args = Vec::<Ty>::new();
            for a in it {
                if a.as_rule() == Rule::ty {
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
 * block / stmt（函数体内部）
 * ================================ */
fn build_block(p: Pair<Rule>) -> Result<Block> {
    let mut stmts = Vec::new();
    let mut tail = None;
    for x in p.into_inner() {
        match x.as_rule() {
            Rule::stmt => stmts.push(build_stmt(x)?),
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
    if p.as_rule() != Rule::stmt {
        return Err(anyhow!("build_stmt expects Rule::stmt"));
    }
    let mut it = p.clone().into_inner();
    let first = it.next().ok_or_else(|| anyhow!("empty stmt"))?;

    match first.as_rule() {
        // 显式 let_decl 分支（顶层/块内统一）
        Rule::let_decl => {
            if let Item::Global { name, ty, init, is_const } = build_global_item(first)? {
                return Ok(Stmt::Let { name, ty, init, is_const });
            } else {
                unreachable!()
            }
        }

        // 命名子规则
        Rule::assign_stmt => {
            let mut ii = first.into_inner();
            let name = ii.next().unwrap().as_str().to_string();
            let expr = build_expr(ii.next().unwrap())?;
            Ok(Stmt::Assign { name, expr })
        }
        Rule::while_stmt => {
            let mut ii = first.into_inner();
            let _kw = ii.next();
            let cond = build_expr(ii.next().unwrap())?;
            let body = build_block(ii.next().unwrap())?;
            Ok(Stmt::While { cond, body })
        }
        Rule::for_stmt => build_for_stmt(first),
        Rule::if_stmt => {
            let mut ii = first.into_inner();
            let _kw = ii.next();
            let cond = build_expr(ii.next().unwrap())?;
            let then_b = build_block(ii.next().unwrap())?;
            let else_b = ii.next().map(build_block).transpose()?;
            Ok(Stmt::If { cond, then_b, else_b })
        }
        Rule::break_stmt => Ok(Stmt::Break),
        Rule::continue_stmt => Ok(Stmt::Continue),
        Rule::return_stmt => {
            let mut ii = first.into_inner();
            let _kw = ii.next();
            let expr = ii.next().map(build_expr).transpose()?;
            Ok(Stmt::Return(expr))
        }
        Rule::expr_stmt => {
            let e = build_expr(first.into_inner().next().unwrap())?;
            Ok(Stmt::Expr(e))
        }

        // 兼容：let/const 直接作为 stmt 的一部分（无 let_decl 包装）
        Rule::KW_LET | Rule::KW_CONST => {
            let is_const = first.as_rule() == Rule::KW_CONST;
            let name = it.next().ok_or_else(|| anyhow!("let: missing ident"))?.as_str().to_string();
            let ty   = build_ty(it.next().ok_or_else(|| anyhow!("let: missing type"))?)?;
            let init = build_expr(it.next().ok_or_else(|| anyhow!("let: missing init expr"))?)?;
            Ok(Stmt::Let { name, ty, init, is_const })
        }

        // 兼容：赋值语句无 assign_stmt 包装： ident "=" expr ";"
        Rule::ident => {
            if let Some(second) = it.next() {
                if second.as_rule() == Rule::expr {
                    let name = first.as_str().to_string();
                    let expr = build_expr(second)?;
                    return Ok(Stmt::Assign { name, expr });
                }
            }
            // 退化为普通表达式语句
            let mut again = p.into_inner();
            let only = again.next().unwrap();
            if only.as_rule() == Rule::expr {
                let e = build_expr(only)?;
                Ok(Stmt::Expr(e))
            } else {
                Err(anyhow!("stmt starting with ident but not assign/expr"))
            }
        }

        // 兜底：如果第一个就是 expr，则为表达式语句
        Rule::expr => {
            let e = build_expr(first)?;
            Ok(Stmt::Expr(e))
        }

        r => Err(anyhow!("build_stmt: unexpected first piece {:?}", r)),
    }
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
            Rule::expr     => cond = Some(build_expr(x)?),
            Rule::block    => body = Some(build_block(x)?),
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
            let ty   = build_ty(it.next().unwrap())?;
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
        Rule::int_lit  => parse_int_pattern(p.as_str())?,
        Rule::char_lit => Pattern::Char(parse_char_lit(p.as_str())?),
        Rule::bool_lit => Pattern::Bool(p.as_str() == "true"),
        r => return Err(anyhow!("unexpected pattern: {:?}", r)),
    })
}

fn build_expr(p: Pair<Rule>) -> Result<Expr> {
    Ok(match p.as_rule() {
        // 包装层
        Rule::expr => build_expr(p.into_inner().next().unwrap())?,

        // 二元优先级
        Rule::logic_or | Rule::logic_and | Rule::equality | Rule::compare | Rule::add | Rule::mult => {
            fold_binary(p)?
        }

        // 一元
        Rule::unary => {
            let mut it = p.into_inner();
            let first = it.next().unwrap();
            match first.as_rule() {
                Rule::OP_NOT => Expr::Unary { op: UnOp::Not, rhs: Box::new(build_expr(it.next().unwrap())?) },
                Rule::OP_SUB => Expr::Unary { op: UnOp::Neg, rhs: Box::new(build_expr(it.next().unwrap())?) },
                _ => build_expr(first)?,
            }
        }

        // 后缀（函数/方法调用）
        Rule::postfix => {
            let mut it = p.into_inner();

            // 读取头
            let head = it.next().unwrap();
            let mut callee_opt: Option<Callee> = None;
            let mut base_expr: Option<Expr> = None;

            match head.as_rule() {
                // name_ref 统一 ident & qident
                Rule::name_ref => {
                    let node = head.into_inner().next().unwrap();
                    match node.as_rule() {
                        Rule::ident => {
                            callee_opt = Some(Callee::Name(node.as_str().to_string()));
                        }
                        Rule::qident => {
                            let s = node.as_str();
                            if let Some(pos) = s.rfind("::") {
                                let trait_path = &s[..pos];      // 可能为多段路径 A::B::C
                                let method     = &s[pos+2..];    // 末段
                                callee_opt = Some(Callee::Qualified {
                                    trait_name: trait_path.to_string(),
                                    method: method.to_string(),
                                });
                            } else {
                                return Err(anyhow!("qident without '::': {}", s));
                            }
                        }
                        other => return Err(anyhow!("name_ref unexpected: {:?}", other)),
                    }
                }
                // 其他原子表达式：group/block/字面量 等
                _ => {
                    base_expr = Some(build_expr(head)?);
                }
            }

            // 依次消费 (call_suffix)*
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

                // 组装调用
                let callee = if let Some(c) = &callee_opt {
                    c.clone()
                } else {
                    match base_expr.take() {
                        Some(Expr::Var(name)) => Callee::Name(name),
                        _ => return Err(anyhow!("call on non-ident expression")),
                    }
                };

                out_call = Some(Expr::Call { callee, generics, args });
                // 形成调用后，不再把结果当“可继续作为函数名”的头
                callee_opt = None;
            }

            // 无任何 call_suffix：把“头部”直接作为表达式返回
            if let Some(e) = out_call {
                e
            } else if let Some(c) = callee_opt {
                match c {
                    Callee::Name(n) => Expr::Var(n), // 变量引用
                    Callee::Qualified { .. } => {
                        return Err(anyhow!(
                            "qualified path cannot be used as a value; call it like `Trait::method<...>(...)`"
                        ));
                    }
                }
            } else if let Some(be) = base_expr {
                be // (a+b) / { ... } / 字面量
            } else {
                return Err(anyhow!("postfix head missing"));
            }
        }

        // 直接出现的 name_ref（不用作调用）→ ident 作为变量，qident 禁止当值
        Rule::name_ref => {
            let node = p.into_inner().next().unwrap();
            match node.as_rule() {
                Rule::ident => Expr::Var(node.as_str().to_string()),
                Rule::qident => {
                    return Err(anyhow!(
                        "qualified path cannot be used as a value; call it like `Trait::method<...>(...)`"
                    ));
                }
                other => return Err(anyhow!("name_ref unexpected: {:?}", other)),
            }
        }
        Rule::qident => {
            return Err(anyhow!(
                "qualified path cannot be used as a value; call it like `Trait::method<...>(...)`"
            ));
        }

        // 其它原子
        Rule::group      => build_expr(p.into_inner().next().unwrap())?,
        Rule::if_expr    => build_if_expr(p)?,
        Rule::match_expr => build_match_expr(p)?,
        Rule::int_lit    => parse_int_expr(p.as_str())?,
        Rule::long_lit   => { let n: i64 = parse_long_lit(p.as_str())?; Expr::Long(n) }

        Rule::float_lit  => Expr::Float(parse_float32_lit(p.as_str())?),
        Rule::double_lit => Expr::Double(parse_float_lit(p.as_str())?),

        Rule::char_lit   => Expr::Char(parse_char_lit(p.as_str())?),
        Rule::bool_lit   => Expr::Bool(p.as_str() == "true"),
        Rule::string_lit => Expr::Str(unescape_string(p.as_str())),
        Rule::ident      => Expr::Var(p.as_str().to_string()),
        Rule::block      => Expr::Block(build_block(p)?),

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

/* ================================
 * trait / impl
 * ================================ */
fn build_trait_decl(p: Pair<Rule>) -> Result<TraitDecl> {
    let mut name: Option<String> = None;
    let mut tparams: Vec<String> = Vec::new();
    let mut methods: Vec<TraitMethodSig> = Vec::new();

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::KW_TRAIT => {}
            Rule::ident     => name = Some(x.as_str().to_string()),
            Rule::ty_params => tparams = build_ty_params(x)?,
            Rule::trait_item => methods.push(build_trait_item(x)?),
            other => return Err(anyhow!("trait_decl: unexpected {:?}", other)),
        }
    }
    Ok(TraitDecl {
        name: name.ok_or_else(|| anyhow!("trait_decl: missing name"))?,
        type_params: tparams,
        items: methods,
    })
}

fn build_trait_item(p: Pair<Rule>) -> Result<TraitMethodSig> {
    let mut name: Option<String> = None;
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::KW_FN => {}
            Rule::ident      => name = Some(x.as_str().to_string()),
            Rule::param_list => params = build_params(x)?,
            Rule::ty         => ret = Some(build_ty(x)?),
            _ => {}
        }
    }
    Ok(TraitMethodSig {
        name: name.ok_or_else(|| anyhow!("trait_item: missing name"))?,
        params,
        ret: ret.ok_or_else(|| anyhow!("trait_item: missing return type"))?,
    })
}

fn build_impl_decl(p: Pair<Rule>) -> Result<ImplDecl> {
    // impl_decl = KW_IMPL ident ty_args? "{" impl_item* "}"
    let mut trait_name: Option<String> = None;
    let mut trait_args: Vec<Ty> = Vec::new();
    let mut items: Vec<ImplMethod> = Vec::new();

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::KW_IMPL => {}
            Rule::ident   => trait_name = Some(x.as_str().to_string()),
            Rule::ty_args => trait_args = build_ty_args(x)?,
            Rule::impl_item => items.push(build_impl_item(x)?),
            _ => {}
        }
    }
    Ok(ImplDecl {
        trait_name: trait_name.ok_or_else(|| anyhow!("impl: missing trait name"))?,
        trait_args,
        items,
    })
}

fn build_impl_item(p: Pair<Rule>) -> Result<ImplMethod> {
    // impl_item = "fn name(params) -> ty block"
    let mut name: Option<String> = None;
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;
    let mut body: Option<Block> = None;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::KW_FN => {}
            Rule::ident      => name = Some(x.as_str().to_string()),
            Rule::param_list => params = build_params(x)?,
            Rule::ty         => ret = Some(build_ty(x)?),
            Rule::block      => body = Some(build_block(x)?),
            _ => {}
        }
    }

    Ok(ImplMethod {
        name: name.ok_or_else(|| anyhow!("impl fn: missing name"))?,
        params,
        ret: ret.ok_or_else(|| anyhow!("impl fn: missing return type"))?,
        body: body.ok_or_else(|| anyhow!("impl fn: missing body"))?,
    })
}

/* ================================
 * 字面量解析 & 工具
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

// 保留旧名：用于解析 Double（无后缀）
fn parse_float_lit(s: &str) -> Result<f64> { Ok(s.parse::<f64>()?) }

// 新增：解析 Float（带 f/F 后缀）
fn parse_float32_lit(s: &str) -> Result<f32> {
    let ns = s.strip_suffix('f')
        .or_else(|| s.strip_suffix('F'))
        .ok_or_else(|| anyhow!("invalid float32 literal suffix: {}", s))?;
    Ok(ns.parse::<f32>()?)
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
