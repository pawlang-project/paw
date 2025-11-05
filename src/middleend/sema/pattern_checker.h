//===--- pattern_checker.h - Pattern Exhaustiveness Checker -----*- C++ -*-===//
//
// patterncheck - validatematchexpressionof/theexhaustiveness
//
//===----------------------------------------------------------------------===//

#ifndef PAW_PATTERN_CHECKER_H
#define PAW_PATTERN_CHECKER_H

#include "semantic_context.h"
#include <vector>

namespace pawc {

class Pattern;
class Type;
class EnumType;
class MatchExpr;

/// PatternChecker - patternchecker
///
/// negativeresponsiblevalidate：
/// - matchexpressionof/theexhaustiveness
/// - patterntypesof/thecorrectperformance
/// - patternvariableof/thescope
class PatternChecker {
    SemanticContext* context_;
    
public:
    explicit PatternChecker(SemanticContext* context);
    
    /// Checkmatchexpressionof/theexhaustiveness
    bool checkExhaustiveness(MatchExpr* match_expr);
    
    /// Checkpatternyesnooverride/covertypesof/theAllpossiblyvalue
    bool isExhaustive(const std::vector<Pattern*>& patterns, Type* type);
    
    /// Checkpatterntypes
    bool checkPatternType(Pattern* pattern, Type* expected_type);
    
    /// collectpatterninvariable
    void collectPatternVariables(Pattern* pattern, 
                                std::vector<std::pair<std::string, Type*>>& variables);

private:
    /// Checkenumexhaustiveness
    bool checkEnumExhaustiveness(const std::vector<Pattern*>& patterns, EnumType* enum_type);
    
    /// Checkbooleanexhaustiveness
    bool checkBoolExhaustiveness(const std::vector<Pattern*>& patterns);
    
    /// CheckOptionalexhaustiveness
    bool checkOptionalExhaustiveness(const std::vector<Pattern*>& patterns);
};

} // namespace pawc

#endif // PAW_PATTERN_CHECKER_H

