use crate::ast::*;
use anyhow::{Result, anyhow};
use pest::Parser;
use pest::iterators::{Pair, Pairs};

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

fn build_fun(p: Pair<Rule>) -> Result<FunDecl> {
    let mut it = p.into_inner();

    // 1) fn 关键字
    match it.next().map(|x| x.as_rule()) {
        Some(Rule::KW_FNs) => {}
        other => return Err(anyhow!("fun_decl: expect `fn`, got {:?}", other)),
    }

    // 2) 标识符
    let name = it
        .next()
        .ok_or_else(|| anyhow!("fun_decl: missing function name"))?;
    if name.as_rule() != Rule::ident {
        return Err(anyhow!("fun_decl: expect ident, got {:?}", name.as_rule()));
    }
    let name = name.as_str().to_string();

    // 3) 可选参数列表（注意：括号不会出现）
    let mut params = Vec::new();
    let next = it
        .next()
        .ok_or_else(|| anyhow!("fun_decl: missing return type or params"))?;
    let after_params = if next.as_rule() == Rule::param_list {
        params = build_params(next)?;
        // 读下一个：应该是 ty
        it.next()
            .ok_or_else(|| anyhow!("fun_decl: missing return type after params"))?
    } else {
        // 没有参数列表，则 next 就是 ty
        next
    };

    // 4) 返回类型
    if after_params.as_rule() != Rule::ty {
        return Err(anyhow!(
            "fun_decl: expect return type, got {:?}",
            after_params.as_rule()
        ));
    }
    let ret = build_ty(after_params)?;

    // 5) 函数体
    let body_pair = it
        .next()
        .ok_or_else(|| anyhow!("fun_decl: missing function body block"))?;
    if body_pair.as_rule() != Rule::block {
        return Err(anyhow!(
            "fun_decl: expect block, got {:?}",
            body_pair.as_rule()
        ));
    }
    let body = build_block(body_pair)?;

    Ok(FunDecl {
        name,
        params,
        ret,
        body,
        is_extern: false,
    })
}

fn build_item(p: Pair<Rule>) -> Result<Item> {
    Ok(match p.as_rule() {
        Rule::let_decl => {
            let mut it = p.into_inner();
            let kw = it.next().unwrap(); // KW_LET | KW_CONST（这两个是命名规则）
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

            // 名称
            let name = it
                .next()
                .ok_or_else(|| anyhow!("extern_fun: missing name"))?;
            if name.as_rule() != Rule::ident {
                return Err(anyhow!(
                    "extern_fun: expect ident, got {:?}",
                    name.as_rule()
                ));
            }
            let name = name.as_str().to_string();

            // 可选参数列表
            let mut params = Vec::new();
            let next = it
                .next()
                .ok_or_else(|| anyhow!("extern_fun: missing return type or params"))?;
            let after_params = if next.as_rule() == Rule::param_list {
                params = build_params(next)?;
                it.next()
                    .ok_or_else(|| anyhow!("extern_fun: missing return type after params"))?
            } else {
                next
            };

            // 返回类型
            if after_params.as_rule() != Rule::ty {
                return Err(anyhow!(
                    "extern_fun: expect return type, got {:?}",
                    after_params.as_rule()
                ));
            }
            let ret = build_ty(after_params)?;

            // 末尾有 `;` 作为字面量，不会出现在 into_inner()，因此不消费

            Item::Fun(FunDecl {
                name,
                params,
                ret,
                body: Block {
                    stmts: vec![],
                    tail: None,
                },
                is_extern: true,
            })
        }
        _ => unreachable!(),
    })
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
        Rule::while_stmt => {
            let mut it = p.into_inner();
            let _kw = it.next().unwrap();
            let _lp = it.next().unwrap();
            let cond = build_expr(it.next().unwrap())?;
            let _rp = it.next().unwrap();
            let body = build_block(it.next().unwrap())?;
            Stmt::While { cond, body }
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
        Rule::expr_stmt => {
            let e = build_expr(p.into_inner().next().unwrap())?;
            Stmt::Expr(e)
        }
        _ => unreachable!(),
    })
}

fn build_if(p: Pair<Rule>) -> Result<Expr> {
    let mut it = p.into_inner();

    // KW_IF
    let kw_if = it.next().ok_or_else(|| anyhow!("if_expr: missing `if`"))?;
    if kw_if.as_rule() != Rule::KW_IF {
        return Err(anyhow!("if_expr: expected `if`, got {:?}", kw_if.as_rule()));
    }

    // 条件 expr（注意没有括号节点）
    let cond_p = it.next().ok_or_else(|| anyhow!("if_expr: missing condition expr"))?;
    if cond_p.as_rule() != Rule::expr {
        return Err(anyhow!("if_expr: expected expr after `if`, got {:?}", cond_p.as_rule()));
    }
    let cond = build_expr(cond_p)?;

    // then 块
    let then_p = it.next().ok_or_else(|| anyhow!("if_expr: missing then block"))?;
    if then_p.as_rule() != Rule::block {
        return Err(anyhow!("if_expr: expected then block, got {:?}", then_p.as_rule()));
    }
    let then_b = build_block(then_p)?;

    // KW_ELSE
    let kw_else = it.next().ok_or_else(|| anyhow!("if_expr: missing `else`"))?;
    if kw_else.as_rule() != Rule::KW_ELSE {
        return Err(anyhow!("if_expr: expected `else`, got {:?}", kw_else.as_rule()));
    }

    // else 块
    let else_p = it.next().ok_or_else(|| anyhow!("if_expr: missing else block"))?;
    if else_p.as_rule() != Rule::block {
        return Err(anyhow!("if_expr: expected else block, got {:?}", else_p.as_rule()));
    }
    let else_b = build_block(else_p)?;

    Ok(Expr::If { cond: Box::new(cond), then_b, else_b })
}

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
                Rule::OP_NOT => Expr::Unary {
                    op: UnOp::Not,
                    rhs: Box::new(build_expr(it.next().unwrap())?),
                },
                Rule::OP_SUB => Expr::Unary {
                    op: UnOp::Neg,
                    rhs: Box::new(build_expr(it.next().unwrap())?),
                },
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
                    _ => unreachable!(),
                }
            }
            e
        }
        Rule::primary => build_expr(p.into_inner().next().unwrap())?,
        Rule::group => build_expr(p.into_inner().next().unwrap())?,
        Rule::if_expr => build_if(p)?,
        Rule::int_lit => Expr::Int(p.as_str().parse::<i64>()?),
        Rule::bool_lit => Expr::Bool(p.as_str() == "true"),
        Rule::string_lit => Expr::Str(unescape_string(p.as_str())),
        Rule::ident => Expr::Var(p.as_str().to_string()),
        Rule::block => Expr::Block(build_block(p)?),
        r => return Err(anyhow!("unexpected expr rule: {:?}", r)),
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
        lhs = Expr::Binary {
            op: bop,
            lhs: Box::new(lhs),
            rhs: Box::new(rhs),
        };
    }
    Ok(lhs)
}

fn unescape_string(s: &str) -> String {
    // 输入形如 "...."
    let bytes = s.as_bytes();
    let mut out = String::new();
    let mut i = 1; // 跳过开头 "
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
                _ => out.push(e),
            }
        } else {
            out.push(c);
        }
        i += 1;
    }
    out
}
