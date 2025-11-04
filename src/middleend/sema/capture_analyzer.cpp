//===--- capture_analyzer.cpp - Closure Capture Analysis -------*- C++ -*-===//

#include "capture_analyzer.h"
#include "frontend/parser/ast/pattern.h"
#include <iostream>

namespace pawc {

std::vector<ClosureExpr::CapturedVar> CaptureAnalyzer::analyze(
    Expr* body, 
    const std::vector<ClosureExpr::Param>& params) {
    
    // 清空之前的分析结果
    local_vars_stack_.clear();
    captured_vars_.clear();
    closure_depth_ = 0;
    
    // 创建第一层作用域
    enterScope();
    
    // 1. 将闭包参数添加到当前作用域（它们不是捕获变量）
    for (const auto& param : params) {
        local_vars_stack_.back().insert(param.name);
    }
    
    // 2. 遍历闭包体
    if (body) {
        body->accept(this);
    }
    
    // 退出作用域
    exitScope();
    
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

void CaptureAnalyzer::enterScope() {
    local_vars_stack_.push_back(std::set<std::string>());
}

void CaptureAnalyzer::exitScope() {
    if (!local_vars_stack_.empty()) {
        local_vars_stack_.pop_back();
    }
}

bool CaptureAnalyzer::isLocalVariable(const std::string& name) const {
    // 检查所有作用域栈
    for (const auto& scope : local_vars_stack_) {
        if (scope.find(name) != scope.end()) {
            return true;
        }
    }
    return false;
}

void CaptureAnalyzer::visit(IdentifierExpr* node) {
    const std::string& name = node->getName();
    
    // 如果不是局部变量，则是捕获变量
    if (!isLocalVariable(name)) {
        // 检查是否在外部作用域存在
        if (symbols_->lookupVariable(name)) {
            captured_vars_.insert(name);
            std::cerr << "[CaptureAnalyzer] Captured variable: " << name << std::endl;
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
    // 🔧 进入新作用域
    enterScope();
    
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
    
    // 🔧 退出作用域（局部变量不再可见）
    exitScope();
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
        // 🔧 每个match arm都是一个新作用域
        enterScope();
        
        // 访问pattern（定义新变量）
        arm.pattern->accept(this);
        
        // 访问arm的expression
        arm.expression->accept(this);
        
        // 🔧 退出arm作用域
        exitScope();
    }
}

void CaptureAnalyzer::visit(ClosureExpr* node) {
    // 🔧 嵌套闭包：递归分析其捕获
    // 嵌套闭包可能捕获外层闭包的参数和捕获变量
    
    closure_depth_++;
    
    if (closure_depth_ > 10) {
        // 防止过深嵌套导致问题
        std::cerr << "[CaptureAnalyzer] Warning: Nested closure depth > 10" << std::endl;
        closure_depth_--;
        return;
    }
    
    // 创建新作用域
    enterScope();
    
    // 添加嵌套闭包的参数
    for (const auto& param : node->getParams()) {
        local_vars_stack_.back().insert(param.name);
    }
    
    // 分析嵌套闭包的body
    if (node->getBody()) {
        node->getBody()->accept(this);
    }
    
    // 退出嵌套闭包作用域
    exitScope();
    
    closure_depth_--;
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
    // 先检查初始值（可能引用外部变量）
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
    
    // 然后添加到当前作用域的局部变量
    if (!local_vars_stack_.empty()) {
        local_vars_stack_.back().insert(node->getName());
    }
}

void CaptureAnalyzer::visit(DestructuringDecl* node) {
    // 先检查初始值
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
    
    // 然后添加到当前作用域的局部变量
    if (!local_vars_stack_.empty()) {
        for (const auto& name : node->getNames()) {
            local_vars_stack_.back().insert(name);
        }
    }
}

void CaptureAnalyzer::visit(StructDestructuringDecl* node) {
    // 先检查初始值
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
    
    // 然后添加到当前作用域的局部变量
    if (!local_vars_stack_.empty()) {
        for (const auto& name : node->getFieldNames()) {
            local_vars_stack_.back().insert(name);
        }
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
    // 🔧 进入新作用域
    enterScope();
    
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
    
    // 🔧 退出作用域（局部变量不再可见）
    exitScope();
}

void CaptureAnalyzer::visit(ForStmt* node) {
    // ForStmt暂时不使用，简化实现
    // PawLang使用统一的loop关键字
}

void CaptureAnalyzer::visit(VariablePattern* node) {
    // Pattern绑定的变量是当前作用域的局部变量
    if (!local_vars_stack_.empty()) {
        local_vars_stack_.back().insert(node->getName());
    }
}

void CaptureAnalyzer::visit(TuplePattern* node) {
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(EnumPattern* node) {
    // EnumPattern可能有嵌套pattern
    const auto& inner_patterns = node->getInnerPatterns();
    for (const auto& pattern : inner_patterns) {
        pattern->accept(this);
    }
}

void CaptureAnalyzer::visit(StructPattern* node) {
    // 结构体模式绑定字段变量
    for (const auto& field : node->getFields()) {
        field.pattern->accept(this);
    }
}

} // namespace pawc

