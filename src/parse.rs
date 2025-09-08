use anyhow::{Result, bail};
use pest::Parser;
use pest::iterators::Pair;
use pest_derive::Parser;

use crate::ast::*;

#[derive(Parser)]
#[grammar = "grammar/grammar.pest"]
struct PawLangParser;

#[derive(thiserror::Error, Debug)]
pub enum ParseError {
    #[error("unexpected rule: {0:?}")]
    UnexpectedRule(Rule),
    #[error("message: {0}")]
    Msg(String),
}

pub fn parse_program(src: &str) -> Result<(Vec<FunDecl>, Vec<Stmt>)> {
    let mut pairs = PawLangParser::parse(Rule::program, src)?;
    let program = pairs.next().unwrap();
    let mut funs = Vec::new();
    let mut top_stmts = Vec::new();

    for item in program.into_inner() {
        match item.as_rule() {
            Rule::item => {
                let mut inner = item.into_inner();
                let first = inner.next().unwrap();
                match first.as_rule() {
                    Rule::fun_decl => funs.push(build_fun(first)?),
                    Rule::let_decl => top_stmts.push(build_let(first)?),
                    _ => unreachable!(),
                }
            }
            _ => {}
        }
    }
    Ok((funs, top_stmts))
}

// ---- builders ----
fn build_fun(p: Pair<Rule>) -> Result<FunDecl> {
    let mut it = p.into_inner().peekable();

    // 先吃掉 fn 关键字（因为 KW_FNs 是可见节点）
    if let Some(peek) = it.peek() {
        if peek.as_rule() == Rule::KW_FNs {
            it.next();
        }
    }

    // 函数名
    let name = match it.next() {
        Some(pair) if pair.as_rule() == Rule::ident => pair.as_str().to_string(),
        other => bail!(
            "fun_decl: expect ident after 'fn', got {:?}",
            other.map(|x| x.as_rule())
        ),
    };

    // 读取参数：可能是 param_list，可能是一串 param，或者无参直接到返回类型
    let mut params: Vec<(String, Ty)> = Vec::new();
    match it.peek().map(|q| q.as_rule()) {
        Some(Rule::param_list) => {
            params = build_params(it.next().unwrap())?;
        }
        Some(Rule::param) => {
            while let Some(peek) = it.peek() {
                if peek.as_rule() != Rule::param {
                    break;
                }
                let pr = it.next().unwrap();
                let mut ip = pr.into_inner();
                let pname = ip.next().unwrap().as_str().to_string();
                let pty = build_ty(ip.next().unwrap())?;
                params.push((pname, pty));
            }
        }
        _ => { /* 无参 */ }
    }

    // 返回类型（兼容 ty 或直接 KW_Int/KW_Bool）
    let ret_ty_pair = match it.next() {
        Some(pair)
            if pair.as_rule() == Rule::ty
                || pair.as_rule() == Rule::KW_Int
                || pair.as_rule() == Rule::KW_Bool =>
        {
            pair
        }
        other => bail!(
            "fun_decl: expect return type, got {:?}",
            other.map(|x| x.as_rule())
        ),
    };
    let ret = build_ty(ret_ty_pair)?;

    // 函数体 block
    let body_pair = match it.next() {
        Some(pair) if pair.as_rule() == Rule::block => pair,
        other => bail!(
            "fun_decl: expect block after return type, got {:?}",
            other.map(|x| x.as_rule())
        ),
    };
    let body = build_block(body_pair)?;

    Ok(FunDecl {
        name,
        params,
        ret,
        body,
    })
}

fn build_params(p: Pair<Rule>) -> Result<Vec<(String, Ty)>> {
    let mut v = Vec::new();
    for pr in p.into_inner() {
        if pr.as_rule() == Rule::param {
            let mut it = pr.into_inner();
            let name = it.next().unwrap().as_str().to_string();
            let ty = build_ty(it.next().unwrap())?;
            v.push((name, ty));
        }
    }
    Ok(v)
}

fn build_ty(p: Pair<Rule>) -> Result<Ty> {
    Ok(match p.as_rule() {
        Rule::KW_Int => Ty::Int,
        Rule::KW_Bool => Ty::Bool,
        Rule::ty => {
            // ty = { KW_Int | KW_Bool } —— 里面会有一个可见的 KW_*
            let kw = p.into_inner().next().unwrap().as_rule();
            match kw {
                Rule::KW_Int => Ty::Int,
                Rule::KW_Bool => Ty::Bool,
                other => bail!("unknown type token in ty: {:?}", other),
            }
        }
        other => bail!("expect type, got {:?}", other),
    })
}

fn build_block(p: Pair<Rule>) -> Result<Block> {
    let mut stmts = Vec::new();
    let mut tail = None;
    for it in p.into_inner() {
        match it.as_rule() {
            Rule::stmt => {
                let inner = it.into_inner().next().unwrap();
                match inner.as_rule() {
                    Rule::let_decl => stmts.push(build_let(inner)?),
                    Rule::while_stmt => stmts.push(build_while(inner)?),
                    Rule::return_stmt => stmts.push(build_return(inner)?),
                    Rule::expr_stmt => {
                        stmts.push(Stmt::Expr(build_expr(inner.into_inner().next().unwrap())?))
                    }
                    _ => unreachable!(),
                }
            }
            Rule::tail_expr => {
                let expr_pair = it.into_inner().next().unwrap();
                tail = Some(Box::new(build_expr(expr_pair)?));
            }
            _ => {}
        }
    }
    Ok(Block { stmts, tail })
}

fn build_let(p: Pair<Rule>) -> Result<Stmt> {
    // ( KW_LET | KW_CONST ) ~ ident ~ ":" ~ ty ~ "=" ~ expr ~ ";"
    let mut it = p.into_inner();

    let kw_rule = it.next().unwrap().as_rule(); // KW_LET or KW_CONST
    let is_const = matches!(kw_rule, Rule::KW_CONST);

    let name = it.next().unwrap().as_str().to_string();
    let ty = build_ty(it.next().unwrap())?;
    let init = build_expr(it.next().unwrap())?;

    Ok(Stmt::Let {
        name,
        ty,
        init,
        is_const,
    })
}

fn build_return(p: Pair<Rule>) -> Result<Stmt> {
    let mut it = p.into_inner();
    let e = it.next().map(build_expr).transpose()?;
    Ok(Stmt::Return(e))
}

fn build_while(p: Pair<Rule>) -> Result<Stmt> {
    // while "(" expr ")" block
    let mut it = p.into_inner();
    let cond = build_expr(it.next().unwrap())?;
    let body = build_block(it.next().unwrap())?;
    Ok(Stmt::While { cond, body })
}

fn build_expr(p: Pair<Rule>) -> Result<Expr> {
    match p.as_rule() {
        Rule::expr => build_expr(p.into_inner().next().unwrap()),
        Rule::logic_or
        | Rule::logic_and
        | Rule::equality
        | Rule::compare
        | Rule::add
        | Rule::mult => fold_binary(p),
        Rule::unary => build_unary(p),
        Rule::postfix => build_postfix(p),
        Rule::primary => build_primary(p),
        _ => bail!("unexpected expr node: {:?}", p.as_rule()),
    }
}

fn build_unary(p: Pair<Rule>) -> Result<Expr> {
    let mut it = p.into_inner();
    let first = it.next().unwrap();
    match first.as_rule() {
        Rule::OP_NOT => Ok(Expr::Unary {
            op: UnOp::Not,
            rhs: Box::new(build_expr(it.next().unwrap())?),
        }),
        Rule::OP_SUB => Ok(Expr::Unary {
            op: UnOp::Neg,
            rhs: Box::new(build_expr(it.next().unwrap())?),
        }),
        _ => build_postfix(first),
    }
}

fn build_postfix(p: Pair<Rule>) -> Result<Expr> {
    // primary ( "(" args? ")" )*
    let mut it = p.into_inner();
    let mut node = build_primary(it.next().unwrap())?;
    for suf in it {
        match suf.as_rule() {
            Rule::call_suffix => {
                let mut args = Vec::new();
                if let Some(al) = suf.into_inner().next() {
                    // arg_list?
                    for e in al.into_inner() {
                        args.push(build_expr(e)?);
                    }
                }
                node = match node {
                    Expr::Var(name) => Expr::Call { callee: name, args },
                    _ => bail!("call target must be an identifier for now"),
                };
            }
            _ => {}
        }
    }
    Ok(node)
}

fn build_primary(p: Pair<Rule>) -> Result<Expr> {
    let mut it = p.clone().into_inner();
    match p.as_rule() {
        Rule::primary => build_primary(it.next().unwrap()),
        Rule::int_lit => Ok(Expr::Int(p.as_str().parse::<i64>()?)),
        Rule::bool_lit => Ok(Expr::Bool(matches!(p.as_str(), "true"))),
        Rule::ident => Ok(Expr::Var(p.as_str().to_string())),
        Rule::group => build_expr(it.next().unwrap()),
        Rule::block => Ok(Expr::Block(build_block(p)?)),
        Rule::if_expr => {
            // if "(" cond ")" block else block
            let mut ii = p.into_inner();
            let cond = build_expr(ii.next().unwrap())?;
            let then_b = build_block(ii.next().unwrap())?;
            let else_b = build_block(ii.next().unwrap())?;
            Ok(Expr::If {
                cond: Box::new(cond),
                then_b: Box::new(then_b),
                else_b,
            })
        }
        _ => bail!("unexpected primary: {:?}", p.as_rule()),
    }
}

fn binop_of(rule: Rule) -> BinOp {
    match rule {
        Rule::OP_OR => BinOp::Or,
        Rule::OP_AND => BinOp::And,
        Rule::OP_EQ => BinOp::Eq,
        Rule::OP_NE => BinOp::Ne,
        Rule::OP_LT => BinOp::Lt,
        Rule::OP_LE => BinOp::Le,
        Rule::OP_GT => BinOp::Gt,
        Rule::OP_GE => BinOp::Ge,
        Rule::OP_ADD => BinOp::Add,
        Rule::OP_SUB => BinOp::Sub,
        Rule::OP_MUL => BinOp::Mul,
        Rule::OP_DIV => BinOp::Div,
        _ => unreachable!(),
    }
}

fn fold_binary(p: Pair<Rule>) -> Result<Expr> {
    // 规则形如: level = lower ~ (op ~ lower)*
    let mut it = p.clone().into_inner();
    let first = it.next().ok_or_else(|| {
        anyhow::anyhow!(
            "empty expression at {:?}",
            p.as_span().start_pos().line_col()
        )
    })?;
    let mut node = build_expr(first)?;

    while let Some(op_or) = it.next() {
        // 取 rhs，如果没有就报错（而不是 unwrap panic）
        let rhs_pair = match it.next() {
            Some(r) => r,
            None => {
                let (line, col) = op_or.as_span().start_pos().line_col();
                bail!(
                    "binary operator {:?} missing rhs at {}:{}",
                    op_or.as_rule(),
                    line,
                    col
                );
            }
        };

        // 只有这些规则才是合法的二元运算符
        let op = match op_or.as_rule() {
            Rule::OP_OR
            | Rule::OP_AND
            | Rule::OP_EQ
            | Rule::OP_NE
            | Rule::OP_LT
            | Rule::OP_LE
            | Rule::OP_GT
            | Rule::OP_GE
            | Rule::OP_ADD
            | Rule::OP_SUB
            | Rule::OP_MUL
            | Rule::OP_DIV => binop_of(op_or.as_rule()),
            other => {
                let (line, col) = op_or.as_span().start_pos().line_col();
                bail!(
                    "unexpected token in binary sequence: {:?} at {}:{}",
                    other,
                    line,
                    col
                );
            }
        };

        let rhs = build_expr(rhs_pair)?;
        node = Expr::Binary {
            op,
            lhs: Box::new(node),
            rhs: Box::new(rhs),
        };
    }
    Ok(node)
}
