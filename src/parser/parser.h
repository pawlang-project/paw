#ifndef PAWC_PARSER_H
#define PAWC_PARSER_H

#include "ast.h"
#include "../lexer/lexer.h"
#include "../diagnostics/diagnostic_engine.h"
#include <vector>
#include <set>

namespace pawc {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens, DiagnosticEngine* diag_engine, const std::string& filename);
    
    // 解析整个程序
    Program parse();
    
private:
    std::vector<Token> tokens_;
    size_t current_;
    DiagnosticEngine* diag_engine_;  // 诊断引擎
    std::string filename_;           // 当前文件名
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
    StmtPtr interfaceDeclaration(Token name_token, std::vector<GenericParam> generic_params, bool is_public = false);
    StmtPtr supportDeclaration();
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
    StmtPtr unsafeBlockStatement();
    
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
    
    TypePtr parseType(bool allow_slice = false);  // allow_slice: 是否允许[T]为切片（函数参数）
    TypePtr inferTypeFromExpr(const Expr* expr);  // 类型推断辅助函数
    TypePtr cloneType(const Type* type);  // 克隆类型节点
    std::vector<GenericParam> parseGenericParams();
    Parameter parseParameter();
    PatternPtr parsePattern();
    MatchArm parseMatchArm();
};

} // namespace pawc

#endif // PAWC_PARSER_H

