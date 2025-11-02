//===--- parser.h - Parser ----------------------------------------*- C++ -*-===//

#ifndef PAW_PARSER_H
#define PAW_PARSER_H

#include "ast/ast_base.h"
#include "ast/expr.h"
#include "ast/stmt.h"
#include "frontend/lexer/token.h"
#include "diagnostics/diagnostic_engine.h"
#include "middleend/types/type_system.h"
#include <vector>

namespace pawc {

/// Parser - 语法分析器
class Parser {
public:
    Parser(const std::vector<Token>& tokens, DiagnosticEngine* diag_engine,
           TypeSystem* type_system);
    
    std::vector<StmtPtr> parse();
    
private:
    // Token处理
    Token advance();
    Token peek() const;
    Token peekNext() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    bool isAtEnd() const;
    
    // 语句解析
    StmtPtr parseStatement();
    StmtPtr parseVarDecl();
    StmtPtr parseFunctionDecl(bool is_public = false);
    StmtPtr parseTypeDecl(bool is_public = false);
    StmtPtr parseStructDecl(const std::string& name, std::vector<GenericParam> generic_params);
    StmtPtr parseEnumDecl(const std::string& name, std::vector<GenericParam> generic_params);
    StmtPtr parseInterfaceDecl(const std::string& name, std::vector<GenericParam> generic_params);
    StmtPtr parseSupportDecl();
    StmtPtr parseReturnStmt();
    StmtPtr parseIfStmt();
    StmtPtr parseLoopStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForStmt();
    StmtPtr parseBreakStmt();
    StmtPtr parseContinueStmt();
    StmtPtr parseBlockStmt();
    StmtPtr parseExprStmt();
    std::vector<StmtPtr> parseBlockStmtsWithImplicitReturn();
    
    // Pattern解析
    std::unique_ptr<class Pattern> parsePattern();
    std::unique_ptr<class Pattern> parseLiteralPattern();
    std::unique_ptr<class Pattern> parseVariableOrEnumPattern();
    std::unique_ptr<class Pattern> parseStructPattern(const std::string& struct_name);
    std::unique_ptr<class Pattern> parseEnumConstructorPattern();
    std::unique_ptr<class Pattern> parseTuplePattern();
    
    // 表达式解析（按优先级）
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseMatchExpr();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseRange();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    
    // 类型解析
    Type* parseType();
    
    // 泛型参数和where约束解析
    std::vector<GenericParam> parseGenericParams();        // 解析 <T, U>
    std::vector<WhereClause> parseWhereClauses();          // 解析 where T: Display, U: Debug
    
    // 错误处理
    void error(const std::string& message);
    void synchronize();
    
    // 辅助方法
    std::string normalizeEnumAlias(const std::string& alias);
    
private:
    const std::vector<Token>& tokens_;
    size_t current_;
    DiagnosticEngine* diag_engine_;
    TypeSystem* type_system_;
    
    // 当前泛型上下文
    std::vector<GenericParam> current_generic_params_;
};

} // namespace pawc

#endif // PAW_PARSER_H
