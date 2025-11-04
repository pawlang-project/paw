//===--- token.cpp - Token Implementation ------------------------*- C++ -*-===//

#include "token.h"

namespace pawc {

const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::INVALID: return "INVALID";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INT_LITERAL: return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::CHAR_LITERAL: return "CHAR_LITERAL";
        
        case TokenType::AS: return "AS";
        case TokenType::BREAK: return "BREAK";
        case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::ELSE: return "ELSE";
        case TokenType::ENUM: return "ENUM";
        case TokenType::FALSE: return "FALSE";
        case TokenType::FN: return "FN";
        case TokenType::IF: return "IF";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::IN: return "IN";
        case TokenType::INTERFACE: return "INTERFACE";
        case TokenType::IS: return "IS";
        case TokenType::LET: return "LET";
        case TokenType::LOOP: return "LOOP";
        case TokenType::NULL_KW: return "NULL_KW (deprecated)";
        case TokenType::OK: return "OK";
        case TokenType::ERR: return "ERR";
        case TokenType::PUB: return "PUB";
        case TokenType::RETURN: return "RETURN";
        case TokenType::SELF_LOWER: return "SELF_LOWER";
        case TokenType::SELF_UPPER: return "SELF_UPPER";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::SUPPORT: return "SUPPORT";
        case TokenType::TRUE: return "TRUE";
        case TokenType::TYPE: return "TYPE";
        case TokenType::WHERE: return "WHERE";
        case TokenType::WITH: return "WITH";
        
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::AMP: return "AMP";
        case TokenType::PIPE: return "PIPE";
        case TokenType::CARET: return "CARET";
        case TokenType::TILDE: return "TILDE";
        case TokenType::BANG: return "BANG";
        case TokenType::QUESTION: return "QUESTION";
        case TokenType::EQ: return "EQ";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::DOT: return "DOT";
        case TokenType::COMMA: return "COMMA";
        case TokenType::COLON: return "COLON";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::AT: return "AT";
        
        case TokenType::PLUS_EQ: return "PLUS_EQ";
        case TokenType::MINUS_EQ: return "MINUS_EQ";
        case TokenType::STAR_EQ: return "STAR_EQ";
        case TokenType::SLASH_EQ: return "SLASH_EQ";
        case TokenType::PERCENT_EQ: return "PERCENT_EQ";
        case TokenType::AMP_EQ: return "AMP_EQ";
        case TokenType::PIPE_EQ: return "PIPE_EQ";
        case TokenType::CARET_EQ: return "CARET_EQ";
        case TokenType::EQ_EQ: return "EQ_EQ";
        case TokenType::NOT_EQ: return "NOT_EQ";
        case TokenType::LESS_EQ: return "LESS_EQ";
        case TokenType::GREATER_EQ: return "GREATER_EQ";
        case TokenType::AND_AND: return "AND_AND";
        case TokenType::OR_OR: return "OR_OR";
        case TokenType::LT_LT: return "LT_LT";
        case TokenType::GT_GT: return "GT_GT";
        case TokenType::COLON_COLON: return "COLON_COLON";
        case TokenType::DOT_DOT: return "DOT_DOT";
        case TokenType::ARROW: return "ARROW";
        case TokenType::FAT_ARROW: return "FAT_ARROW";
        
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        
        default: return "UNKNOWN";
    }
}

std::string Token::toString() const {
    return std::string(tokenTypeToString(type)) + "(" + lexeme + ")";
}

} // namespace pawc

