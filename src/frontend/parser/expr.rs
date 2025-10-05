fn build_expr(p: Pair<Rule>, file: FileId) -> Result<Expr> {
    Ok(match p.as_rule() {
        // 包装层
        Rule::expr => build_expr(p.into_inner().next().unwrap(), file)?,

        // 二元优先级（含 add / mult 等）
        Rule::logic_or | Rule::logic_and | Rule::equality | Rule::compare | Rule::add | Rule::mult => {
            fold_binary(p, file)?
        }

        // as 转换层
        Rule::cast => build_cast(p, file)?,
        Rule::cast_term => {
            let mut it = p.into_inner();
            build_expr(it.next().unwrap(), file)?
        }

        // 一元
        Rule::unary => {
            let sp = sp_of(&p, file);
            let mut it = p.into_inner();
            let first = it.next().unwrap();
            match first.as_rule() {
                Rule::OP_NOT => Expr::Unary { op: UnOp::Not, rhs: Box::new(build_expr(it.next().unwrap(), file)?), span: sp },
                Rule::OP_SUB => Expr::Unary { op: UnOp::Neg, rhs: Box::new(build_expr(it.next().unwrap(), file)?), span: sp },
                _ => build_expr(first, file)?,
            }
        }

        // 后缀（函数/方法调用 + 字段访问）
        Rule::postfix => {
            let sp_post = sp_of(&p, file);
            let mut it = p.into_inner();

            // 读取头
            let head = it.next().unwrap();
            let mut callee_opt: Option<Callee> = None; // 当作“可调用名”的头
            let mut base_expr: Option<Expr> = None;    // 当作“可继续取字段/调用的值”的头

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
                                let trait_path = &s[..pos];      // 可为多段
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
                _ => {
                    base_expr = Some(build_expr(head, file)?);
                }
            }

            // 依次消费 (field_suffix | call_suffix)*
            let mut out_expr: Option<Expr> = None;
            for suf in it {
                match suf.as_rule() {
                    Rule::field_suffix => {
                        // 解析字段名
                        let fname = suf.into_inner().next().unwrap().as_str().to_string();
                        // 字段一定要有 base 值
                        let base = if let Some(e) = out_expr.take() {
                            e
                        } else if let Some(e) = base_expr.take() {
                            e
                        } else if callee_opt.is_some() {
                            // 从可调用名降解为值（变量引用）
                            match callee_opt.take().unwrap() {
                                Callee::Name(n) => Expr::Var { name: n, span: sp_post },
                                Callee::Qualified { .. } => return Err(anyhow!("cannot access field on a qualified path")),
                            }
                        } else {
                            return Err(anyhow!("field access without base"));
                        };
                        out_expr = Some(Expr::Field { base: Box::new(base), field: fname, span: sp_post });
                    }
                    Rule::call_suffix => {
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
                                            args = build_arg_list(maybe_args, file)?;
                                        }
                                    }
                                }
                                Rule::arg_list => {
                                    args = build_arg_list(first, file)?;
                                }
                                r => return Err(anyhow!("call_suffix: unexpected {:?}", r)),
                            }
                        }

                        // 组装调用
                        let callee = if let Some(c) = &callee_opt {
                            c.clone()
                        } else {
                            match out_expr.take().or_else(|| base_expr.take()) {
                                Some(Expr::Var { name, span: _ }) => Callee::Name(name),
                                _ => return Err(anyhow!("call on non-ident expression")),
                            }
                        };
                        out_expr = Some(Expr::Call { callee, generics, args, span: sp_post });
                        // 形成调用后，不再把结果当“可继续作为函数名”的头
                        callee_opt = None;
                    }
                    other => return Err(anyhow!("postfix: unexpected suffix {:?}", other)),
                }
            }

            if let Some(e) = out_expr {
                e
            } else if let Some(c) = callee_opt {
                match c {
                    Callee::Name(n) => Expr::Var { name: n, span: sp_post }, // 变量引用
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
            let sp = sp_of(&p, file);
            let node = p.into_inner().next().unwrap();
            match node.as_rule() {
                Rule::ident => Expr::Var { name: node.as_str().to_string(), span: sp },
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

        // 结构体字面量
        Rule::struct_lit => {
            // struct_lit = (ident_with_ty | ident) ~ "{" ~ struct_init_list? ~ "}"
            let sp = sp_of(&p, file);
            let mut name: Option<String> = None;
            let mut generics: Vec<Ty> = Vec::new();
            let mut fields: Vec<(String, Expr)> = Vec::new();
            for x in p.into_inner() {
                match x.as_rule() {
                    Rule::ident => { 
                        if name.is_none() { 
                            name = Some(x.as_str().to_string()); 
                        }
                    }
                    Rule::ident_with_ty => {
                        if name.is_none() {
                            // ident_with_ty = ident ~ ty_args
                            for part in x.into_inner() {
                                match part.as_rule() {
                                    Rule::ident => name = Some(part.as_str().to_string()),
                                    Rule::ty_args => generics = build_ty_args(part)?,
                                    _ => {}
                                }
                            }
                        }
                    }
                    Rule::ty_args => { generics = build_ty_args(x)?; }
                    Rule::struct_init_list => {
                        for f in x.into_inner() {
                            if f.as_rule() != Rule::struct_init { continue; }
                            let mut ii = f.into_inner();
                            let fname = ii.next().ok_or_else(|| anyhow!("struct_init: missing ident"))?.as_str().to_string();
                            let fexpr = build_struct_field_value(ii.next().ok_or_else(|| anyhow!("struct_init: missing field value"))?, file)?;
                            fields.push((fname, fexpr));
                        }
                    }
                    _ => {}
                }
            }
            Expr::StructLit {
                name: name.ok_or_else(|| anyhow!("struct_lit: missing name"))?,
                generics,
                fields,
                span: sp,
            }
        }

        // 其它原子
        Rule::group      => build_expr(p.into_inner().next().unwrap(), file)?,
        Rule::if_expr    => build_if_expr(p, file)?,
        Rule::match_expr => build_match_expr(p, file)?,
        Rule::int_lit    => { let sp = sp_of(&p, file); parse_int_expr(p.as_str(), sp)? }
        Rule::long_lit   => { let n: i64 = parse_long_lit(p.as_str())?; Expr::Long  { value: n, span: sp_of(&p, file) } }
        Rule::float_lit  => { let v = parse_float32_lit(p.as_str())?;   Expr::Float { value: v, span: sp_of(&p, file) } }
        Rule::double_lit => { let v = parse_float_lit(p.as_str())?;     Expr::Double{ value: v, span: sp_of(&p, file) } }
        Rule::char_lit   => { let u = parse_char_lit(p.as_str())?;      Expr::Char  { value: u, span: sp_of(&p, file) } }
        Rule::bool_lit   => Expr::Bool { value: p.as_str() == "true", span: sp_of(&p, file) },
        Rule::string_lit => Expr::Str  { value: unescape_string(p.as_str()), span: sp_of(&p, file) },
        Rule::ident      => Expr::Var  { name: p.as_str().to_string(), span: sp_of(&p, file) },
        Rule::block      => {
            let sp = sp_of(&p, file);
            let b  = build_block(p, file)?;
            Expr::Block { block: b, span: sp }
        }

        other => {
            let mut it = p.clone().into_inner();
            if let Some(q) = it.next() {
                build_expr(q, file)?
            } else {
                return Err(anyhow!("unexpected expr rule: {:?}", other));
            }
        }
    })
}

fn build_if_expr(p: Pair<Rule>, file: FileId) -> Result<Expr> {
    let sp = sp_of(&p, file);
    let mut it = p.into_inner();
    let _if = it.next();
    let cond = build_expr(it.next().unwrap(), file)?;
    let then_b = build_block(it.next().unwrap(), file)?;
    let _else = it.next();
    let else_b = build_block(it.next().unwrap(), file)?;
    Ok(Expr::If { cond: Box::new(cond), then_b, else_b, span: sp })
}

fn build_match_expr(p: Pair<Rule>, file: FileId) -> Result<Expr> {
    let sp = sp_of(&p, file);
    let mut scrut: Option<Expr> = None;
    let mut arms: Vec<(Pattern, Block)> = Vec::new();
    let mut default: Option<Block> = None;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::expr => scrut = Some(build_expr(x, file)?),
            Rule::match_arms => {
                for a in x.into_inner() {
                    match a.as_rule() {
                        Rule::match_arm => {
                            let mut ii = a.into_inner();
                            let pat = build_pattern(ii.next().unwrap())?;
                            let blk = build_block(ii.next().unwrap(), file)?;
                            arms.push((pat, blk));
                        }
                        Rule::match_default => {
                            let mut ii = a.into_inner();
                            let blk = build_block(ii.next().unwrap(), file)?;
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
    Ok(Expr::Match { scrut: Box::new(scrut), arms, default, span: sp })
}

fn build_cast(p: Pair<Rule>, file: FileId) -> Result<Expr> {
    // cast = mult ~ (KW_AS ~ ty)+
    let sp = sp_of(&p, file);
    let mut it = p.into_inner();
    // 起始操作数：mult
    let mut e = build_expr(it.next().ok_or_else(|| anyhow!("cast: missing base expr"))?, file)?;
    // 后续重复：KW_AS ty
    while let Some(tok) = it.next() {
        match tok.as_rule() {
            Rule::KW_AS => {
                let ty_pair = it.next().ok_or_else(|| anyhow!("cast: missing target type after `as`"))?;
                if ty_pair.as_rule() != Rule::ty {
                    return Err(anyhow!("cast: expected type after `as`, got {:?}", ty_pair.as_rule()));
                }
                let ty = build_ty(ty_pair)?;
                let merged = Span::merge(span_of_expr(&e), sp);
                e = Expr::Cast { expr: Box::new(e), ty, span: merged };
            }
            other => return Err(anyhow!("cast: unexpected piece {:?}", other)),
        }
    }
    Ok(e)
}

fn build_arg_list(p: Pair<Rule>, file: FileId) -> Result<Vec<Expr>> {
    let mut args = Vec::new();
    for ae in p.into_inner() {
        args.push(build_expr(ae, file)?);
    }
    Ok(args)
}

fn fold_binary(p: Pair<Rule>, file: FileId) -> Result<Expr> {
    use BinOp::*;
    let mut it = p.into_inner();
    let mut lhs = build_expr(it.next().unwrap(), file)?;
    while let Some(op) = it.next() {
        let rhs = build_expr(it.next().unwrap(), file)?;
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
        let span = Span::merge(span_of_expr(&lhs), span_of_expr(&rhs));
        lhs = Expr::Binary { op: bop, lhs: Box::new(lhs), rhs: Box::new(rhs), span };
    }
    Ok(lhs)
}

fn build_struct_field_value(p: Pair<Rule>, file: FileId) -> Result<Expr> {
    // struct_field_value now contains expr, so we can directly parse it
    build_expr(p, file)
}

// field_access is handled via field_suffix in the postfix chain