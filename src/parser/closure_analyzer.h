/**
 * @file closure_analyzer.h
 * @brief 闭包环境捕获分析器
 */

#ifndef PAWC_CLOSURE_ANALYZER_H
#define PAWC_CLOSURE_ANALYZER_H

#include "ast.h"
#include <vector>
#include <set>
#include <string>

namespace pawc {

class ClosureAnalyzer {
public:
    /**
     * @brief 分析闭包捕获的变量
     * @param closure 闭包表达式
     * @param available_vars 当前作用域中可用的变量
     * @return 需要捕获的变量列表
     */
    static std::vector<std::string> analyzeCapturedVars(
        const ClosureExpr* closure,
        const std::set<std::string>& available_vars
    );
    
private:
    static void analyzeExpr(const Expr* expr, std::set<std::string>& used_vars);
    static void analyzeStmt(const Stmt* stmt, std::set<std::string>& used_vars);
    static void removeLocalVars(const Stmt* stmt, std::set<std::string>& used_vars);
};

} // namespace pawc

#endif // PAWC_CLOSURE_ANALYZER_H

