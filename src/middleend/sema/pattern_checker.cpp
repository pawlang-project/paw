//===--- pattern_checker.cpp - Pattern Checker Implementation ---*- C++ -*-===//
/// @file pattern_checker.cpp
/// @brief Type system and semantic analysis implementation

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
    // getby/passive markermatchof/theexpressiontypes
    Expr* scrutinee = match_expr->getScrutinee();
    if (!scrutinee || !scrutinee->getType()) {
        return true; // typenot yetknown，skipcheck
    }
    
    Type* scrutinee_type = scrutinee->getType();
    
    // collectAllpattern
    std::vector<Pattern*> patterns;
    for (const auto& arm : match_expr->getArms()) {
        patterns.push_back(arm.pattern.get());
    }
    
    return isExhaustive(patterns, scrutinee_type);
}

bool PatternChecker::isExhaustive(const std::vector<Pattern*>& patterns, Type* type) {
    // Checkyesnohaswildcardpattern
    for (Pattern* pattern : patterns) {
        if (auto* wildcard = dynamic_cast<WildcardPattern*>(pattern)) {
            return true; // wildcardoverride/coverAllcase/situation
        }
        if (auto* var = dynamic_cast<VariablePattern*>(pattern)) {
            // purevariablepatternalsooverride/coverAllcase/situation（like/such as x => ...）
            return true;
        }
    }
    
    // right/correctat/inenumtypes，checkyesnooverride/coverAllvariablebody/struct
    if (type->isEnum()) {
        EnumType* enum_type = static_cast<EnumType*>(type);
        return checkEnumExhaustiveness(patterns, enum_type);
    }
    
    // right/correctat/inbooleantypes，check true and false
    if (type->isBool()) {
        return checkBoolExhaustiveness(patterns);
    }
    
    // right/correctat/inOptionaltypes，check some and none
    if (type->isOptional()) {
        return checkOptionalExhaustiveness(patterns);
    }
    
    // Other typestypes：ifnohaswildcard，reportwarning（butnottreat/viewis/aserror）
    std::cerr << "[PatternChecker] Warning: Match may not be exhaustive for type: " 
              << type->toString() << std::endl;
    return true;
}

bool PatternChecker::checkEnumExhaustiveness(const std::vector<Pattern*>& patterns, 
                                              EnumType* enum_type) {
    const auto& variants = enum_type->getVariants();
    std::unordered_set<std::string> covered_variants;
    
    // collectalreadyoverride/coverof/thevariablebody/struct
    for (Pattern* pattern : patterns) {
        if (auto* enum_pattern = dynamic_cast<EnumPattern*>(pattern)) {
            covered_variants.insert(enum_pattern->getVariantName());
        }
    }
    
    // CheckyesnoAllvariablebody/structallby/passive markeroverride/cover
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
            // simplify：checkyesnohasliteralpatternoverride/cover
            // detailedimplementationneedvisitliteralof/thevalue
            // TODO: implementationcompleteof/theliteralcheck
            has_true = true;
            has_false = true;
        }
    }
    
    if (!has_true || !has_false) {
        std::cerr << "[PatternChecker] Warning: Match may not be exhaustive for bool type" << std::endl;
        return true; // warningbutnotblock compilation
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
    // concreteimplementationfromTypeCheckermiddle/centerextract
    // TODO: checkpatterntypes
    return true;
}

void PatternChecker::collectPatternVariables(
    Pattern* pattern, 
    std::vector<std::pair<std::string, Type*>>& variables) {
    
    // concreteimplementationfromTypeCheckermiddle/centerextract
    // TODO: collectpatternvariable
}

} // namespace pawc

