#ifndef PAWC_LEXER_H
#define PAWC_LEXER_H

#include "pawc/common.h"
#include "pawc/string_pool.h"
#include <string>
#include <vector>

namespace pawc {

class Lexer {
public:
    explicit Lexer(const std::string& source, const std::string& filename = "<stdin>", StringPool* string_pool = nullptr);
    
    // 获取所有tokens
    std::vector<Token> tokenize();
    
    // 获取下一个token
    Token nextToken();
    
    // 查看下一个token但不消费
    Token peekToken();
    
    // 是否到达文件末尾
    bool isAtEnd() const;
    
    private:
        std::string source_;
        std::string filename_;
        StringPool* string_pool_;  // 可选的字符串池
        size_t current_;
        int line_;
        int column_;
    
    // 辅助方法
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    void skipWhitespace();
    void skipComment();
    
    // Token生成方法
    Token makeToken(TokenType type, const std::string& value);
    Token identifier();
    Token number();
    Token string();
    Token charLiteral();
    
    // 关键字检查
    TokenType checkKeyword(const std::string& text);
};

} // namespace pawc

#endif // PAWC_LEXER_H



