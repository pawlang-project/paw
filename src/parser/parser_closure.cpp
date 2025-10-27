/**
 * @file parser_closure.cpp
 * @brief 闭包解析实现
 * 
 * 语法：(x: i32, y: i32) -> i32 { body }
 *       (x, y) -> { body }  // 类型推导
 */

#include "parser.h"
#include <iostream>

namespace pawc {

bool Parser::isClosurePattern() {
    // 前瞻判断 (x: i32) -> { } 还是 (x + y)
    // 注意：调用时已经看到了 LPAREN，但还没有 consume
    
    size_t saved = current_;
    advance();  // skip (
    
    // () -> { }  无参数闭包
    if (check(TokenType::RPAREN)) {
        advance();
        bool result = check(TokenType::ARROW);
        current_ = saved;
        return result;
    }
    
    // (x: i32, ...) -> { } 或 (x, ...) -> { }
    if (check(TokenType::IDENTIFIER)) {
        advance();
        
        // 检查是否有 : 或 , 或 )
        bool has_colon = check(TokenType::COLON);
        bool has_comma = check(TokenType::COMMA);
        bool has_rparen = check(TokenType::RPAREN);
        
        if (has_colon) {
            // (x: Type ...) 明确是参数定义
            current_ = saved;
            return true;
        }
        
        if (has_comma) {
            // (x, ...) 可能是闭包或元组
            // 继续检查下一个
            advance();  // skip ,
            if (check(TokenType::IDENTIFIER)) {
                advance();
                // 检查下一个标识符后面
                if (check(TokenType::COLON) || check(TokenType::COMMA) || check(TokenType::RPAREN)) {
                    // 如果是 : 或 , 或 )，都可能是闭包
                    // 需要进一步检查是否有 ->
                    while (!check(TokenType::RPAREN) && !isAtEnd()) {
                        advance();
                    }
                    if (match({TokenType::RPAREN})) {
                        bool result = check(TokenType::ARROW);
                        current_ = saved;
                        return result;
                    }
                }
            }
        }
        
        if (has_rparen) {
            // (x) -> { } 单参数无类型
            advance();  // skip )
            bool result = check(TokenType::ARROW);
            current_ = saved;
            return result;
        }
    }
    
    current_ = saved;
    return false;
}

ExprPtr Parser::parseClosure() {
    Token lparen = previous();  // (
    
    // 解析参数列表
    std::vector<ClosureParam> params;
    
    if (!check(TokenType::RPAREN)) {
        do {
            Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
            
            TypePtr param_type = nullptr;
            if (match({TokenType::COLON})) {
                param_type = parseType();
            }
            
            params.push_back({param_name.value, std::move(param_type)});
        } while (match({TokenType::COMMA}));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after closure parameters");
    consume(TokenType::ARROW, "Expected '->' after closure parameters");
    
    // 可选：显式返回类型
    TypePtr return_type = nullptr;
    if (!check(TokenType::LBRACE)) {
        // (x: i32) -> i32 { ... }
        // 有返回类型
        return_type = parseType();
    }
    
    // 解析函数体
    consume(TokenType::LBRACE, "Expected '{' for closure body");
    
    // 使用 blockStatement() 解析块内容
    // 但我们需要构造一个 BlockStmt
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(statement());
    }
    
    Token rbrace = consume(TokenType::RBRACE, "Expected '}' after closure body");
    
    auto body = std::make_unique<BlockStmt>(std::move(statements), rbrace.location);
    
    // 创建 ClosureExpr
    return std::make_unique<ClosureExpr>(
        std::move(params),
        std::move(return_type),
        std::move(body),
        lparen.location
    );
}

} // namespace pawc

