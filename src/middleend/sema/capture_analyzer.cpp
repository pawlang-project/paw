//===--- capture_analyzer.cpp - Closure Capture Analysis -------*- C++ -*-===//

#include "capture_analyzer.h"
#include "frontend/parser/ast/pattern.h"
#include <iostream>

namespace pawc {

std::vector<ClosureExpr::CapturedVar> CaptureAnalyzer::analyze(
    Expr* body, 
    const std::vector<ClosureExpr::Param>& params) {
    
    // 清空之前的分析结果
    local_vars_.clear();
    captured_vars_.clear();
    
    // 1. 将闭包参数添加到local_vars（它们不是捕获变量）
    for (const auto& param : params) {
        local_vars_.insert(param.name);
    }
    
    // 2. 遍历闭包体
    if (body) {
        body->accept(this);
    }
    
    // 3. 构建捕获变量列表
    std::vector<ClosureExpr::CapturedVar> result;
    for (const auto& var_name : captured_vars_) {
        // 从符号表查找类型
        VariableSymbol* var = symbols_->lookupVariable(var_name);
        if (var) {
            result.emplace_back(var_name, var->getType(), false);  // 默认值捕获
        }
    }
    
    return result;
}

void CaptureAnalyzer::visit(IdentifierExpr* node) {
    const std::string& name = node->getName();
    
    // 如果不是局部变量，则是捕获变量
    if (local_vars_.find(name) == local_vars_.end()) {
        // 检查是否在外部作用域存在
        if (symbols_->lookupVariable(name)) {
            captured_vars_.insert(name);
        }
    }
}

void CaptureAnalyzer::visit(BinaryExpr* node) {
    node->getLeft()->accept(this);
    node->getRight()->accept(this);
}

void CaptureAnalyzer::visit(UnaryExpr* node) {
    node->getOperand()->accept(this);
}

void CaptureAnalyzer::visit(CallExpr* node) {
    node->getCallee()->accept(this);
    for (const auto& arg : node->getArgs()) {
        arg->accept(this);
    }
}

void CaptureAnalyzer::visit(MemberExpr* node) {
    node->getObject()->accept(this);
}

void CaptureAnalyzer::visit(IndexExpr* node) {
    node->getObject()->accept(this);
    node->getIndex()->accept(this);
}

void CaptureAnalyzer::visit(IfExpr* node) {
    node->getCondition()->accept(this);
    node->getThenExpr()->accept(this);
    if (node->getElseExpr()) {
        node->getElseExpr()->accept(this);
    }
}

void CaptureAnalyzer::visit(BlockExpr* node) {
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
}

void CaptureAnalyzer::visit(ArrayLiteral* node) {
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(TupleExpr* node) {
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(RangeExpr* node) {
    node->getStart()->accept(this);
    node->getEnd()->accept(this);
}

void CaptureAnalyzer::visit(StructLiteral* node) {
    for (const auto& field : node->getFields()) {
        field.value->accept(this);
    }
}

void CaptureAnalyzer::visit(MatchExpr* node) {
    node->getScrutinee()->accept(this);
    for (const auto& arm : node->getArms()) {
        // 访问pattern（可能定义新变量）
        arm.pattern->accept(this);
        // 访问arm的expression
        arm.expression->accept(this);
    }
}

void CaptureAnalyzer::visit(ClosureExpr* node) {
    // 嵌套闭包：不遍历其内部（它有自己的捕获分析）
}

void CaptureAnalyzer::visit(TryExpr* node) {
    node->getExpr()->accept(this);
}

void CaptureAnalyzer::visit(ExprStmt* node) {
    if (node->getExpr()) {
        node->getExpr()->accept(this);
    }
}

void CaptureAnalyzer::visit(VarDecl* node) {
    // 添加到局部变量
    local_vars_.insert(node->getName());
    
    // 检查初始值
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
}

void CaptureAnalyzer::visit(ReturnStmt* node) {
    if (node->getValue()) {
        node->getValue()->accept(this);
    }
}

void CaptureAnalyzer::visit(IfStmt* node) {
    node->getCondition()->accept(this);
    node->getThenStmt()->accept(this);
    if (node->getElseStmt()) {
        node->getElseStmt()->accept(this);
    }
}

void CaptureAnalyzer::visit(LoopStmt* node) {
    // 简化实现：只遍历body
    node->getBody()->accept(this);
}

void CaptureAnalyzer::visit(WhileStmt* node) {
    node->getCondition()->accept(this);
    node->getBody()->accept(this);
}

void CaptureAnalyzer::visit(BlockStmt* node) {
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
}

void CaptureAnalyzer::visit(ForStmt* node) {
    // ForStmt暂时不使用，简化实现
    // PawLang使用统一的loop关键字
}

void CaptureAnalyzer::visit(VariablePattern* node) {
    // Pattern绑定的变量是局部变量
    local_vars_.insert(node->getName());
}

void CaptureAnalyzer::visit(TuplePattern* node) {
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(EnumPattern* node) {
    // EnumPattern可能有嵌套pattern
    if (node->getInner()) {
        node->getInner()->accept(this);
    }
}

} // namespace pawc

