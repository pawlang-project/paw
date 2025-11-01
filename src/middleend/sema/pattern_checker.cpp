//===--- pattern_checker.cpp - Pattern Checker Implementation ---*- C++ -*-===//

#include "pattern_checker.h"
#include "frontend/parser/ast/pattern.h"
#include "frontend/parser/ast/expr.h"

namespace pawc {

PatternChecker::PatternChecker(SemanticContext* context)
    : context_(context) {}

bool PatternChecker::checkExhaustiveness(MatchExpr* match_expr) {
    // 具体实现从TypeChecker中提取
    // TODO: 检查穷尽性
    return true;
}

bool PatternChecker::isExhaustive(const std::vector<Pattern*>& patterns, Type* type) {
    // 具体实现从TypeChecker中提取
    // TODO: 判断是否穷尽
    return true;
}

bool PatternChecker::checkPatternType(Pattern* pattern, Type* expected_type) {
    // 具体实现从TypeChecker中提取
    // TODO: 检查模式类型
    return true;
}

void PatternChecker::collectPatternVariables(
    Pattern* pattern, 
    std::vector<std::pair<std::string, Type*>>& variables) {
    
    // 具体实现从TypeChecker中提取
    // TODO: 收集模式变量
}

} // namespace pawc

