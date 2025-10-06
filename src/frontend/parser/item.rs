fn build_item(p: Pair<Rule>, file: FileId) -> Result<Item> {
    match p.as_rule() {
        Rule::fun_decl    => build_fun_item(p, file),
        Rule::extern_fun  => build_extern_fun_item(p, file),
        Rule::import_decl => build_import_item(p, file),
        Rule::let_decl    => build_global_item(p, file),
        Rule::struct_decl => {
            let sp = sp_of(&p, file);
            let sd = build_struct_decl(p, file)?;
            Ok(Item::Struct(sd, sp))
        }
        Rule::trait_decl  => {
            let sp = sp_of(&p, file);
            let td = build_trait_decl(p, file)?;
            Ok(Item::Trait(td, sp))
        }
        Rule::impl_decl => {
            let sp = sp_of(&p, file);
            let id = build_impl_decl(p, file)?;
            Ok(Item::Impl(id, sp))
        }
        other => Err(anyhow!("item: unsupported top-level rule: {:?}", other)),
    }
}

fn build_fun_item(p: Pair<Rule>, file: FileId) -> Result<Item> {
    let mut vis: Visibility = Visibility::Private;
    let mut name: Option<String> = None;
    let mut type_params: Vec<String> = Vec::new();
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret_pair: Option<Pair<Rule>> = None;
    let mut where_bounds: Vec<WherePred> = Vec::new();
    let mut body: Option<Block> = None;

    for child in p.clone().into_inner() {
        match child.as_rule() {
            Rule::KW_PUB => { vis = Visibility::Public; }
            Rule::KW_FN => {}
            Rule::ident        => name = Some(child.as_str().to_string()),
            Rule::ty_params    => type_params = build_ty_params(child)?,
            Rule::param_list   => params = build_params(child)?,
            Rule::ret_type     => ret_pair = Some(child),
            Rule::where_clause => where_bounds = build_where_clause(child, file)?,
            Rule::block        => body = Some(build_block(child, file)?),
            r => return Err(anyhow!("fun_decl: unexpected piece {:?}", r)),
        }
    }

    let sp = sp_of(&p, file);
    let name = name.ok_or_else(|| anyhow!("fun_decl: missing name"))?;
    let ret  = parse_ret_type_pair(ret_pair, file)?;
    let body = body.ok_or_else(|| anyhow!("fun_decl `{}` missing body", &name))?;

    let f = FunDecl {
        vis,
        name,
        type_params,
        params,
        ret,
        where_bounds,
        body,
        is_extern: false,
        span: sp,
    };
    Ok(Item::Fun(f, sp))
}

fn build_extern_fun_item(p: Pair<Rule>, file: FileId) -> Result<Item> {
    let mut vis: Visibility = Visibility::Private;
    let mut name: Option<String> = None;
    let mut type_params: Vec<String> = Vec::new();
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret_pair: Option<Pair<Rule>> = None;

    for child in p.clone().into_inner() {
        match child.as_rule() {
            Rule::KW_PUB => { vis = Visibility::Public; }
            Rule::KW_EXTERN | Rule::KW_FN => {}
            Rule::ident      => name = Some(child.as_str().to_string()),
            Rule::ty_params  => type_params = build_ty_params(child)?,
            Rule::param_list => params = build_params(child)?,
            Rule::ret_type   => ret_pair = Some(child),
            _other => {}
        }
    }
    let sp = sp_of(&p, file);
    let name = name.ok_or_else(|| anyhow!("extern_fun: missing name"))?;
    let ret  = parse_ret_type_pair(ret_pair, file)?;
    let f = FunDecl {
        vis,
        name,
        type_params,
        params,
        ret,
        where_bounds: vec![],
        body: Block { stmts: vec![], tail: None, span: sp }, // 外部函数无函数体
        is_extern: true,
        span: sp,
    };
    Ok(Item::Fun(f, sp))
}

fn build_import_item(p: Pair<Rule>, file: FileId) -> Result<Item> {
    let sp = sp_of(&p, file);
    for x in p.into_inner() {
        if x.as_rule() == Rule::string_lit {
            return Ok(Item::Import(unescape_string(x.as_str()), sp));
        }
    }
    Err(anyhow!("import: missing string path"))
}

fn build_global_item(p: Pair<Rule>, file: FileId) -> Result<Item> {
    // let_decl = (KW_LET|KW_CONST) ident ":" ty "=" expr ";"
    let sp = sp_of(&p, file);
    let mut it = p.into_inner();
    let kw = it.next().ok_or_else(|| anyhow!("let_decl: missing kw"))?;
    let is_const = kw.as_rule() == Rule::KW_CONST;
    let name = it.next().ok_or_else(|| anyhow!("let_decl: missing ident"))?.as_str().to_string();
    let ty   = build_ty(it.next().ok_or_else(|| anyhow!("let_decl: missing type"))?)?;
    let init = build_expr(it.next().ok_or_else(|| anyhow!("let_decl: missing init expr"))?, file)?;
    Ok(Item::Global { name, ty, init, is_const, span: sp })
}

fn build_trait_decl(p: Pair<Rule>, file: FileId) -> Result<TraitDecl> {
    // grammar:
    // trait_decl = { KW_PUB? ~ KW_TRAIT ~ ident ~ ty_params? ~ "{" ~ trait_item* ~ "}" }
    let sp = sp_of(&p, file);
    let mut vis: Visibility = Visibility::Private;
    let mut name: Option<String> = None;
    let mut tparams: Vec<String> = Vec::new();
    let mut items: Vec<TraitItem> = Vec::new();

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::KW_PUB => { vis = Visibility::Public; }
            Rule::KW_TRAIT => {}
            Rule::ident      => name = Some(x.as_str().to_string()),
            Rule::ty_params  => tparams = build_ty_params(x)?,
            Rule::trait_item => items.push(build_trait_item(x, file)?),
            other => return Err(anyhow!("trait_decl: unexpected {:?}", other)),
        }
    }
    Ok(TraitDecl {
        vis,
        name: name.ok_or_else(|| anyhow!("trait_decl: missing name"))?,
        type_params: tparams,
        items,
        span: sp,
    })
}

fn build_trait_item(p: Pair<Rule>, file: FileId) -> Result<TraitItem> {
    // trait_item:
    // 1) 方法：KW_PUB? KW_FN ident "(" param_list? ")" ret_type ";"
    // 2) 关联类型：KW_PUB? KW_TYPE ident ty_params? ( ":" bound ("+" bound)* )? ";"
    let sp = sp_of(&p, file);
    let mut kind: Option<&'static str> = None;
    for x in p.clone().into_inner() {
        match x.as_rule() {
            Rule::KW_FN => { kind = Some("fn"); break; }
            Rule::KW_TYPE => { kind = Some("type"); break; }
            _ => {}
        }
    }
    match kind {
        Some("fn") => {
            let mut vis: Visibility = Visibility::Private;
            let mut name: Option<String> = None;
            let mut params: Vec<(String, Ty)> = Vec::new();
            let mut ret_pair: Option<Pair<Rule>> = None;

            for x in p.into_inner() {
                match x.as_rule() {
                    Rule::KW_PUB => { vis = Visibility::Public; }
                    Rule::KW_FN => {}
                    Rule::ident      => name = Some(x.as_str().to_string()),
                    Rule::param_list => params = build_params(x)?,
                    Rule::ret_type   => ret_pair = Some(x),
                    // 注：grammar 中 ; 是 silent（_）规则，这里不会出现 Rule::SEMICOLON
                    _ => {}
                }
            }
            let ret = parse_ret_type_pair(ret_pair, file)?;
            Ok(TraitItem::Method(TraitMethodSig {
                vis,
                name: name.ok_or_else(|| anyhow!("trait method: missing name"))?,
                params,
                ret,
                span: sp,
            }))
        }
        Some("type") => {
            let mut vis: Visibility = Visibility::Private;
            let mut name: Option<String> = None;
            let mut type_params: Vec<String> = Vec::new();
            let mut bounds: Vec<TraitRef> = Vec::new();

            for x in p.into_inner() {
                match x.as_rule() {
                    Rule::KW_PUB  => { vis = Visibility::Public; }
                    Rule::KW_TYPE => {}
                    Rule::ident   => name = Some(x.as_str().to_string()),
                    Rule::ty_params => type_params = build_ty_params(x)?,
                    Rule::bound     => bounds.push(build_bound_as_traitref(x)?),
                    // ":" 与 ";" 同样是 silent，不会出现在 parse 树里
                    _ => {}
                }
            }

            Ok(TraitItem::AssocType(TraitAssocTypeDecl {
                vis,
                name: name.ok_or_else(|| anyhow!("trait assoc type: missing name"))?,
                type_params,
                bounds,
                span: sp,
            }))
        }
        _ => Err(anyhow!("trait_item: neither fn nor type")),
    }
}

fn build_impl_decl(p: Pair<Rule>, file: FileId) -> Result<ImplDecl> {
    // impl_decl = { KW_PUB? ~ KW_IMPL ~ ty_params? ~ (ty_app | qident | ident) ~ (KW_FOR ~ (ty_app | qident | ident))? ~ where_clause? ~ "{" ~ impl_item* ~ "}" }
    let sp = sp_of(&p, file);
    let mut vis: Visibility = Visibility::Private;
    let mut type_params: Vec<String> = Vec::new();
    let mut trait_name: Option<String> = None;
    let mut trait_args: Vec<Ty> = Vec::new();
    let mut where_bounds: Vec<WherePred> = Vec::new();
    let mut items: Vec<ImplItem> = Vec::new();
    let mut found_for = false;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::KW_PUB   => { vis = Visibility::Public; }
            Rule::KW_IMPL  => {}
            Rule::KW_FOR   => { found_for = true; }
            Rule::ty_params => type_params = build_ty_params(x)?,
            Rule::ident | Rule::qident => {
                if found_for {
                    // 在 for 之后的标识符是类型名称，我们暂时忽略它
                    // 因为当前的 ImplDecl 结构体没有存储实现类型
                } else if trait_name.is_none() {
                    trait_name = Some(x.as_str().to_string());
                } else {
                    // 若 grammar 把路径拆成多个 token，这里忽略后续
                }
            }
            Rule::ty_app => {
                if found_for {
                    // 在 for 之后的类型应用是类型名称，我们暂时忽略它
                } else if trait_name.is_none() {
                    // 解析类型应用，如 Box<T>
                    let ty_app = build_ty_app(x)?;
                    match ty_app {
                        Ty::App { name, args } => {
                            trait_name = Some(name);
                            trait_args = args;
                        }
                        _ => return Err(anyhow!("expected type application in impl")),
                    }
                } else {
                    // 若 grammar 把路径拆成多个 token，这里忽略后续
                }
            }
            Rule::ty => {
                if found_for {
                    // 在 for 之后的类型是类型名称，我们暂时忽略它
                    // 因为当前的 ImplDecl 结构体没有存储实现类型
                } else {
                    // 这不应该发生，因为 ty 只在 for 之后出现
                    return Err(anyhow!("unexpected type in impl declaration"));
                }
            }
            Rule::ty_args       => trait_args = build_ty_args(x)?,
            Rule::where_clause  => where_bounds = build_where_clause(x, file)?,
            Rule::impl_item     => items.push(build_impl_item(x, file)?),
            _ => {}
        }
    }
    let trait_name = trait_name.ok_or_else(|| anyhow!("impl: missing trait name"))?;
    
    // 自动推断类型参数：从 trait_args 中提取类型变量
    let mut inferred_type_params = type_params;
    if inferred_type_params.is_empty() {
        let mut free_vars = crate::utils::fast::FastSet::default();
        for ta in &trait_args {
            collect_free_tyvars_inline(ta, &mut free_vars);
        }
        inferred_type_params = free_vars.into_iter().collect();
    }
    
    Ok(ImplDecl {
        vis,
        type_params: inferred_type_params,
        trait_name,
        trait_args,
        where_bounds,
        items,
        span: sp,
    })
}

fn build_impl_item(p: Pair<Rule>, file: FileId) -> Result<ImplItem> {
    // impl_item:
    // 1) 方法：KW_PUB? KW_FN ident "(" param_list? ")" ret_type block
    // 2) 外部函数：KW_PUB? KW_EXTERN KW_FN ident "(" param_list? ")" ret_type ";"
    // 3) 关联类型：KW_PUB? KW_TYPE ident "=" ty ";"
    let sp = sp_of(&p, file);
    let mut kind: Option<&'static str> = None;
    for x in p.clone().into_inner() {
        match x.as_rule() {
            Rule::KW_FN => { 
                // Check if it's preceded by KW_EXTERN
                let mut is_extern = false;
                for y in p.clone().into_inner() {
                    if y.as_rule() == Rule::KW_EXTERN {
                        is_extern = true;
                        break;
                    }
                    if y.as_rule() == Rule::KW_FN {
                        break;
                    }
                }
                kind = Some(if is_extern { "extern_fn" } else { "fn" });
                break;
            }
            Rule::KW_TYPE => { kind = Some("type"); break; }
            _ => {}
        }
    }

    match kind {
        Some("fn") => {
            let mut vis: Visibility = Visibility::Private;
            let mut name: Option<String> = None;
            let mut params: Vec<(String, Ty)> = Vec::new();
            let mut ret_pair: Option<Pair<Rule>> = None;
            let mut body: Option<Block> = None;

            for x in p.into_inner() {
                match x.as_rule() {
                    Rule::KW_PUB      => { vis = Visibility::Public; }
                    Rule::KW_FN       => {}
                    Rule::ident       => name = Some(x.as_str().to_string()),
                    Rule::param_list  => params = build_params(x)?,
                    Rule::ret_type    => ret_pair = Some(x),
                    Rule::block       => body = Some(build_block(x, file)?),
                    _ => {}
                }
            }

            Ok(ImplItem::Method(ImplMethod {
                vis,
                name: name.ok_or_else(|| anyhow!("impl fn: missing name"))?,
                params,
                ret: parse_ret_type_pair(ret_pair, file)?,
                body: body.ok_or_else(|| anyhow!("impl fn: missing body"))?,
                span: sp,
            }))
        }
        Some("extern_fn") => {
            let mut vis: Visibility = Visibility::Private;
            let mut name: Option<String> = None;
            let mut params: Vec<(String, Ty)> = Vec::new();
            let mut ret_pair: Option<Pair<Rule>> = None;

            for x in p.into_inner() {
                match x.as_rule() {
                    Rule::KW_PUB      => { vis = Visibility::Public; }
                    Rule::KW_EXTERN   => {}
                    Rule::KW_FN       => {}
                    Rule::ident       => name = Some(x.as_str().to_string()),
                    Rule::param_list  => params = build_params(x)?,
                    Rule::ret_type    => ret_pair = Some(x),
                    _ => {}
                }
            }

            Ok(ImplItem::ExternMethod(ImplExternMethod {
                vis,
                name: name.ok_or_else(|| anyhow!("impl extern fn: missing name"))?,
                params,
                ret: parse_ret_type_pair(ret_pair, file)?,
                span: sp,
            }))
        }
        Some("type") => {
            let mut vis: Visibility = Visibility::Private;
            let mut name: Option<String> = None;
            let mut ty_opt: Option<Ty> = None;

            for x in p.into_inner() {
                match x.as_rule() {
                    Rule::KW_PUB  => { vis = Visibility::Public; }
                    Rule::KW_TYPE => {}
                    Rule::ident   => name = Some(x.as_str().to_string()),
                    Rule::ty      => ty_opt = Some(build_ty(x)?),
                    // "=" 与 ";" 在 grammar 中是 silent 规则
                    _ => {}
                }
            }

            Ok(ImplItem::AssocType(ImplAssocType {
                vis,
                name: name.ok_or_else(|| anyhow!("impl assoc type: missing name"))?,
                ty: ty_opt.ok_or_else(|| anyhow!("impl assoc type: missing type rhs"))?,
                span: sp,
            }))
        }
        _ => Err(anyhow!("impl_item: neither fn nor type")),
    }
}

fn build_struct_decl(p: Pair<Rule>, file: FileId) -> Result<StructDecl> {
    // struct_decl = { KW_PUB? ~ KW_STRUCT ~ ident ~ ty_params? ~ "{" ~ struct_field_list? ~ "}" }
    let sp = sp_of(&p, file);
    let mut vis: Visibility = Visibility::Private;
    let mut name: Option<String> = None;
    let mut type_params: Vec<String> = Vec::new();
    let mut fields: Vec<(String, Ty)> = Vec::new();

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::KW_PUB => { vis = Visibility::Public; }
            Rule::KW_STRUCT => {}
            Rule::ident => { if name.is_none() { name = Some(x.as_str().to_string()); } }
            Rule::ty_params => { type_params = build_ty_params(x)?; }
            Rule::struct_field_list => {
                for f in x.into_inner() {
                    if f.as_rule() != Rule::struct_field { continue; }
                    let mut it = f.into_inner();
                    let fname = it.next().ok_or_else(|| anyhow!("struct field: missing ident"))?.as_str().to_string();
                    let fty = build_ty(it.next().ok_or_else(|| anyhow!("struct field: missing type"))?)?;
                    fields.push((fname, fty));
                }
            }
            _ => {}
        }
    }

    Ok(StructDecl {
        vis,
        name: name.ok_or_else(|| anyhow!("struct: missing name"))?,
        type_params,
        fields,
        span: sp,
    })
}

// 内联函数：收集类型中的自由类型变量
fn collect_free_tyvars_inline(t: &crate::frontend::ast::Ty, out: &mut crate::utils::fast::FastSet<String>) {
    match t {
        crate::frontend::ast::Ty::Var(v) => { out.insert(v.clone()); }
        crate::frontend::ast::Ty::App { args, .. } => for a in args { collect_free_tyvars_inline(a, out) },
        _ => {}
    }
}

// 解析类型应用，如 Box<T>
fn build_ty_app(p: Pair<Rule>) -> Result<crate::frontend::ast::Ty> {
    let mut it = p.into_inner();
    let name = it.next().unwrap().as_str().to_string();
    let mut args = Vec::new();
    
    for arg in it {
        if arg.as_rule() == Rule::ty {
            args.push(build_ty(arg)?);
        }
    }
    
    Ok(crate::frontend::ast::Ty::App { name, args })
}