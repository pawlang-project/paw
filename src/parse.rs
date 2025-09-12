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

fn build_item(p: Pair<Rule>) -> Result<Item> {
    Ok(match p.as_rule() {
        Rule::let_decl => {
            let mut it = p.into_inner();
            let kw = it.next().unwrap(); // KW_LET | KW_CONST
            let name = it.next().unwrap().as_str().to_string();
            let ty = build_ty(it.next().unwrap())?;
            let init = build_expr(it.next().unwrap())?;
            Item::Global {
                name, ty, init,
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
                return Err(anyhow!("import_decl: expected string_lit, got {:?}", lit.as_rule()));
            }
            Item::Import(unescape_string(lit.as_str()))
        }
        r => return Err(anyhow!("unexpected item: {:?}", r)),
    })
}

fn build_fun_item(p: Pair<Rule>) -> Result<Item> {
    let mut name: Option<String> = None;
    let mut params: Vec<(String, Ty)> = Vec::new();
    let mut ret: Option<Ty> = None;
    let mut body: Option<Block> = None;

    for child in p.into_inner() {
        match child.as_rule() {
            Rule::KW_FNs => { /* ignore */ }
            Rule::ident => name = Some(child.as_str().to_string()),
            Rule::param_list => params = build_params(child)?,
            Rule::ty => ret = Some(build_ty(child)?),
            Rule::block => body = Some(build_block(child)?),
            _ => { /* 忽略字面量 "(" ")" "->" 等 */ }
        }
    }

    let name = name.ok_or_else(|| anyhow!("fun_decl: missing name"))?;
    let ret  = ret.ok_or_else(|| anyhow!("fun_decl: missing return ty for `{}`", name))?;
    let body = body.ok_or_else(|| anyhow!("fun_decl: missing body for `{}`", name))?;

    Ok(Item::Fun(FunDecl {
        name,
        params,
        ret,
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
            _ => { /* 忽略字面量 "(" ")" "->" ";" 等 */ }
        }
    }

    let name = name.ok_or_else(|| anyhow!("extern_fun: missing name"))?;
    let ret  = ret.ok_or_else(|| anyhow!("extern_fun: missing return ty for `{}`", name))?;

    Ok(Item::Fun(FunDecl {
        name,
        params,
        ret,
        body: Block { stmts: vec![], tail: None },
        is_extern: true,
    }))
}


fn build_params(p: Pair<Rule>) -> Result<Vec<(String, Ty)>> {
    let mut v = Vec::new();
    for x in p.into_inner() { // x: param
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
            Stmt::Let { name, ty, init, is_const: kw.as_rule() == Rule::KW_CONST }
        }

        Rule::assign_stmt => {
            let mut it = p.into_inner();
            let name = it.next().unwrap().as_str().to_string();
            let expr = build_expr(it.next().unwrap())?;
            Stmt::Assign { name, expr }
        }

        Rule::while_stmt => {
            let mut it = p.into_inner();
            let _kw  = it.next();                   // KW_WHILE
            let cond = build_expr(it.next().unwrap())?; // expr
            let body = build_block(it.next().unwrap())?; // block
            Stmt::While { cond, body }
        }

        Rule::for_stmt => build_for_stmt(p)?,

        Rule::if_stmt => {
            let mut it = p.into_inner();
            let _kw    = it.next();                     // KW_IF
            let cond   = build_expr(it.next().unwrap())?;   // expr
            let then_b = build_block(it.next().unwrap())?;  // block
            let else_b = it.next().map(build_block).transpose()?; // 可能有一个 block
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
    let mut cond: Option<Expr>    = None;
    let mut step: Option<ForStep> = None;
    let mut body: Option<Block>   = None;

    for x in p.into_inner() {
        match x.as_rule() {
            Rule::for_init => init = Some(build_for_init(x)?),
            Rule::for_step => step = Some(build_for_step(x)?),
            Rule::expr     => cond = Some(build_expr(x)?),
            Rule::block    => body = Some(build_block(x)?),
            _ => { /* 忽略 "(" , ";" , ")" 等字面量 */ }
        }
    }
    let body = body.ok_or_else(|| anyhow!("for_stmt: missing body block"))?;
    Ok(Stmt::For { init, cond, step, body })
}

fn build_for_init(p: Pair<Rule>) -> Result<ForInit> {
    // for_init 可能是：
    //   let/const 形式:  KW_LET|KW_CONST, ident, ty, expr
    //   赋值形式:        ident, expr        （= 不会进 inner）
    //   表达式形式:      expr
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
            let expr = build_expr(it.next().unwrap())?; // 不要读 "="
            ForInit::Assign { name, expr }
        }
        Rule::expr => ForInit::Expr(build_expr(first)?),
        r => return Err(anyhow!("unexpected for_init node: {:?}", r)),
    })
}


fn build_for_step(p: Pair<Rule>) -> Result<ForStep> {
    // for_step 可能是：
    //   赋值形式: ident, expr   （= 不会进 inner）
    //   表达式形式: expr
    let mut it = p.into_inner();
    let first = it.next().unwrap();

    Ok(match first.as_rule() {
        Rule::ident => {
            let name = first.as_str().to_string();
            let expr = build_expr(it.next().unwrap())?; // 不要读 "="
            ForStep::Assign { name, expr }
        }
        Rule::expr => ForStep::Expr(build_expr(first)?),
        r => return Err(anyhow!("unexpected for_step node: {:?}", r)),
    })
}

fn build_if_expr(p: Pair<Rule>) -> Result<Expr> {
    let mut it = p.into_inner();
    let _if   = it.next();                         // KW_IF
    let cond  = build_expr(it.next().unwrap())?;   // expr
    let then_b= build_block(it.next().unwrap())?;  // block
    let _else = it.next();                         // KW_ELSE
    let else_b= build_block(it.next().unwrap())?;  // block
    Ok(Expr::If { cond: Box::new(cond), then_b, else_b })
}


fn build_match_expr(p: Pair<Rule>) -> Result<Expr> {
    // match (expr) { arms? }
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
        // pattern 是一个包装规则：可能包着 int_lit / bool_lit，或者为空（当写成 "_"）
        Rule::pattern => {
            let mut inner = p.into_inner();
            if let Some(q) = inner.next() {
                build_pattern(q)?
            } else {
                Pattern::Wild // "_" 的情况
            }
        }
        Rule::int_lit  => Pattern::Int(p.as_str().parse::<i64>()?),
        Rule::bool_lit => Pattern::Bool(p.as_str() == "true"),
        // 其他都不该出现；如果 grammar 将 "_" 单独设成 rule，这里也可以匹配它：
        // Rule::wild_pat => Pattern::Wild,
        r => return Err(anyhow!("unexpected pattern node: {:?}", r)),
    })
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
                            for ae in al.into_inner() { args.push(build_expr(ae)?); }
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
        Rule::primary    => build_expr(p.into_inner().next().unwrap())?,
        Rule::group      => build_expr(p.into_inner().next().unwrap())?,
        Rule::if_expr    => build_if_expr(p)?,
        Rule::match_expr => build_match_expr(p)?,
        Rule::int_lit    => Expr::Int(p.as_str().parse::<i64>()?),
        Rule::bool_lit   => Expr::Bool(p.as_str() == "true"),
        Rule::string_lit => Expr::Str(unescape_string(p.as_str())),
        Rule::ident      => Expr::Var(p.as_str().to_string()),
        Rule::block      => Expr::Block(build_block(p)?),
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
            Rule::OP_OR  => Or,
            r => return Err(anyhow!("binary op {:?} not expected", r)),
        };
        lhs = Expr::Binary { op: bop, lhs: Box::new(lhs), rhs: Box::new(rhs) };
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
