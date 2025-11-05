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

/// Lexer - Lexical analyzer for PawLang
class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename, 
          DiagnosticEngine* diag_engine);
    
    /// Main entry point for lexical analysis
    std::vector<Token> tokenize();
    
private:
    /// Scan a single token
    Token scanToken();
    
    /// Character processing utilities
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    bool isAtEnd() const;
    
    /// Skip whitespace and comments
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    
    /// Scan different token types
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString();
    Token scanChar();
    
    /// Keyword lookup
    TokenType identifierType(const std::string& text);
    
    /// Create token
    Token makeToken(TokenType type);
    Token makeToken(TokenType type, const std::string& lexeme);
    Token errorToken(const std::string& message);
    
    SourceLocation currentLocation() const;
    
private:
    std::string source_;
    std::string filename_;
    DiagnosticEngine* diag_engine_;
    
     size_t start_;      // Current token start position
     size_t current_;    // Current scanning position
     size_t line_;       // Current line number
     size_t column_;     // Current column number
    
    // Keyword table
    static const std::unordered_map<std::string, TokenType> keywords_;
};

} // namespace pawc

#endif // PAW_LEXER_H
