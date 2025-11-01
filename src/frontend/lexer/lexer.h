//===--- lexer.h - Lexical Analyzer ------------------------------*- C++ -*-===//
//
// PawLang Compiler - Lexer
//
//===----------------------------------------------------------------------===//

#ifndef PAW_LEXER_H
#define PAW_LEXER_H

#include "token.h"
#include "diagnostics/diagnostic_engine.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace pawc {

/// Lexer - 词法分析器
class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename, 
          DiagnosticEngine* diag_engine);
    
    // 词法分析主入口
    std::vector<Token> tokenize();
    
private:
    // 扫描单个token
    Token scanToken();
    
    // 字符处理
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    bool isAtEnd() const;
    
    // 跳过空白和注释
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    
    // 扫描不同类型的token
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString();
    Token scanChar();
    
    // 关键字查找
    TokenType identifierType(const std::string& text);
    
    // 创建token
    Token makeToken(TokenType type);
    Token makeToken(TokenType type, const std::string& lexeme);
    Token errorToken(const std::string& message);
    
    SourceLocation currentLocation() const;
    
private:
    std::string source_;
    std::string filename_;
    DiagnosticEngine* diag_engine_;
    
    size_t start_;      // 当前token起始位置
    size_t current_;    // 当前扫描位置
    size_t line_;       // 当前行号
    size_t column_;     // 当前列号
    
    // 关键字表
    static const std::unordered_map<std::string, TokenType> keywords_;
};

} // namespace pawc

#endif // PAW_LEXER_H
