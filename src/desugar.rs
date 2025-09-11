// src/desugar.rs
use crate::ast::*;

pub fn desugar_program(mut p: Program) -> Program {
    p.items = p.items.into_iter().map(desugar_item).collect();
    p
}

fn desugar_item(it: Item) -> Item {
    match it {
        Item::Fun(mut f) => {
            f.body = desugar_block(f.body, /*in_loop=*/false, /*for_step=*/None);
            Item::Fun(f)
        }
        other => other,
    }
}

fn desugar_block(b: Block, in_loop: bool, for_step: Option<&ForStep>) -> Block {
    let mut out_stmts = Vec::new();
    for s in b.stmts {
        match s {
            Stmt::For { init, cond, step, body } => {
                if let Some(init) = init {
                    out_stmts.extend(for_init_to_stmts(init));
                }
                let cond_expr = cond.unwrap_or(Expr::Bool(true));
                let step_ref = step.as_ref(); // Option<&ForStep>

                // for 的主体：在本层递归传入 step_ref，这样本体里的 continue 会被改写为 step+continue
                let mut lowered_body = desugar_block(body, /*in_loop=*/true, step_ref);
                if let Some(st) = step_ref {
                    out_stmts_append_step(&mut lowered_body.stmts, st); // 正常路径末尾也执行 step
                }

                out_stmts.push(Stmt::While { cond: cond_expr, body: lowered_body });
            }

            Stmt::While { cond, body } => {
                // 内层循环不受外层 for 的 step 影响
                let body2 = desugar_block(body, /*in_loop=*/true, /*for_step=*/None);
                out_stmts.push(Stmt::While { cond, body: body2 });
            }

            Stmt::Continue => {
                // 若在 for 的主体中（for_step=Some），把 continue 反糖为 { step; continue; }
                if let Some(st) = for_step {
                    out_stmts_append_step(&mut out_stmts, st);
                }
                out_stmts.push(Stmt::Continue);
            }

            Stmt::Break => {
                out_stmts.push(Stmt::Break);
            }

            Stmt::Let { .. } | Stmt::Assign { .. } | Stmt::Return(_) => {
                out_stmts.push(desugar_stmt_inside(s, in_loop, for_step));
            }

            Stmt::Expr(e) => {
                out_stmts.push(Stmt::Expr(desugar_expr(e, in_loop, for_step)));
            }
        }
    }

    let tail = b.tail.map(|e| Box::new(desugar_expr(*e, in_loop, for_step)));
    Block { stmts: out_stmts, tail }
}


fn desugar_stmt_inside(s: Stmt, in_loop: bool, for_step: Option<&ForStep>) -> Stmt {
    match s {
        Stmt::Return(opt) => {
            Stmt::Return(opt.map(|e| desugar_expr(e, in_loop, for_step)))
        }
        Stmt::Let { name, ty, init, is_const } => {
            Stmt::Let { name, ty, init: desugar_expr(init, in_loop, for_step), is_const }
        }
        Stmt::Assign { name, expr } => {
            Stmt::Assign { name, expr: desugar_expr(expr, in_loop, for_step) }
        }
        other => other
    }
}

// 把 for-body 里的 continue 替换为 { step?; continue; }
fn rewrite_continue_in_block(mut b: Block, step: Option<&ForStep>) -> Block {
    let mut new_stmts = Vec::new();
    for s in b.stmts {
        match s {
            Stmt::Continue => {
                if let Some(st) = step {
                    out_stmts_append_step(&mut new_stmts, st);
                }
                new_stmts.push(Stmt::Continue);
                // continue 后面的语句自然不会执行；剩余语句仍会依次处理（到达它们的前提是没执行 continue）
            }
            Stmt::While { cond, body } => {
                // 内层 while/for 不受外层 for 的 step 影响
                new_stmts.push(Stmt::While { cond, body });
            }
            Stmt::Expr(e) => new_stmts.push(Stmt::Expr(e)),
            Stmt::Assign { .. } | Stmt::Let { .. } | Stmt::Return(_) | Stmt::Break => {
                new_stmts.push(s)
            }
            Stmt::For { .. } => {
                // 递归：内层 for 自己处理自己的 step
                new_stmts.push(s)
            }
        }
    }
    b.stmts = new_stmts;
    b
}

fn desugar_expr(e: Expr, in_loop: bool, for_step: Option<&ForStep>) -> Expr {
    match e {
        Expr::If { cond, then_b, else_b } => {
            Expr::If {
                cond: Box::new(desugar_expr(*cond, in_loop, for_step)),
                then_b: desugar_block(then_b, in_loop, for_step),
                else_b: desugar_block(else_b, in_loop, for_step),
            }
        }
        Expr::Block(b) => Expr::Block(desugar_block(b, in_loop, for_step)),
        Expr::Match { scrut, arms, default } => {
            // match 反糖成 if-else 链
            let s_tmp = *scrut;
            lower_match_to_if(s_tmp, arms, default)
        }
        other => other,
    }
}

fn lower_match_to_if(scrut: Expr, arms: Vec<(Pattern, Block)>, default: Option<Block>) -> Expr {
    let mut else_b = default.unwrap_or(Block { stmts: vec![], tail: None });

    if arms.is_empty() {
        return Expr::Block(else_b);
    }

    for (pk, bk) in arms.into_iter().rev() {
        let cond = Box::new(eq_pat(scrut.clone(), pk));
        let then_b = bk;
        let e = Expr::If { cond, then_b, else_b };
        else_b = Block { stmts: vec![], tail: Some(Box::new(e)) };
    }

    match else_b.tail {
        Some(t) => *t,
        None => Expr::Block(else_b),
    }
}


// 生成 (__s == pat) 的布尔表达式（再由类型检查保证两侧类型匹配）
fn eq_pat(scrut: Expr, pat: Pattern) -> Expr {
    use crate::ast::BinOp::*;
    let rhs = match pat {
        Pattern::Int(n) => Expr::Int(n),
        Pattern::Bool(b) => Expr::Bool(b),
        Pattern::Wild => return Expr::Bool(true), // 万能匹配
    };
    Expr::Binary { op: Eq, lhs: Box::new(scrut), rhs: Box::new(rhs) }
}

fn for_init_to_stmts(init: ForInit) -> Vec<Stmt> {
    match init {
        ForInit::Let { name, ty, init, is_const } =>
            vec![Stmt::Let { name, ty, init, is_const }],
        ForInit::Assign { name, expr } =>
            vec![Stmt::Assign { name, expr }],
        ForInit::Expr(e) =>
            vec![Stmt::Expr(e)],
    }
}

fn out_stmts_append_step(out: &mut Vec<Stmt>, st: &ForStep) {
    match st {
        ForStep::Assign { name, expr } => out.push(Stmt::Assign { name: name.clone(), expr: expr.clone() }),
        ForStep::Expr(e) => out.push(Stmt::Expr(e.clone())),
    }
}
