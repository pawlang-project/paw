//===--- pattern_checker.cpp - Pattern Checker Implementation ---*- C++ -*-===//

#include "pattern_checker.h"
#include "frontend/parser/ast/pattern.h"
#include "frontend/parser/ast/expr.h"
#include "middleend/types/type.h"
#include <unordered_set>
#include <iostream>

namespace pawc {

PatternChecker::PatternChecker(SemanticContext* context)
    : context_(context) {}

bool PatternChecker::checkExhaustiveness(MatchExpr* match_expr) {
    // 获取被匹配的表达式类型
    Expr* scrutinee = match_expr->getScrutinee();
    if (!scrutinee || !scrutinee->getType()) {
        return true; // 类型未知，跳过检查
    }
    
    Type* scrutinee_type = scrutinee->getType();
    
    // 收集所有模式
    std::vector<Pattern*> patterns;
    for (const auto& arm : match_expr->getArms()) {
        patterns.push_back(arm.pattern.get());
    }
    
    return isExhaustive(patterns, scrutinee_type);
}

bool PatternChecker::isExhaustive(const std::vector<Pattern*>& patterns, Type* type) {
    // 检查是否有通配符模式
    for (Pattern* pattern : patterns) {
        if (auto* wildcard = dynamic_cast<WildcardPattern*>(pattern)) {
            return true; // 通配符覆盖所有情况
        }
        if (auto* var = dynamic_cast<VariablePattern*>(pattern)) {
            // 纯变量模式也覆盖所有情况（如 x => ...）
            return true;
        }
    }
    
    // 对于枚举类型，检查是否覆盖所有变体
    if (type->isEnum()) {
        EnumType* enum_type = static_cast<EnumType*>(type);
        return checkEnumExhaustiveness(patterns, enum_type);
    }
    
    // 对于布尔类型，检查 true 和 false
    if (type->isBool()) {
        return checkBoolExhaustiveness(patterns);
    }
    
    // 对于Optional类型，检查 some 和 none
    if (type->isOptional()) {
        return checkOptionalExhaustiveness(patterns);
    }
    
    // 其他类型：如果没有通配符，报警告（但不视为错误）
    std::cerr << "[PatternChecker] Warning: Match may not be exhaustive for type: " 
              << type->toString() << std::endl;
    return true;
}

bool PatternChecker::checkEnumExhaustiveness(const std::vector<Pattern*>& patterns, 
                                              EnumType* enum_type) {
    const auto& variants = enum_type->getVariants();
    std::unordered_set<std::string> covered_variants;
    
    // 收集已覆盖的变体
    for (Pattern* pattern : patterns) {
        if (auto* enum_pattern = dynamic_cast<EnumPattern*>(pattern)) {
            covered_variants.insert(enum_pattern->getVariantName());
        }
    }
    
    // 检查是否所有变体都被覆盖
    bool exhaustive = true;
    for (const auto& variant : variants) {
        if (covered_variants.find(variant.first) == covered_variants.end()) {
            std::cerr << "[PatternChecker] Error: Match not exhaustive - missing variant: " 
                      << variant.first << std::endl;
            exhaustive = false;
        }
    }
    
    return exhaustive;
}

bool PatternChecker::checkBoolExhaustiveness(const std::vector<Pattern*>& patterns) {
    bool has_true = false;
    bool has_false = false;
    
    for (Pattern* pattern : patterns) {
        if (auto* lit = dynamic_cast<LiteralPattern*>(pattern)) {
            // 简化：检查是否有字面量模式覆盖
            // 详细实现需要访问literal的值
            // TODO: 实现完整的字面量检查
            has_true = true;
            has_false = true;
        }
    }
    
    if (!has_true || !has_false) {
        std::cerr << "[PatternChecker] Warning: Match may not be exhaustive for bool type" << std::endl;
        return true; // 警告但不阻止编译
    }
    
    return true;
}

bool PatternChecker::checkOptionalExhaustiveness(const std::vector<Pattern*>& patterns) {
    bool has_some = false;
    bool has_none = false;
    
    for (Pattern* pattern : patterns) {
        if (auto* enum_pattern = dynamic_cast<EnumPattern*>(pattern)) {
            std::string variant = enum_pattern->getVariantName();
            if (variant == "Some" || variant == "some") {
                has_some = true;
            } else if (variant == "None" || variant == "none") {
                has_none = true;
            }
        }
    }
    
    if (!has_some || !has_none) {
        std::cerr << "[PatternChecker] Error: Match not exhaustive for Optional type" << std::endl;
        return false;
    }
    
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

