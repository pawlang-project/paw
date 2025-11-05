//===--- parser.h - Recursive Descent Parser -------------------*- C++ -*-===//
//
// Recursive descent parser for PawLang.
//
// Transforms token stream into Abstract Syntax Tree (AST).
//
// Architecture:
//   - Recursive descent with operator precedence climbing
//   - One parse method per grammar rule
//   - Error recovery with synchronization points
//   - Panic mode for multiple error reporting
//
// Features:
//   - All PawLang constructs: expressions, statements, declarations
//   - Advanced pattern matching: range, OR, guards, nested
//   - Generic types with where clauses
//   - Interface system with default methods
//
// Performance: O(n) single-pass parsing
//
//===----------------------------------------------------------------------===//

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

/// Parser - Syntax analyzer for PawLang
///
/// Converts token stream to AST using recursive descent.
///
/// Usage:
///   Parser parser(tokens, diag, types);
///   auto ast = parser.parse();
class Parser {
public:
    Parser(const std::vector<Token>& tokens, DiagnosticEngine* diag_engine,
           TypeSystem* type_system);
    
    std::vector<StmtPtr> parse();
    
private:
    // Tokenprocess
    Token advance();
    Token peek() const;
    Token peekNext() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    bool isAtEnd() const;
    
    // statementparse
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
    
    // Patternparse
    std::unique_ptr<class Pattern> parsePattern();
    std::unique_ptr<class Pattern> parseBasePattern();
    std::unique_ptr<class Pattern> parseLiteralPattern();
    std::unique_ptr<class Pattern> parseVariableOrEnumPattern();
    std::unique_ptr<class Pattern> parseStructPattern(const std::string& struct_name);
    std::unique_ptr<class Pattern> parseArrayPattern();
    std::unique_ptr<class Pattern> parseSlicePattern(std::vector<std::unique_ptr<class Pattern>> prefix);
    std::unique_ptr<class Pattern> parseRangePattern(std::unique_ptr<class Pattern> start);
    std::unique_ptr<class Pattern> parseEnumConstructorPattern();
    std::unique_ptr<class Pattern> parseTuplePattern();
    
    // Expression parsing (by operator precedence)
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
    
    // typeparse
    Type* parseType();
    
    // genericparameterandwhereconstraintparse
    std::vector<GenericParam> parseGenericParams();        // parsing <T, U>
    std::vector<WhereClause> parseWhereClauses();          // parsing where T: Display, U: Debug
    
    // errorprocess
    void error(const std::string& message);
    void synchronize();
    
    // helpermethod
    std::string normalizeEnumAlias(const std::string& alias);
    
private:
    const std::vector<Token>& tokens_;
    size_t current_;
    DiagnosticEngine* diag_engine_;
    TypeSystem* type_system_;
    
    // currentgenericcontext
    std::vector<GenericParam> current_generic_params_;
};

} // namespace pawc

#endif // PAW_PARSER_H
