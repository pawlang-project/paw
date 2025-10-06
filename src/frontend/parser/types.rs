fn build_params(p: Pair<Rule>) -> Result<Vec<(String, Ty)>> {
    let mut v = Vec::new();
    for x in p.into_inner() {
        let mut it = x.into_inner(); // ident ~ (COLON ~ ty)?
        let first = it.next().unwrap();
        let name = match first.as_rule() {
            Rule::ident => first.as_str().to_string(),
            _ => return Err(anyhow!("expected ident in parameter")),
        };
        
        // 检查是否有类型声明
        let ty = if let Some(colon_ty) = it.next() {
            if colon_ty.as_rule() == Rule::ty {
                build_ty(colon_ty)?
            } else {
                return Err(anyhow!("expected type after colon"));
            }
        } else {
            // 没有类型声明，需要推断
            // 对于 self 参数，我们暂时返回一个占位符类型
            // 实际的类型推断将在类型检查阶段进行
            if name == "self" {
                // 返回一个特殊的占位符类型，表示需要推断
                Ty::Var("_Self".to_string())
            } else {
                return Err(anyhow!("parameter '{}' must have explicit type", name));
            }
        };
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
            // ty_app = (qident | ident) "<" ty ("," ~ ty)* ">"
            let mut it = p.into_inner();
            let head = it.next().ok_or_else(|| anyhow!("ty_app: missing head"))?;
            let name = match head.as_rule() {
                Rule::ident | Rule::qident => head.as_str().to_string(),
                other => return Err(anyhow!("ty_app head not ident/qident: {:?}", other)),
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
        Rule::KW_Byte   => Ty::Byte,
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

fn build_where_clause(p: Pair<Rule>, file: FileId) -> Result<Vec<WherePred>> {
    let mut preds = Vec::new();
    for child in p.into_inner() {
        if child.as_rule() != Rule::where_item { continue; }
        let mut it = child.into_inner();
        let first = it.next().ok_or_else(|| anyhow!("where_item empty"))?;
        match first.as_rule() {
            Rule::where_pred => preds.push(build_where_pred(first, file)?),
            Rule::bound => {
                let tref = build_bound_as_traitref(first.clone())?;
                let sp = sp_of(&first, file);
                preds.push(WherePred {
                    ty: Ty::App { name: "__Self".to_string(), args: vec![] },
                    bounds: vec![tref],
                    span: sp,
                });
            }
            r => return Err(anyhow!("where_item: unexpected {:?}", r)),
        }
    }
    Ok(preds)
}

fn build_where_pred(p: Pair<Rule>, file: FileId) -> Result<WherePred> {
    let sp = sp_of(&p, file);
    let mut it = p.into_inner();
    let ty = build_ty(it.next().ok_or_else(|| anyhow!("where_pred: missing ty"))?)?;
    let mut bounds = Vec::new();
    for b in it {
        if b.as_rule() == Rule::bound {
            bounds.push(build_bound_as_traitref(b)?);
        }
    }
    Ok(WherePred { ty, bounds, span: sp })
}

fn build_bound_as_traitref(p: Pair<Rule>) -> Result<TraitRef> {
    // bound = (qident | ident) ("<" ~ ty ~ ("," ~ ty)* ~ ">")?
    let mut it = p.into_inner();
    let head = it.next().ok_or_else(|| anyhow!("bound: missing head"))?;
    let name = match head.as_rule() {
        Rule::ident | Rule::qident => head.as_str().to_string(),
        other => return Err(anyhow!("bound head not ident/qident: {:?}", other)),
    };
    let mut args = Vec::new();
    for t in it {
        if t.as_rule() == Rule::ty {
            args.push(build_ty(t)?);
        }
    }
    Ok(TraitRef { name, args })
}

fn parse_ret_type_pair(ret_type_pair: Option<Pair<Rule>>, _file: FileId) -> Result<Ty> {
    if let Some(rt) = ret_type_pair {
        let mut it = rt.into_inner();
        let ty_pair = it.next().ok_or_else(|| anyhow!("ret_type without ty"))?;
        if ty_pair.as_rule() != Rule::ty {
            return Err(anyhow!("ret_type: expected ty, got {:?}", ty_pair.as_rule()));
        }
        build_ty(ty_pair)
    } else {
        Ok(Ty::Void)
    }
}