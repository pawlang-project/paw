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

/// TokenType - Alltokentypes
enum class TokenType {
    // Special tokens
    END_OF_FILE,
    INVALID,
    
    // Identifiers and literals
    IDENTIFIER,       // abc, _name
    INT_LITERAL,      // 123, 0x1F
    FLOAT_LITERAL,    // 3.14, 1e-10
    STRING_LITERAL,   // "hello"
    CHAR_LITERAL,     // 'a'
    
    // Keywords (alphabetical order)
    AS,              // as
    BREAK,           // break
    CONTINUE,        // continue
    ELSE,            // else
    ENUM,            // enum
    ERR,             // err (Result error constructor)
    FALSE,           // false
    FN,              // fn
    IF,              // if
    IMPORT,          // import
    IN,              // in
    INTERFACE,       // interface
    IS,              // is (match pattern matching)
    LET,             // let
    LOOP,            // loop (unified loop)
    NONE,            // none (Optional empty value constructor)
    NULL_KW,         // null (Optional literal)
    OK,              // ok (Result success constructor)
    PUB,             // pub (public visibility)
    RETURN,          // return
    SELF_LOWER,      // self
    SELF_UPPER,      // Self
    SOME,            // some (Optional value constructor function name)
    STRUCT,          // struct
    SUPPORT,         // support (interface implementation)
    TRUE,            // true
    TYPE,            // type (unified type definition)
    WHERE,           // where (generic constraints)
    WITH,            // with (support...with)
    
    // Operators (single character)
    PLUS,            // +
    MINUS,           // -
    STAR,            // *
    SLASH,           // /
    PERCENT,         // %
    AMP,             // &
    PIPE,            // |
    CARET,           // ^
    TILDE,           // ~ (mutability marker)
    BANG,            // ! (also used forT!)
    QUESTION,        // ? (used forT?)
    EQ,              // =
    LT,              // <
    GT,              // >
    DOT,             // .
    COMMA,           // ,
    COLON,           // :
    SEMICOLON,       // ;
    AT,              // @
    
    // Operators (two characters)
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
    COLON_COLON,     // :: (staticvisit)
    DOT_DOT,         // .. (range)
    ARROW,           // -> (functionreturntypes)
    FAT_ARROW,       // => (matchbranch)
    
    // Delimiters
    LPAREN,          // (
    RPAREN,          // )
    LBRACE,          // {
    RBRACE,          // }
    LBRACKET,        // [
    RBRACKET,        // ]
};

/// Token - Tokenstruct
struct Token {
    TokenType type;
    std::string lexeme;
    SourceLocation location;
    
    Token(TokenType t, const std::string& lex, const SourceLocation& loc)
        : type(t), lexeme(lex), location(loc) {}
    
    // determineyesnoyesliteral
    bool isLiteral() const {
        return type == TokenType::INT_LITERAL ||
               type == TokenType::FLOAT_LITERAL ||
               type == TokenType::STRING_LITERAL ||
               type == TokenType::CHAR_LITERAL ||
               type == TokenType::TRUE ||
               type == TokenType::FALSE;
    }
    
    // determineyesnoyesoperator
    bool isOperator() const {
        return type >= TokenType::PLUS && type <= TokenType::FAT_ARROW;
    }
    
    // determineyesnoyeskeyword
    bool isKeyword() const {
        return type >= TokenType::AS && type <= TokenType::WITH;
    }
    
    // Convert to string
    std::string toString() const;
};

// Token type to string
const char* tokenTypeToString(TokenType type);

} // namespace pawc

#endif // PAW_TOKEN_H
