use crate::ast::*;
use anyhow::{anyhow, Result};
use pest::iterators::Pair;
use pest::Parser;

#[derive(pest_derive::Parser)]
#[grammar = "grammar/grammar.pest"]
pub struct PawParser;

pub fn parse_program(src: &str) -> Result<Program> {
    let pairs = PawParser::parse(Rule::program, src)?;
    let mut items = Vec::new();
    for p in pairs {
        match p.as_rule() {
            Rule::program => {
                for it in p.into_inner() {
                    if it.as_rule() == Rule::item {
                        items.push(build_item(it.into_inner().next().unwrap())?);
                    }
                }
            }
            _ => {}
        }
    }
    Ok(Program { items })
}

/* ---------------- item / function ---------------- */

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

        Rule::fun_decl => Item::Fun(build_fun(p)?),

        Rule::extern_fun => {
            let mut it = p.into_inner();

            // extern fn
            match it.next().map(|x| x.as_rule()) {
                Some(Rule::KW_EXTERN) => {}
                other => return Err(anyhow!("extern_fun: expect `extern`, got {:?}", other)),
            }
            match it.next().map(|x| x.as_rule()) {
                Some(Rule::KW_FNs) => {}
                other => return Err(anyhow!("extern_fun: expect `fn`, got {:?}", other)),
            }

            // name
            let name = it.next().ok_or_else(|| anyhow!("extern_fun: missing name"))?;
            if name.as_rule() != Rule::ident {
                return Err(anyhow!("extern_fun: expect ident, got {:?}", name.as_rule()));
            }
            let name = name.as_str().to_string();

            // params?
            let mut params = Vec::new();
            let next = it.next().ok_or_else(|| anyhow!("extern_fun: missing return type or params"))?;
            let after_params = if next.as_rule() == Rule::param_list {
                params = build_params(next)?;
                it.next().ok_or_else(|| anyhow!("extern_fun: missing return type after params"))?
            } else { next };

            // return type
            if after_params.as_rule() != Rule::ty {
                return Err(anyhow!("extern_fun: expect return type, got {:?}", after_params.as_rule()));
            }
            let ret = build_ty(after_params)?;

            Item::Fun(FunDecl {
                name,
                params,
                ret,
                body: Block { stmts: vec![], tail: None },
                is_extern: true,
            })
        }

        Rule::import_decl => {
            let mut it = p.into_inner();
            let kw = it.next().ok_or_else(|| anyhow!("import_decl: missing `import`"))?;
            if kw.as_rule() != Rule::KW_IMPORT {
                return Err(anyhow!("import_decl: expected `import`, got {:?}", kw.as_rule()));
            }
            let lit = it.next().ok_or_else(|| anyhow!("import_decl: missing string literal"))?;
            if lit.as_rule() != Rule::string_lit {
                return Err(anyhow!("import_decl: expected string_lit, got {:?}", lit.as_rule()));
            }
            let spec = unescape_string(lit.as_str());
            Item::Import(spec)
        }

        _ => unreachable!(),
    })
}

fn build_fun(p: Pair<Rule>) -> Result<FunDecl> {
    let mut it = p.into_inner();

    // fn
    match it.next().map(|x| x.as_rule()) {
        Some(Rule::KW_FNs) => {}
        other => return Err(anyhow!("fun_decl: expect `fn`, got {:?}", other)),
    }

    // name
    let name = it
        .next()
        .ok_or_else(|| anyhow!("fun_decl: missing function name"))?;
    if name.as_rule() != Rule::ident {
        return Err(anyhow!("fun_decl: expect ident, got {:?}", name.as_rule()));
    }
    let name = name.as_str().to_string();

    // params?
    let mut params = Vec::new();
    let next = it
        .next()
        .ok_or_else(|| anyhow!("fun_decl: missing return type or params"))?;
    let after_params = if next.as_rule() == Rule::param_list {
        params = build_params(next)?;
        it.next()
            .ok_or_else(|| anyhow!("fun_decl: missing return type after params"))?
    } else {
        next
    };

    // return type
    if after_params.as_rule() != Rule::ty {
        return Err(anyhow!(
            "fun_decl: expect return type, got {:?}",
            after_params.as_rule()
        ));
    }
    let ret = build_ty(after_params)?;

    // body
    let body_pair = it
        .next()
        .ok_or_else(|| anyhow!("fun_decl: missing function body block"))?;
    if body_pair.as_rule() != Rule::block {
        return Err(anyhow!("fun_decl: expect block, got {:?}", body_pair.as_rule()));
    }
    let body = build_block(body_pair)?;

    Ok(FunDecl { name, params, ret, body, is_extern: false })
}

fn build_params(p: Pair<Rule>) -> Result<Vec<(String, Ty)>> {
    let mut v = Vec::new();
    for x in p.into_inner() {
        // x: param
        let mut it = x.into_inner(); // ident ":" ty
        let name = it.next().unwrap().as_str().to_string();
        let ty = build_ty(it.next().unwrap())?;
        v.push((name, ty));
    }
    Ok(v)
}

fn build_ty(p: Pair<Rule>) -> Result<Ty> {
    Ok(match p.as_rule() {
        Rule::KW_Int => Ty::Int,
        Rule::KW_Bool => Ty::Bool,
        Rule::KW_String => Ty::String,
        Rule::ty => build_ty(p.into_inner().next().unwrap())?,
        r => return Err(anyhow!("unexpected ty rule: {:?}", r)),
    })
}

/* ---------------- blocks / statements ---------------- */

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
            Stmt::Let { name, ty, init, is_const: kw.as_rule() == Rule::KW_CONST }
        }

        Rule::assign_stmt => {
            let mut it = p.into_inner(); // ident "=" expr
            let name = it.next().unwrap().as_str().to_string();
            let expr = build_expr(it.next().unwrap())?;
            Stmt::Assign { name, expr }
        }

        Rule::while_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next().unwrap();
            let _lp = it.next().unwrap();
            let cond = build_expr(it.next().unwrap())?;
            let _rp = it.next().unwrap();
            let body = build_block(it.next().unwrap())?;
            Stmt::While { cond, body }
        }

        Rule::if_stmt => {
            let mut it = p.into_inner();

            // KW_IF
            let kw_if = it.next().ok_or_else(|| anyhow!("if_stmt: missing `if`"))?;
            if kw_if.as_rule() != Rule::KW_IF {
                return Err(anyhow!("if_stmt: expected `if`, got {:?}", kw_if.as_rule()));
            }

            // 条件 expr
            let cond_p = it.next().ok_or_else(|| anyhow!("if_stmt: missing condition expr"))?;
            if cond_p.as_rule() != Rule::expr {
                return Err(anyhow!("if_stmt: expected expr after `if`, got {:?}", cond_p.as_rule()));
            }
            let cond = build_expr(cond_p)?;

            // then 块
            let then_p = it.next().ok_or_else(|| anyhow!("if_stmt: missing then block"))?;
            if then_p.as_rule() != Rule::block {
                return Err(anyhow!("if_stmt: expected then block, got {:?}", then_p.as_rule()));
            }
            let then_b = build_block(then_p)?;

            // 可选 else
            let mut else_b = Block { stmts: vec![], tail: None };
            if let Some(next) = it.next() {
                if next.as_rule() == Rule::KW_ELSE {
                    let blk = it.next().ok_or_else(|| anyhow!("if_stmt: missing else block"))?;
                    if blk.as_rule() != Rule::block {
                        return Err(anyhow!("if_stmt: expected else block, got {:?}", blk.as_rule()));
                    }
                    else_b = build_block(blk)?;
                } else {
                    // 若 grammar 把多余节点放进来，给出提示
                    return Err(anyhow!("if_stmt: unexpected trailing node {:?}", next.as_rule()));
                }
            }

            Stmt::Expr(Expr::If {
                cond: Box::new(cond),
                then_b,
                else_b,
            })
        }

        // ✅ 新增：for_stmt
        // 语法：for "(" for_init? ";" expr? ";" for_step? ")" block
        // 注意：分号/括号是否出现在 into_inner() 里取决于你的 grammar，
        // 这里不依赖它们，按顺序尝试消费 for_init / expr / for_step，最后拿 block。
        Rule::for_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next(); // KW_FOR（可能存在）
            let _lp = it.next(); // "("（可能存在）

            let init = match it.peek().map(|x| x.as_rule()) {
                Some(Rule::for_init) => { let v = build_for_init(it.next().unwrap())?; v.into() }
                _ => None,
            };

            let cond = match it.peek().map(|x| x.as_rule()) {
                Some(Rule::expr) => { let v = build_expr(it.next().unwrap())?; v.into() }
                _ => None,
            };

            let step = match it.peek().map(|x| x.as_rule()) {
                Some(Rule::for_step) => { let v = build_for_step(it.next().unwrap())?; v.into() }
                _ => None,
            };

            // 现在应到达 body（如果 grammar 把 ")" 也作为节点，你可能需要再 it.next() 丢弃一次）
            let maybe = it.next().ok_or_else(|| anyhow!("for_stmt: missing body block"))?;
            let body = if maybe.as_rule() == Rule::block {
                build_block(maybe)?
            } else {
                // 尝试再取一个（兼容出现 RPAREN 的情况）
                let body_pair = it.next().ok_or_else(|| anyhow!("for_stmt: missing body block"))?;
                if body_pair.as_rule() != Rule::block {
                    return Err(anyhow!("for_stmt: expected block, got {:?}", body_pair.as_rule()));
                }
                build_block(body_pair)?
            };

            Stmt::For { init, cond, step, body }
        }

        Rule::return_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next().unwrap();
            let maybe = it.next();
            let expr = if let Some(pe) = maybe.filter(|x| x.as_rule() == Rule::expr) {
                Some(build_expr(pe)?)
            } else {
                None
            };
            Stmt::Return(expr)
        }

        Rule::break_stmt => Stmt::Break,
        Rule::continue_stmt => Stmt::Continue,

        Rule::expr_stmt => {
            let e = build_expr(p.into_inner().next().unwrap())?;
            Stmt::Expr(e)
        }

        other => return Err(anyhow!("build_stmt: unexpected rule {:?}", other)),
    })
}


/* ---------------- if / for / match helpers ---------------- */

fn build_if(p: Pair<Rule>) -> Result<Expr> {
    let mut it = p.into_inner();

    // if
    let kw_if = it.next().ok_or_else(|| anyhow!("if_expr: missing `if`"))?;
    if kw_if.as_rule() != Rule::KW_IF {
        return Err(anyhow!("if_expr: expected `if`, got {:?}", kw_if.as_rule()));
    }

    // condition (no paren node in grammar after rewrite)
    let cond_p = it.next().ok_or_else(|| anyhow!("if_expr: missing condition expr"))?;
    if cond_p.as_rule() != Rule::expr {
        return Err(anyhow!("if_expr: expected expr after `if`, got {:?}", cond_p.as_rule()));
    }
    let cond = build_expr(cond_p)?;

    // then block
    let then_p = it.next().ok_or_else(|| anyhow!("if_expr: missing then block"))?;
    if then_p.as_rule() != Rule::block {
        return Err(anyhow!("if_expr: expected then block, got {:?}", then_p.as_rule()));
    }
    let then_b = build_block(then_p)?;

    // optional else
    let else_b = if let Some(next) = it.next() {
        if next.as_rule() != Rule::KW_ELSE {
            return Err(anyhow!("if_expr: expected `else`, got {:?}", next.as_rule()));
        }
        let else_p = it.next().ok_or_else(|| anyhow!("if_expr: missing else block"))?;
        if else_p.as_rule() != Rule::block {
            return Err(anyhow!("if_expr: expected else block, got {:?}", else_p.as_rule()));
        }
        build_block(else_p)?
    } else {
        Block { stmts: vec![], tail: None }
    };

    Ok(Expr::If { cond: Box::new(cond), then_b, else_b })
}

fn build_for(p: Pair<Rule>) -> Result<Stmt> {
    // for_stmt = { KW_FOR ~ "(" ~ for_init? ~ ";" ~ expr? ~ ";" ~ for_step? ~ ")" ~ block }
    let mut init: Option<ForInit> = None;
    let mut cond: Option<Expr> = None;
    let mut step: Option<ForStep> = None;
    let mut body: Option<Block> = None;

    for n in p.into_inner() {
        match n.as_rule() {
            Rule::for_init => init = Some(build_for_init(n)?),
            Rule::expr     => cond = Some(build_expr(n)?),
            Rule::for_step => step = Some(build_for_step(n)?),
            Rule::block    => body = Some(build_block(n)?),
            _ => { /* 忽略 KW_FOR / 括号 / 分号等字面量 */ }
        }
    }

    Ok(Stmt::For {
        init,
        cond,
        step,
        body: body.ok_or_else(|| anyhow::anyhow!("for_stmt: missing body block"))?,
    })
}

fn build_for_init(p: Pair<Rule>) -> Result<ForInit> {
    // for_init = {
    //   (KW_LET | KW_CONST) ~ ident ~ ":" ~ ty ~ "=" ~ expr
    // | ident ~ "=" ~ expr
    // | expr
    // }
    let mut it = p.into_inner();
    let first = it.next().ok_or_else(|| anyhow!("empty for_init"))?;

    Ok(match first.as_rule() {
        // 1) let/const 形式（注意：这里不会出现分号）
        Rule::KW_LET | Rule::KW_CONST => {
            let is_const = first.as_rule() == Rule::KW_CONST;

            let name = it
                .next()
                .ok_or_else(|| anyhow!("for_init let: missing name"))?;
            if name.as_rule() != Rule::ident {
                return Err(anyhow!("for_init let: expect ident, got {:?}", name.as_rule()));
            }
            let name = name.as_str().to_string();

            let ty_pair = it
                .next()
                .ok_or_else(|| anyhow!("for_init let: missing type"))?;
            let ty = build_ty(ty_pair)?;

            let init_pair = it
                .next()
                .ok_or_else(|| anyhow!("for_init let: missing init expr"))?;
            let init = build_expr(init_pair)?;

            ForInit::Let { name, ty, init, is_const }
        }

        // 2) 赋值（ident "=" expr）
        // 注意：字面量 '=' 不会出现在 into_inner() 中，这里会拿到 [ident, expr]
        Rule::ident => {
            let name = first.as_str().to_string();
            let expr_pair = it
                .next()
                .ok_or_else(|| anyhow!("for_init assign: missing expr"))?;
            let expr = build_expr(expr_pair)?;
            ForInit::Assign { name, expr }
        }

        // 3) 纯表达式
        Rule::expr => ForInit::Expr(build_expr(first)?),

        other => return Err(anyhow!("unexpected for_init node: {:?}", other)),
    })
}

fn build_for_step(p: Pair<Rule>) -> Result<ForStep> {
    // for_step = { ident ~ "=" ~ expr | expr }
    let mut it = p.into_inner();
    let first = it.next().ok_or_else(|| anyhow!("empty for_step"))?;
    Ok(match first.as_rule() {
        Rule::ident => {
            let name = first.as_str().to_string();
            let expr_pair = it
                .next()
                .ok_or_else(|| anyhow!("for_step assign: missing expr"))?;
            let expr = build_expr(expr_pair)?;
            ForStep::Assign { name, expr }
        }
        Rule::expr => ForStep::Expr(build_expr(first)?),
        other => return Err(anyhow!("unexpected for_step node: {:?}", other)),
    })
}

fn build_match(p: Pair<Rule>) -> Result<Expr> {
    // grammar: match_expr = { KW_MATCH ~ "(" ~ expr ~ ")" ~ "{" ~ match_arm* ~ match_default? ~ "}" }
    // 这里只消费命名子规则：expr / match_arm / match_default
    let mut scrut: Option<Expr> = None;
    let mut arms: Vec<(Pattern, Block)> = Vec::new();
    let mut default: Option<Block> = None;

    for n in p.into_inner() {
        match n.as_rule() {
            Rule::expr => {
                scrut = Some(build_expr(n)?);
            }
            Rule::match_arm => {
                // match_arm = { pattern ~ "=>" ~ block }
                let mut ai = n.into_inner();
                let pat = build_pattern(ai.next().ok_or_else(|| anyhow!("match_arm: missing pattern"))?)?;
                let blk = build_block(ai.next().ok_or_else(|| anyhow!("match_arm: missing block"))?)?;
                arms.push((pat, blk));
            }
            Rule::match_default => {
                // match_default = { KW_DEFAULT ~ "=>" ~ block }
                let mut di = n.into_inner();
                let _kw = di.next().ok_or_else(|| anyhow!("match_default: missing `default`"))?;
                let blk = build_block(di.next().ok_or_else(|| anyhow!("match_default: missing block"))?)?;
                default = Some(blk);
            }
            _ => { /* 忽略字面量 */ }
        }
    }

    let scrut = Box::new(scrut.ok_or_else(|| anyhow!("match_expr: missing scrutinee expr"))?);
    Ok(Expr::Match { scrut, arms, default })
}


fn build_pattern(p: Pair<Rule>) -> Result<Pattern> {
    Ok(match p.as_rule() {
        Rule::pattern => build_pattern(p.into_inner().next().ok_or_else(|| anyhow!("empty pattern"))?)?,
        Rule::int_lit => Pattern::Int(p.as_str().parse::<i64>()?),
        Rule::bool_lit => Pattern::Bool(p.as_str() == "true"),
        Rule::wild_pat => Pattern::Wild,
        r => return Err(anyhow!("unexpected pattern rule: {:?}", r)),
    })
}

/* ---------------- expressions ---------------- */

fn build_expr(p: Pair<Rule>) -> Result<Expr> {
    Ok(match p.as_rule() {
        Rule::expr => build_expr(p.into_inner().next().unwrap())?,
        Rule::logic_or
        | Rule::logic_and
        | Rule::equality
        | Rule::compare
        | Rule::add
        | Rule::mult => fold_binary(p)?,

        Rule::unary => {
            let mut it = p.into_inner();
            let first = it.next().unwrap();
            match first.as_rule() {
                Rule::OP_NOT => Expr::Unary { op: UnOp::Not, rhs: Box::new(build_expr(it.next().unwrap())?) },
                Rule::OP_SUB => Expr::Unary { op: UnOp::Neg, rhs: Box::new(build_expr(it.next().unwrap())?) },
                _ => build_expr(first)?,
            }
        }

        Rule::postfix => {
            let mut it = p.into_inner();
            let mut e = build_expr(it.next().unwrap())?;
            for suf in it {
                match suf.as_rule() {
                    Rule::call_suffix => {
                        let mut args = Vec::new();
                        let mut ii = suf.into_inner();
                        if let Some(al) = ii.next() {
                            for ae in al.into_inner() {
                                args.push(build_expr(ae)?);
                            }
                        }
                        match e {
                            Expr::Var(name) => e = Expr::Call { callee: name, args },
                            _ => return Err(anyhow!("call on non-ident")),
                        }
                    }
                    other => return Err(anyhow!("postfix: unexpected suffix {:?}", other)),
                }
            }
            e
        }

        Rule::primary => build_expr(p.into_inner().next().unwrap())?,
        Rule::group   => build_expr(p.into_inner().next().unwrap())?,
        Rule::if_expr => build_if(p)?,

        // ✅ 新增：match_expr
        Rule::match_expr => build_match(p)?,

        Rule::int_lit    => Expr::Int(p.as_str().parse::<i64>()?),
        Rule::bool_lit   => Expr::Bool(p.as_str() == "true"),
        Rule::string_lit => Expr::Str(unescape_string(p.as_str())),
        Rule::ident      => Expr::Var(p.as_str().to_string()),
        Rule::block      => Expr::Block(build_block(p)?),

        other => return Err(anyhow!("unexpected expr rule: {:?}", other)),
    })
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

/* ---------------- utils ---------------- */

fn unescape_string(s: &str) -> String {
    // 输入形如 "...."
    let bytes = s.as_bytes();
    let mut out = String::new();
    let mut i = 1; // 跳过开头 "
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
                _ => out.push(e),
            }
        } else {
            out.push(c);
        }
        i += 1;
    }
    out
}
