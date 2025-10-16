#include "lexer.h"
#include <cctype>
#include <unordered_map>

namespace pawc {

static const std::unordered_map<std::string, TokenType> keywords = {
    {"fn", TokenType::KW_FN},
    {"let", TokenType::KW_LET},
    {"mut", TokenType::KW_MUT},
    {"type", TokenType::KW_TYPE},
    {"struct", TokenType::KW_STRUCT},
    {"enum", TokenType::KW_ENUM},
    // {"impl", TokenType::KW_IMPL},  // 已废弃：现在方法直接在struct内定义
    {"if", TokenType::KW_IF},
    {"else", TokenType::KW_ELSE},
    {"loop", TokenType::KW_LOOP},
    {"break", TokenType::KW_BREAK},
    {"continue", TokenType::KW_CONTINUE},
    {"return", TokenType::KW_RETURN},
        {"pub", TokenType::KW_PUB},
        {"import", TokenType::KW_IMPORT},
        {"extern", TokenType::KW_EXTERN},
    {"self", TokenType::KW_SELF},
    {"Self", TokenType::KW_SELF_TYPE},
    {"true", TokenType::KW_TRUE},
    {"false", TokenType::KW_FALSE},
    {"in", TokenType::KW_IN},
    {"is", TokenType::KW_IS},
    {"as", TokenType::KW_AS},
    {"ok", TokenType::KW_OK},
    {"err", TokenType::KW_ERR}
};

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename), current_(0), line_(1), column_(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        Token token = nextToken();
        if (token.type != TokenType::INVALID) {
            tokens.push_back(token);
        }
        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }
    return tokens;
}

Token Lexer::nextToken() {
    skipWhitespace();
    
    if (isAtEnd()) {
        return makeToken(TokenType::END_OF_FILE, "");
    }
    
    char c = advance();
    
    // 标识符或关键字
    if (std::isalpha(c) || c == '_') {
        current_--;
        column_--;
        return identifier();
    }
    
    // 数字
    if (std::isdigit(c)) {
        current_--;
        column_--;
        return number();
    }
    
    // 字符串
    if (c == '"') {
        return string();
    }
    
    // 字符
    if (c == '\'') {
        return charLiteral();
    }
    
    // 运算符和分隔符
    switch (c) {
        case '+':
            return makeToken(match('=') ? TokenType::PLUS_EQ : TokenType::PLUS, std::string(1, c));
        case '-':
            if (match('>')) return makeToken(TokenType::ARROW, "->");
            if (match('=')) return makeToken(TokenType::MINUS_EQ, "-=");
            return makeToken(TokenType::MINUS, "-");
        case '*': return makeToken(TokenType::STAR, "*");
        case '/':
            if (peek() == '/') {
                skipComment();
                return nextToken();
            }
            return makeToken(TokenType::SLASH, "/");
        case '%': return makeToken(TokenType::PERCENT, "%");
        case '=':
            if (match('=')) return makeToken(TokenType::EQ, "==");
            if (match('>')) return makeToken(TokenType::FAT_ARROW, "=>");
            return makeToken(TokenType::ASSIGN, "=");
        case '!':
            return makeToken(match('=') ? TokenType::NE : TokenType::NOT, std::string(1, c));
        case '<':
            return makeToken(match('=') ? TokenType::LE : TokenType::LT, std::string(1, c));
        case '>':
            return makeToken(match('=') ? TokenType::GE : TokenType::GT, std::string(1, c));
        case '?':
            return makeToken(TokenType::QUESTION, "?");
        case '&':
            if (match('&')) return makeToken(TokenType::AND, "&&");
            break;
        case '|':
            if (match('|')) return makeToken(TokenType::OR, "||");
            break;
        case '(': return makeToken(TokenType::LPAREN, "(");
        case ')': return makeToken(TokenType::RPAREN, ")");
        case '{': return makeToken(TokenType::LBRACE, "{");
        case '}': return makeToken(TokenType::RBRACE, "}");
        case '[': return makeToken(TokenType::LBRACKET, "[");
        case ']': return makeToken(TokenType::RBRACKET, "]");
        case ',': return makeToken(TokenType::COMMA, ",");
        case ';': return makeToken(TokenType::SEMICOLON, ";");
        case ':':
            return makeToken(match(':') ? TokenType::DOUBLE_COLON : TokenType::COLON, ":");
        case '.':
            if (peek() == '.') {
                advance();
                return makeToken(TokenType::DOTDOT, "..");
            }
            return makeToken(TokenType::DOT, ".");
    }
    
    return makeToken(TokenType::INVALID, std::string(1, c));
}

Token Lexer::peekToken() {
    size_t saved_current = current_;
    int saved_line = line_;
    int saved_column = column_;
    
    Token token = nextToken();
    
    current_ = saved_current;
    line_ = saved_line;
    column_ = saved_column;
    
    return token;
}

bool Lexer::isAtEnd() const {
    return current_ >= source_.length();
}

char Lexer::advance() {
    column_++;
    return source_[current_++];
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[current_];
}

char Lexer::peekNext() const {
    if (current_ + 1 >= source_.length()) return '\0';
    return source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source_[current_] != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            line_++;
            column_ = 0;
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skipComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    return Token(type, value, SourceLocation(filename_, line_, column_));
}

Token Lexer::identifier() {
    size_t start = current_;
    int start_column = column_;
    
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }
    
    std::string text = source_.substr(start, current_ - start);
    TokenType type = checkKeyword(text);
    
    return Token(type, text, SourceLocation(filename_, line_, start_column));
}

Token Lexer::number() {
    size_t start = current_;
    int start_column = column_;
    bool is_float = false;
    
    while (!isAtEnd() && std::isdigit(peek())) {
        advance();
    }
    
    if (peek() == '.' && std::isdigit(peekNext())) {
        is_float = true;
        advance(); // consume '.'
        while (!isAtEnd() && std::isdigit(peek())) {
            advance();
        }
    }
    
    std::string text = source_.substr(start, current_ - start);
    TokenType type = is_float ? TokenType::FLOAT : TokenType::INTEGER;
    
    return Token(type, text, SourceLocation(filename_, line_, start_column));
}

Token Lexer::string() {
    size_t start = current_;
    int start_column = column_ - 1;
    std::string value;
    
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (!isAtEnd()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': value += '"'; break;
                    default: value += escaped;
                }
            }
        } else {
            value += advance();
        }
    }
    
    if (isAtEnd()) {
        return makeToken(TokenType::INVALID, "Unterminated string");
    }
    
    advance(); // closing "
    
    return Token(TokenType::STRING, value, SourceLocation(filename_, line_, start_column));
}

Token Lexer::charLiteral() {
    int start_column = column_ - 1;
    
    if (isAtEnd()) {
        return makeToken(TokenType::INVALID, "Unterminated char");
    }
    
    char c = advance();
    if (c == '\\' && !isAtEnd()) {
        char escaped = advance();
        switch (escaped) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            default: c = escaped;
        }
    }
    
    if (isAtEnd() || peek() != '\'') {
        return makeToken(TokenType::INVALID, "Unterminated char");
    }
    
    advance(); // closing '
    
    return Token(TokenType::CHAR, std::string(1, c), SourceLocation(filename_, line_, start_column));
}

TokenType Lexer::checkKeyword(const std::string& text) {
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::IDENTIFIER;
}

} // namespace pawc



