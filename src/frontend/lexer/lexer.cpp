//===--- lexer.cpp - Lexer Implementation ------------------------*- C++ -*-===//

#include "lexer.h"
#include <cctype>

namespace pawc {

// 关键字映射表
const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"as", TokenType::AS},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"else", TokenType::ELSE},
    {"enum", TokenType::ENUM},
    {"err", TokenType::ERR},
    {"false", TokenType::FALSE},
    {"fn", TokenType::FN},
    {"if", TokenType::IF},
    {"import", TokenType::IMPORT},
    {"in", TokenType::IN},
    {"interface", TokenType::INTERFACE},
    {"is", TokenType::IS},
    {"let", TokenType::LET},
    {"loop", TokenType::LOOP},
    {"match", TokenType::MATCH},
    {"null", TokenType::NULL_KW},
    {"ok", TokenType::OK},
    {"return", TokenType::RETURN},
    {"self", TokenType::SELF_LOWER},
    {"Self", TokenType::SELF_UPPER},
    {"struct", TokenType::STRUCT},
    {"support", TokenType::SUPPORT},
    {"true", TokenType::TRUE},
    {"type", TokenType::TYPE},
    {"where", TokenType::WHERE},
    {"with", TokenType::WITH},
};

Lexer::Lexer(const std::string& source, const std::string& filename,
             DiagnosticEngine* diag_engine)
    : source_(source), filename_(filename), diag_engine_(diag_engine),
      start_(0), current_(0), line_(1), column_(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        start_ = current_;
        Token token = scanToken();
        if (token.type != TokenType::INVALID) {
            tokens.push_back(token);
        }
    }
    
    // 添加EOF token
    tokens.push_back(Token(TokenType::END_OF_FILE, "", currentLocation()));
    
    return tokens;
}

Token Lexer::scanToken() {
    skipWhitespace();
    
    if (isAtEnd()) {
        return makeToken(TokenType::END_OF_FILE);
    }
    
    start_ = current_;
    char c = advance();
    
    // 标识符或关键字
    if (std::isalpha(c) || c == '_') {
        return scanIdentifierOrKeyword();
    }
    
    // 数字
    if (std::isdigit(c)) {
        return scanNumber();
    }
    
    // 单字符和双字符token
    switch (c) {
        case '(': return makeToken(TokenType::LPAREN);
        case ')': return makeToken(TokenType::RPAREN);
        case '{': return makeToken(TokenType::LBRACE);
        case '}': return makeToken(TokenType::RBRACE);
        case '[': return makeToken(TokenType::LBRACKET);
        case ']': return makeToken(TokenType::RBRACKET);
        case ',': return makeToken(TokenType::COMMA);
        case ';': return makeToken(TokenType::SEMICOLON);
        case '@': return makeToken(TokenType::AT);
        case '~': return makeToken(TokenType::TILDE);
        case '?': return makeToken(TokenType::QUESTION);
        
        case '+':
            return makeToken(match('=') ? TokenType::PLUS_EQ : TokenType::PLUS);
        case '-':
            if (match('=')) return makeToken(TokenType::MINUS_EQ);
            if (match('>')) return makeToken(TokenType::ARROW);
            return makeToken(TokenType::MINUS);
        case '*':
            return makeToken(match('=') ? TokenType::STAR_EQ : TokenType::STAR);
        case '/':
            return makeToken(match('=') ? TokenType::SLASH_EQ : TokenType::SLASH);
        case '%':
            return makeToken(match('=') ? TokenType::PERCENT_EQ : TokenType::PERCENT);
        case '^':
            return makeToken(match('=') ? TokenType::CARET_EQ : TokenType::CARET);
        case '!':
            return makeToken(match('=') ? TokenType::NOT_EQ : TokenType::BANG);
        case '=':
            if (match('=')) return makeToken(TokenType::EQ_EQ);
            if (match('>')) return makeToken(TokenType::FAT_ARROW);
            return makeToken(TokenType::EQ);
        case '<':
            if (match('=')) return makeToken(TokenType::LESS_EQ);
            if (match('<')) return makeToken(TokenType::LT_LT);
            return makeToken(TokenType::LESS);
        case '>':
            if (match('=')) return makeToken(TokenType::GREATER_EQ);
            if (match('>')) return makeToken(TokenType::GT_GT);
            return makeToken(TokenType::GREATER);
        case '&':
            if (match('&')) return makeToken(TokenType::AND_AND);
            if (match('=')) return makeToken(TokenType::AMP_EQ);
            return makeToken(TokenType::AMP);
        case '|':
            if (match('|')) return makeToken(TokenType::OR_OR);
            if (match('=')) return makeToken(TokenType::PIPE_EQ);
            return makeToken(TokenType::PIPE);
        case ':':
            return makeToken(match(':') ? TokenType::COLON_COLON : TokenType::COLON);
        case '.':
            return makeToken(match('.') ? TokenType::DOT_DOT : TokenType::DOT);
        
        case '"':
            return scanString();
        case '\'':
            return scanChar();
        
        default:
            return errorToken("Unexpected character");
    }
}

char Lexer::advance() {
    char c = source_[current_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

char Lexer::peek() const {
    return isAtEnd() ? '\0' : source_[current_];
}

char Lexer::peekNext() const {
    return (current_ + 1 >= source_.length()) ? '\0' : source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[current_] != expected) {
        return false;
    }
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return current_ >= source_.length();
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            skipLineComment();
        } else if (c == '/' && peekNext() == '*') {
            skipBlockComment();
        } else {
            break;
        }
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    advance(); // '/'
    advance(); // '*'
    
    while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') {
            advance(); // '*'
            advance(); // '/'
            break;
        }
        advance();
    }
}

Token Lexer::scanIdentifierOrKeyword() {
    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }
    
    std::string text = source_.substr(start_, current_ - start_);
    TokenType type = identifierType(text);
    
    return makeToken(type, text);
}

Token Lexer::scanNumber() {
    bool is_float = false;
    
    // 扫描整数部分
    while (std::isdigit(peek())) {
        advance();
    }
    
    // 检查小数点
    if (peek() == '.' && std::isdigit(peekNext())) {
        is_float = true;
        advance(); // '.'
        while (std::isdigit(peek())) {
            advance();
        }
    }
    
    // 检查指数
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        while (std::isdigit(peek())) {
            advance();
        }
    }
    
    std::string text = source_.substr(start_, current_ - start_);
    TokenType type = is_float ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL;
    
    return makeToken(type, text);
}

Token Lexer::scanString() {
    std::string value;
    
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (!isAtEnd()) {
                char c = advance();
                switch (c) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': value += '"'; break;
                    default: value += c; break;
                }
            }
        } else {
            value += advance();
        }
    }
    
    if (isAtEnd()) {
        return errorToken("Unterminated string");
    }
    
    advance(); // closing '"'
    
    return makeToken(TokenType::STRING_LITERAL, value);
}

Token Lexer::scanChar() {
    if (isAtEnd()) {
        return errorToken("Unterminated character literal");
    }
    
    char c = advance();
    
    // 处理转义字符
    if (c == '\\') {
        if (isAtEnd()) {
            return errorToken("Unterminated character literal");
        }
        c = advance();
        switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            default: break;
        }
    }
    
    if (isAtEnd() || peek() != '\'') {
        return errorToken("Unterminated character literal");
    }
    
    advance(); // closing '\''
    
    return makeToken(TokenType::CHAR_LITERAL, std::string(1, c));
}

TokenType Lexer::identifierType(const std::string& text) {
    auto it = keywords_.find(text);
    if (it != keywords_.end()) {
        return it->second;
    }
    return TokenType::IDENTIFIER;
}

Token Lexer::makeToken(TokenType type) {
    std::string text = source_.substr(start_, current_ - start_);
    return Token(type, text, currentLocation());
}

Token Lexer::makeToken(TokenType type, const std::string& lexeme) {
    return Token(type, lexeme, currentLocation());
}

Token Lexer::errorToken(const std::string& message) {
    if (diag_engine_) {
        diag_engine_->reportError(message, currentLocation());
    }
    return Token(TokenType::INVALID, "", currentLocation());
}

SourceLocation Lexer::currentLocation() const {
    return SourceLocation{filename_, static_cast<int>(line_), static_cast<int>(column_)};
}

} // namespace pawc
