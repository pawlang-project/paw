//===--- token.h - Token Definitions -----------------------------*- C++ -*-===//
//
// PawLang Compiler - Lexer Tokens
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TOKEN_H
#define PAW_TOKEN_H

#include "diagnostics/source_location.h"
#include <string>

namespace pawc {

/// TokenType - 所有token类型
enum class TokenType {
    // 特殊token
    END_OF_FILE,
    INVALID,
    
    // 标识符和字面量
    IDENTIFIER,       // abc, _name
    INT_LITERAL,      // 123, 0x1F
    FLOAT_LITERAL,    // 3.14, 1e-10
    STRING_LITERAL,   // "hello"
    CHAR_LITERAL,     // 'a'
    
    // 关键字（按字母顺序）
    AS,              // as
    BREAK,           // break
    CONTINUE,        // continue
    ELSE,            // else
    ENUM,            // enum
    ERR,             // err (Result错误构造器)
    FALSE,           // false
    FN,              // fn
    IF,              // if
    IMPORT,          // import
    IN,              // in
    INTERFACE,       // interface
    IS,              // is (match模式匹配)
    LET,             // let
    LOOP,            // loop (统一的循环)
    NONE,            // none (Optional空值构造器)
    NULL_KW,         // null (Optional字面量)
    OK,              // ok (Result成功构造器)
    PUB,             // pub (公开可见性)
    RETURN,          // return
    SELF_LOWER,      // self
    SELF_UPPER,      // Self
    SOME,            // some (Optional值构造器函数名)
    STRUCT,          // struct
    SUPPORT,         // support (接口实现)
    TRUE,            // true
    TYPE,            // type (统一的类型定义)
    WHERE,           // where (泛型约束)
    WITH,            // with (support...with)
    
    // 运算符（单字符）
    PLUS,            // +
    MINUS,           // -
    STAR,            // *
    SLASH,           // /
    PERCENT,         // %
    AMP,             // &
    PIPE,            // |
    CARET,           // ^
    TILDE,           // ~ (可变性标记)
    BANG,            // ! (也用于T!)
    QUESTION,        // ? (用于T?)
    EQ,              // =
    LT,              // <
    GT,              // >
    DOT,             // .
    COMMA,           // ,
    COLON,           // :
    SEMICOLON,       // ;
    AT,              // @
    
    // 运算符（双字符）
    PLUS_EQ,         // +=
    MINUS_EQ,        // -=
    STAR_EQ,         // *=
    SLASH_EQ,        // /=
    PERCENT_EQ,      // %=
    AMP_EQ,          // &=
    PIPE_EQ,         // |=
    CARET_EQ,        // ^=
    EQ_EQ,           // ==
    NOT_EQ,          // !=
    LESS,            // <
    LESS_EQ,         // <=
    GREATER,         // >
    GREATER_EQ,      // >=
    AND_AND,         // &&
    OR_OR,           // ||
    LT_LT,           // <<
    GT_GT,           // >>
    COLON_COLON,     // :: (静态访问)
    DOT_DOT,         // .. (范围)
    ARROW,           // -> (函数返回类型)
    FAT_ARROW,       // => (match分支)
    
    // 分隔符
    LPAREN,          // (
    RPAREN,          // )
    LBRACE,          // {
    RBRACE,          // }
    LBRACKET,        // [
    RBRACKET,        // ]
};

/// Token - Token结构
struct Token {
    TokenType type;
    std::string lexeme;
    SourceLocation location;
    
    Token(TokenType t, const std::string& lex, const SourceLocation& loc)
        : type(t), lexeme(lex), location(loc) {}
    
    // 判断是否是字面量
    bool isLiteral() const {
        return type == TokenType::INT_LITERAL ||
               type == TokenType::FLOAT_LITERAL ||
               type == TokenType::STRING_LITERAL ||
               type == TokenType::CHAR_LITERAL ||
               type == TokenType::TRUE ||
               type == TokenType::FALSE;
    }
    
    // 判断是否是运算符
    bool isOperator() const {
        return type >= TokenType::PLUS && type <= TokenType::FAT_ARROW;
    }
    
    // 判断是否是关键字
    bool isKeyword() const {
        return type >= TokenType::AS && type <= TokenType::WITH;
    }
    
    // 转为字符串
    std::string toString() const;
};

// Token类型转字符串
const char* tokenTypeToString(TokenType type);

} // namespace pawc

#endif // PAW_TOKEN_H
