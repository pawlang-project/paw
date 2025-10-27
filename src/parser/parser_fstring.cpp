/**
 * @file parser_fstring.cpp
 * @brief F-String (字符串插值) 解析实现
 */

#include "parser.h"
#include <sstream>

namespace pawc {

ExprPtr Parser::parseFString() {
    Token fstr_token = previous();
    
    // 解码 token.value
    // 格式：<count>|part0|expr0|part1|expr1|...
    std::string encoded = fstr_token.value;
    std::vector<std::string> parts;
    std::vector<std::string> expr_strings;
    
    // 解析编码
    size_t pos = 0;
    size_t pipe_pos = encoded.find('|');
    
    if (pipe_pos == std::string::npos) {
        // 没有插值，只是普通 f-string
        return std::make_unique<StringExpr>(encoded, fstr_token.location);
    }
    
    std::string count_str = encoded.substr(0, pipe_pos);
    int part_count = std::stoi(count_str);
    
    pos = pipe_pos + 1;
    
    // 解析 parts 和 expressions
    for (int i = 0; i < part_count; ++i) {
        // 解析 part
        size_t next_pipe = encoded.find('|', pos);
        if (next_pipe == std::string::npos) {
            // 最后一个 part
            parts.push_back(encoded.substr(pos));
            break;
        }
        
        std::string part = encoded.substr(pos, next_pipe - pos);
        parts.push_back(part);
        pos = next_pipe + 1;
        
        // 解析 expression（如果有）
        if (i < part_count - 1) {  // 不是最后一个
            next_pipe = encoded.find('|', pos);
            if (next_pipe == std::string::npos) {
                // 错误
                error("Invalid f-string encoding");
                return std::make_unique<StringExpr>("", fstr_token.location);
            }
            
            std::string expr_str = encoded.substr(pos, next_pipe - pos);
            expr_strings.push_back(expr_str);
            pos = next_pipe + 1;
        }
    }
    
    // 解析每个表达式字符串
    std::vector<ExprPtr> expressions;
    for (const auto& expr_str : expr_strings) {
        // 创建临时 lexer 解析表达式
        Lexer expr_lexer(expr_str, filename_, nullptr);
        auto expr_tokens = expr_lexer.tokenize();
        
        // 创建临时 parser 解析表达式
        Parser expr_parser(expr_tokens, diag_engine_, filename_);
        try {
            auto expr = expr_parser.expression();
            if (expr) {
                expressions.push_back(std::move(expr));
            } else {
                error("Invalid expression in f-string: " + expr_str);
                return std::make_unique<StringExpr>("", fstr_token.location);
            }
        } catch (...) {
            error("Failed to parse expression in f-string: " + expr_str);
            return std::make_unique<StringExpr>("", fstr_token.location);
        }
    }
    
    // 创建 FStringExpr
    return std::make_unique<FStringExpr>(
        std::move(parts),
        std::move(expressions),
        fstr_token.location
    );
}

} // namespace pawc

