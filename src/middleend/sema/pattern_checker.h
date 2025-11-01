//===--- pattern_checker.h - Pattern Exhaustiveness Checker -----*- C++ -*-===//
//
// 模式检查 - 验证match表达式的穷尽性
//
//===----------------------------------------------------------------------===//

#ifndef PAW_PATTERN_CHECKER_H
#define PAW_PATTERN_CHECKER_H

#include "semantic_context.h"
#include <vector>

namespace pawc {

class Pattern;
class Type;
class MatchExpr;

/// PatternChecker - 模式检查器
///
/// 负责验证：
/// - match表达式的穷尽性
/// - 模式类型的正确性
/// - 模式变量的作用域
class PatternChecker {
    SemanticContext* context_;
    
public:
    explicit PatternChecker(SemanticContext* context);
    
    /// 检查match表达式的穷尽性
    bool checkExhaustiveness(MatchExpr* match_expr);
    
    /// 检查模式是否覆盖类型的所有可能值
    bool isExhaustive(const std::vector<Pattern*>& patterns, Type* type);
    
    /// 检查模式类型
    bool checkPatternType(Pattern* pattern, Type* expected_type);
    
    /// 收集模式中的变量
    void collectPatternVariables(Pattern* pattern, 
                                std::vector<std::pair<std::string, Type*>>& variables);
};

} // namespace pawc

#endif // PAW_PATTERN_CHECKER_H

