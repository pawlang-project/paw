fn build_block(p: Pair<Rule>, file: FileId) -> Result<Block> {
    let mut stmts = Vec::new();
    let mut tail = None;
    for x in p.clone().into_inner() {
        match x.as_rule() {
            Rule::stmt => stmts.push(build_stmt(x, file)?),
            Rule::tail_expr => {
                let e = build_expr(x.into_inner().next().unwrap(), file)?;
                tail = Some(Box::new(e));
            }
            _ => {}
        }
    }
    Ok(Block { stmts, tail, span: sp_of(&p, file) })
}

fn build_stmt(p: Pair<Rule>, file: FileId) -> Result<Stmt> {
    if p.as_rule() != Rule::stmt {
        return Err(anyhow!("build_stmt expects Rule::stmt"));
    }
    let stmt_span = sp_of(&p, file);
    let mut it = p.clone().into_inner();
    let first = it.next().ok_or_else(|| anyhow!("empty stmt"))?;

    match first.as_rule() {
        // 显式 let_decl 分支（顶层/块内统一）
        Rule::let_decl => {
            if let Item::Global { name, ty, init, is_const, .. } = build_global_item(first, file)? {
                return Ok(Stmt::Let { name, ty, init, is_const, span: stmt_span });
            } else {
                unreachable!()
            }
        }

        // 命名子规则
        Rule::assign_stmt => {
            let mut ii = first.into_inner();
            let name = ii.next().unwrap().as_str().to_string();
            let expr = build_expr(ii.next().unwrap(), file)?;
            Ok(Stmt::Assign { name, expr, span: stmt_span })
        }
        Rule::while_stmt => {
            let mut ii = first.into_inner();
            let _kw = ii.next();
            let cond = build_expr(ii.next().unwrap(), file)?;
            let body = build_block(ii.next().unwrap(), file)?;
            Ok(Stmt::While { cond, body, span: stmt_span })
        }
        Rule::for_stmt => build_for_stmt(first, file),
        Rule::if_stmt => {
            let mut ii = first.into_inner();
            let _kw = ii.next();
            let cond = build_expr(ii.next().unwrap(), file)?;
            let then_b = build_block(ii.next().unwrap(), file)?;
            let else_b = ii.next().map(|q| build_block(q, file)).transpose()?;
            Ok(Stmt::If { cond, then_b, else_b, span: stmt_span })
        }
        Rule::break_stmt => Ok(Stmt::Break { span: stmt_span }),
        Rule::continue_stmt => Ok(Stmt::Continue { span: stmt_span }),
        Rule::return_stmt => {
            let mut ii = first.into_inner();
            let _kw = ii.next();
            let expr = ii.next().map(|q| build_expr(q, file)).transpose()?;
            Ok(Stmt::Return { expr, span: stmt_span })
        }
        Rule::expr_stmt => {
            let e = build_expr(first.into_inner().next().unwrap(), file)?;
            Ok(Stmt::Expr { expr: e, span: stmt_span })
        }

        // 兼容：let/const 直接作为 stmt 的一部分（无 let_decl 包装）
        Rule::KW_LET | Rule::KW_CONST => {
            let is_const = first.as_rule() == Rule::KW_CONST;
            let name = it.next().ok_or_else(|| anyhow!("let: missing ident"))?.as_str().to_string();
            let ty   = build_ty(it.next().ok_or_else(|| anyhow!("let: missing type"))?)?;
            let init = build_expr(it.next().ok_or_else(|| anyhow!("let: missing init expr"))?, file)?;
            Ok(Stmt::Let { name, ty, init, is_const, span: stmt_span })
        }

        // 兼容：赋值语句无 assign_stmt 包装： ident "=" expr ";"
        Rule::ident => {
            if let Some(second) = it.next() {
                if second.as_rule() == Rule::expr {
                    let name = first.as_str().to_string();
                    let expr = build_expr(second, file)?;
                    return Ok(Stmt::Assign { name, expr, span: stmt_span });
                }
            }
            // 退化为普通表达式语句
            let mut again = p.into_inner();
            let only = again.next().unwrap();
            if only.as_rule() == Rule::expr {
                let e = build_expr(only, file)?;
                Ok(Stmt::Expr { expr: e, span: stmt_span })
            } else {
                Err(anyhow!("stmt starting with ident but not assign/expr"))
            }
        }

        // 兜底：如果第一个就是 expr，则为表达式语句
        Rule::expr => {
            let e = build_expr(first, file)?;
            Ok(Stmt::Expr { expr: e, span: stmt_span })
        }

        r => Err(anyhow!("build_stmt: unexpected first piece {:?}", r)),
    }
}

fn build_for_stmt(p: Pair<Rule>, file: FileId) -> Result<Stmt> {
    let sp = sp_of(&p, file);
    let mut init: Option<ForInit> = None;
    let mut cond: Option<Expr> = None;
    let mut step: Option<ForStep> = None;
    let mut body: Option<Block> = None;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::for_init => init = Some(build_for_init(x, file)?),
            Rule::for_step => step = Some(build_for_step(x, file)?),
            Rule::expr     => cond = Some(build_expr(x, file)?),
            Rule::block    => body = Some(build_block(x, file)?),
            _ => {}
        }
    }
    let body = body.ok_or_else(|| anyhow!("for_stmt: missing body"))?;
    Ok(Stmt::For { init, cond, step, body, span: sp })
}

fn build_for_init(p: Pair<Rule>, file: FileId) -> Result<ForInit> {
    let sp = sp_of(&p, file);
    let mut it = p.into_inner();
    let first = it.next().unwrap();
    Ok(match first.as_rule() {
        Rule::KW_LET | Rule::KW_CONST => {
            let is_const = first.as_rule() == Rule::KW_CONST;
            let name = it.next().unwrap().as_str().to_string();
            let ty   = build_ty(it.next().unwrap())?;
            let init = build_expr(it.next().unwrap(), file)?;
            ForInit::Let { name, ty, init, is_const, span: sp }
        }
        Rule::ident => {
            let name = first.as_str().to_string();
            let expr = build_expr(it.next().unwrap(), file)?;
            ForInit::Assign { name, expr, span: sp }
        }
        Rule::expr => {
            let e = build_expr(first, file)?;
            ForInit::Expr(e, sp)
        }
        r => return Err(anyhow!("unexpected for_init: {:?}", r)),
    })
}

fn build_for_step(p: Pair<Rule>, file: FileId) -> Result<ForStep> {
    let sp = sp_of(&p, file);
    let mut it = p.into_inner();
    let first = it.next().unwrap();
    Ok(match first.as_rule() {
        Rule::ident => {
            let name = first.as_str().to_string();
            let expr = build_expr(it.next().unwrap(), file)?;
            ForStep::Assign { name, expr, span: sp }
        }
        Rule::expr => {
            let e = build_expr(first, file)?;
            ForStep::Expr(e, sp)
        }
        r => return Err(anyhow!("unexpected for_step: {:?}", r)),
    })
}