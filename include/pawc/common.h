#ifndef PAWC_COMMON_H
#define PAWC_COMMON_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <optional>
#include <variant>

namespace pawc {

// Source code location information
struct SourceLocation {
    std::string filename;
    int line;
    int column;
    
    SourceLocation(const std::string& file = "", int l = 0, int c = 0)
        : filename(file), line(l), column(c) {}
};

// Token types
enum class TokenType {
    // Keywords
    KW_FN,          // fn
    KW_LET,         // let
    KW_MUT,         // mut
    KW_TYPE,        // type
    KW_STRUCT,      // struct
    KW_ENUM,        // enum
    KW_IMPL,        // impl
    KW_IF,          // if
    KW_ELSE,        // else
    KW_LOOP,        // loop
    KW_BREAK,       // break
    KW_CONTINUE,    // continue
    KW_RETURN,      // return
    KW_IMPORT,      // import
    KW_PUB,         // pub
    KW_EXTERN,      // extern
    KW_SELF,        // self
    KW_SELF_TYPE,   // Self
    KW_TRUE,        // true
    KW_FALSE,       // false
    KW_IN,          // in
    KW_IS,          // is (for pattern matching)
    KW_AS,          // as
    KW_OK,          // ok (error handling)
    KW_ERR,         // err (error handling)
    
    // Identifiers and literals
    IDENTIFIER,
    INTEGER,
    FLOAT,
    STRING,
    CHAR,
    
    // Operators
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    EQ,             // ==
    NE,             // !=
    LT,             // <
    LE,             // <=
    GT,             // >
    GE,             // >=
    AND,            // &&
    OR,             // ||
    NOT,            // !
    ASSIGN,         // =
    PLUS_EQ,        // +=
    MINUS_EQ,       // -=
    ARROW,          // ->
    FAT_ARROW,      // =>
    QUESTION,       // ? (error handling)
    
    // Delimiters
    LPAREN,         // (
    RPAREN,         // )
    LBRACE,         // {
    RBRACE,         // }
    LBRACKET,       // [
    RBRACKET,       // ]
    COMMA,          // ,
    SEMICOLON,      // ;
    COLON,          // :
    DOT,            // .
    DOTDOT,         // ..
    DOUBLE_COLON,   // ::
    
    // Special
    END_OF_FILE,
    INVALID
};

// Token
struct Token {
    TokenType type;
    std::string value;
    SourceLocation location;
    
    Token(TokenType t, const std::string& v, const SourceLocation& loc)
        : type(t), value(v), location(loc) {}
};

// Primitive types
enum class PrimitiveType {
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64,
    BOOL, CHAR, STRING, VOID
    // Note: size type is not exposed to users, stdlib uses i64 directly
};

// Compiler error
struct CompilerError {
    std::string message;
    SourceLocation location;
    
    CompilerError(const std::string& msg, const SourceLocation& loc)
        : message(msg), location(loc) {}
};

} // namespace pawc

#endif // PAWC_COMMON_H



