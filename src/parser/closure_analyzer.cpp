/**
 * @file closure_analyzer.cpp
 * @brief 闭包环境捕获分析
 * 
 * 分析闭包中使用的外部变量，确定需要捕获哪些变量
 */

#include "closure_analyzer.h"
#include "ast.h"
#include <set>
#include <iostream>

namespace pawc {

// 分析表达式中使用的变量
void ClosureAnalyzer::analyzeExpr(const Expr* expr, std::set<std::string>& used_vars) {
    if (!expr) return;
    
    switch (expr->kind) {
        case Expr::Kind::Identifier: {
            const IdentifierExpr* id = static_cast<const IdentifierExpr*>(expr);
            used_vars.insert(id->name);
            break;
        }
        
        case Expr::Kind::Binary: {
            const BinaryExpr* bin = static_cast<const BinaryExpr*>(expr);
            analyzeExpr(bin->left.get(), used_vars);
            analyzeExpr(bin->right.get(), used_vars);
            break;
        }
        
        case Expr::Kind::Unary: {
            const UnaryExpr* un = static_cast<const UnaryExpr*>(expr);
            analyzeExpr(un->operand.get(), used_vars);
            break;
        }
        
        case Expr::Kind::Call: {
            const CallExpr* call = static_cast<const CallExpr*>(expr);
            analyzeExpr(call->callee.get(), used_vars);
            for (const auto& arg : call->arguments) {
                analyzeExpr(arg.get(), used_vars);
            }
            break;
        }
        
        case Expr::Kind::Index: {
            const IndexExpr* idx = static_cast<const IndexExpr*>(expr);
            analyzeExpr(idx->array.get(), used_vars);
            analyzeExpr(idx->index.get(), used_vars);
            break;
        }
        
        case Expr::Kind::MemberAccess: {
            const MemberAccessExpr* member = static_cast<const MemberAccessExpr*>(expr);
            analyzeExpr(member->object.get(), used_vars);
            break;
        }
        
        case Expr::Kind::Assign: {
            const AssignExpr* assign = static_cast<const AssignExpr*>(expr);
            analyzeExpr(assign->target_expr.get(), used_vars);
            analyzeExpr(assign->value.get(), used_vars);
            break;
        }
        
        case Expr::Kind::Cast: {
            const CastExpr* cast = static_cast<const CastExpr*>(expr);
            analyzeExpr(cast->expression.get(), used_vars);
            break;
        }
        
        case Expr::Kind::IfExpr: {
            const IfExpr* if_expr = static_cast<const IfExpr*>(expr);
            analyzeExpr(if_expr->condition.get(), used_vars);
            analyzeExpr(if_expr->then_expr.get(), used_vars);
            analyzeExpr(if_expr->else_expr.get(), used_vars);
            break;
        }
        
        case Expr::Kind::ArrayLiteral: {
            const ArrayLiteralExpr* arr = static_cast<const ArrayLiteralExpr*>(expr);
            for (const auto& elem : arr->elements) {
                analyzeExpr(elem.get(), used_vars);
            }
            break;
        }
        
        case Expr::Kind::FString: {
            const FStringExpr* fstr = static_cast<const FStringExpr*>(expr);
            for (const auto& e : fstr->expressions) {
                analyzeExpr(e.get(), used_vars);
            }
            break;
        }
        
        // Closure 嵌套：不深入分析（嵌套闭包有自己的捕获）
        case Expr::Kind::Closure:
            break;
        
        // 其他表达式类型...
        default:
            break;
    }
}

// 分析语句中使用的变量
void ClosureAnalyzer::analyzeStmt(const Stmt* stmt, std::set<std::string>& used_vars) {
    if (!stmt) return;
    
    switch (stmt->kind) {
        case Stmt::Kind::Expression: {
            const ExprStmt* expr_stmt = static_cast<const ExprStmt*>(stmt);
            analyzeExpr(expr_stmt->expression.get(), used_vars);
            break;
        }
        
        case Stmt::Kind::Return: {
            const ReturnStmt* ret = static_cast<const ReturnStmt*>(stmt);
            if (ret->value) {
                analyzeExpr(ret->value.get(), used_vars);
            }
            break;
        }
        
        case Stmt::Kind::Let: {
            const LetStmt* let = static_cast<const LetStmt*>(stmt);
            if (let->initializer) {
                analyzeExpr(let->initializer.get(), used_vars);
            }
            // 注意：let 定义的变量不算捕获
            break;
        }
        
        case Stmt::Kind::If: {
            const IfStmt* if_stmt = static_cast<const IfStmt*>(stmt);
            analyzeExpr(if_stmt->condition.get(), used_vars);
            analyzeStmt(if_stmt->then_branch.get(), used_vars);
            if (if_stmt->else_branch) {
                analyzeStmt(if_stmt->else_branch.get(), used_vars);
            }
            break;
        }
        
        case Stmt::Kind::Block: {
            const BlockStmt* block = static_cast<const BlockStmt*>(stmt);
            for (const auto& s : block->statements) {
                analyzeStmt(s.get(), used_vars);
            }
            break;
        }
        
        case Stmt::Kind::Loop: {
            const LoopStmt* loop = static_cast<const LoopStmt*>(stmt);
            if (loop->condition) {
                analyzeExpr(loop->condition.get(), used_vars);
            }
            if (loop->iterable) {
                analyzeExpr(loop->iterable.get(), used_vars);
            }
            analyzeStmt(loop->body.get(), used_vars);
            break;
        }
        
        default:
            break;
    }
}

// 分析闭包捕获的变量
std::vector<std::string> ClosureAnalyzer::analyzeCapturedVars(
    const ClosureExpr* closure,
    const std::set<std::string>& available_vars
) {
    std::set<std::string> used_vars;
    
    // 分析闭包 body
    analyzeStmt(closure->body.get(), used_vars);
    
    // 移除参数（参数不是捕获）
    for (const auto& param : closure->params) {
        used_vars.erase(param.name);
    }
    
    // 移除 body 内定义的局部变量
    removeLocalVars(closure->body.get(), used_vars);
    
    // 只保留在外部作用域中存在的变量
    std::vector<std::string> captures;
    for (const auto& var : used_vars) {
        if (available_vars.find(var) != available_vars.end()) {
            captures.push_back(var);
        }
    }
    
    return captures;
}

// 移除 body 内定义的局部变量
void ClosureAnalyzer::removeLocalVars(const Stmt* stmt, std::set<std::string>& used_vars) {
    if (!stmt) return;
    
    if (stmt->kind == Stmt::Kind::Let) {
        const LetStmt* let = static_cast<const LetStmt*>(stmt);
        used_vars.erase(let->name);
    } else if (stmt->kind == Stmt::Kind::Block) {
        const BlockStmt* block = static_cast<const BlockStmt*>(stmt);
        for (const auto& s : block->statements) {
            removeLocalVars(s.get(), used_vars);
        }
    } else if (stmt->kind == Stmt::Kind::If) {
        const IfStmt* if_stmt = static_cast<const IfStmt*>(stmt);
        removeLocalVars(if_stmt->then_branch.get(), used_vars);
        if (if_stmt->else_branch) {
            removeLocalVars(if_stmt->else_branch.get(), used_vars);
        }
    } else if (stmt->kind == Stmt::Kind::Loop) {
        const LoopStmt* loop = static_cast<const LoopStmt*>(stmt);
        // 移除迭代器变量
        if (!loop->iterator_var.empty()) {
            used_vars.erase(loop->iterator_var);
        }
        removeLocalVars(loop->body.get(), used_vars);
    }
}

} // namespace pawc

