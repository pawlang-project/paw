#ifndef PAWC_PARSER_H
#define PAWC_PARSER_H

#include "ast.h"
#include "../lexer/lexer.h"
#include "pawc/error_reporter.h"
#include <vector>
#include <set>

namespace pawc {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens, ErrorReporter* reporter = nullptr);
    
    // 解析整个程序
    Program parse();
    
private:
    std::vector<Token> tokens_;
    size_t current_;
    ErrorReporter* error_reporter_;  // 使用ErrorReporter替代CompilerError
    std::vector<CompilerError> errors_;  // 保留用于向后兼容
    std::set<std::string> type_names_;  // 已注册的类型名（通用）
    std::set<std::string> struct_names_;  // 已定义的Struct名
    std::set<std::string> enum_names_;  // 已定义的Enum名
    std::set<std::string> mutable_vars_;  // 可变变量集合（let mut）
    std::string current_parsing_struct_;  // 当前正在解析的struct名（用于Self）
    
    // Token操作
    Token peek() const;
    Token previous() const;
    Token advance();
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool match(const std::vector<TokenType>& types);
    Token consume(TokenType type, const std::string& message);
    
    // 错误处理
    void error(const std::string& message);
    void synchronize();
    
    // 类型注册（符号表）
    void registerType(const std::string& name);
    bool isRegisteredType(const std::string& name) const;
    
    // 类型查询辅助方法
    bool isDefinedStruct(const std::string& name) const {
        return struct_names_.find(name) != struct_names_.end();
    }
    
    bool isDefinedEnum(const std::string& name) const {
        return enum_names_.find(name) != enum_names_.end();
    }
    
    // 解析方法
    StmtPtr statement();
    StmtPtr importDeclaration();
    StmtPtr externDeclaration();
    StmtPtr functionDeclaration(bool is_public = false);
    StmtPtr structDeclaration(Token name_token, std::vector<GenericParam> generic_params, bool is_public = false);
    StmtPtr enumDeclaration(Token name_token, std::vector<GenericParam> generic_params, bool is_public = false);
    StmtPtr typeAliasDeclaration(bool is_public = false);
    StmtPtr implDeclaration();
    StmtPtr letDeclaration();
    StmtPtr ifStatement();
    StmtPtr loopStatement();
    StmtPtr returnStatement();
    StmtPtr breakStatement();
    StmtPtr continueStatement();
    StmtPtr expressionStatement();
    StmtPtr blockStatement();
    
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr matchExpression();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr postfix();
    ExprPtr primary();
    
    TypePtr parseType();
    std::vector<GenericParam> parseGenericParams();
    Parameter parseParameter();
    PatternPtr parsePattern();
    MatchArm parseMatchArm();
};

} // namespace pawc

#endif // PAWC_PARSER_H

