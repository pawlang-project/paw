/**
 * PawLang Parser
 * 
 * 负责将Token流转换为AST，支持：
 * - 表达式解析（运算符优先级）
 * - 语句解析（let/if/loop/return等）
 * - 类型解析（泛型、数组）
 * - 泛型语法（func<T>、Type<T>）
 * - 模式匹配（is表达式）
 * 
 * 文件组织：
 * - 第1部分：核心接口和工具
 * - 第2部分：语句解析
 * - 第3部分：表达式解析
 * - 第4部分：类型和模式解析
 */

#include "parser.h"
#include "pawc/colors.h"
#include <stdexcept>
#include <iostream>

namespace pawc {

// ============================================================================
// 第1部分：核心接口和工具函数
// ============================================================================

Parser::Parser(const std::vector<Token>& tokens, DiagnosticEngine* diag_engine, const std::string& filename)
    : tokens_(tokens), current_(0), diag_engine_(diag_engine), filename_(filename) {}

Program Parser::parse() {
    Program program;
    // 性能优化：预分配语句容器（估计至少 50 个语句）
    program.statements.reserve(50);
    
    while (!isAtEnd()) {
        try {
            if (auto stmt = statement()) {
                program.statements.push_back(std::move(stmt));
            }
        } catch (const std::exception& e) {
            synchronize();
        }
    }
    
    return program;
}

Token Parser::peek() const {
    return tokens_[current_];
}

Token Parser::previous() const {
    return tokens_[current_ - 1];
}

Token Parser::advance() {
    if (!isAtEnd()) current_++;
    return previous();
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(const std::vector<TokenType>& types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    error(message);
    throw std::runtime_error(message);
}

void Parser::error(const std::string& message) {
    SourceLocation loc = peek().location;
    
    // 使用新的诊断引擎
    if (diag_engine_) {
        // 转换为 DiagnosticLocation 格式
        pawc::DiagnosticLocation diag_loc(
            filename_.empty() ? loc.filename : filename_,
            loc.line,
            loc.column,
            peek().value.length()
        );
        diag_engine_->reportError(message, diag_loc);
    } else {
        // 如果没有诊断引擎，直接输出错误（不应该发生）
        std::cerr << Colors::error("Error: ") << message << std::endl;
        std::cerr << Colors::info("  --> ") << loc.filename 
                  << ":" << loc.line 
                  << ":" << loc.column << std::endl;
    }
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        
        switch (peek().type) {
            case TokenType::KW_FN:
            case TokenType::KW_LET:
            case TokenType::KW_IF:
            case TokenType::KW_LOOP:
            case TokenType::KW_RETURN:
                return;
            default:
                advance();
        }
    }
}

StmtPtr Parser::statement() {
    // import语句
    if (match({TokenType::KW_IMPORT})) return importDeclaration();
    
    // extern声明
    if (match({TokenType::KW_EXTERN})) return externDeclaration();
    
    // support声明（不支持pub）
    if (match({TokenType::KW_SUPPORT})) return supportDeclaration();
    
    // pub修饰符
    bool is_public = false;
    if (match({TokenType::KW_PUB})) {
        is_public = true;
    }
    
    if (match({TokenType::KW_TYPE})) return typeAliasDeclaration(is_public);
    if (match({TokenType::KW_FN})) return functionDeclaration(is_public);
    
    // impl已废弃，现在方法直接在struct内定义
    // if (match({TokenType::KW_IMPL})) return implDeclaration();
    
    if (match({TokenType::KW_LET})) return letDeclaration();
    if (match({TokenType::KW_IF})) return ifStatement();
    if (match({TokenType::KW_LOOP})) return loopStatement();
    if (match({TokenType::KW_RETURN})) return returnStatement();
    if (match({TokenType::KW_BREAK})) return breakStatement();
    if (match({TokenType::KW_CONTINUE})) return continueStatement();
    if (match({TokenType::KW_UNSAFE})) return unsafeBlockStatement();
    if (match({TokenType::LBRACE})) return blockStatement();
    
    return expressionStatement();
}

// ============================================================================
// 第2部分：语句解析（Statement Parsing）
// ============================================================================

/**
 * 函数声明解析
 * 支持：泛型参数、self参数、返回类型
 */
StmtPtr Parser::functionDeclaration(bool is_public) {
    Token name = consume(TokenType::IDENTIFIER, "Expected function name");
    
    // 解析泛型参数 <T, U> 或 <T: Display>
    auto generic_params = parseGenericParams();
    
    consume(TokenType::LPAREN, "Expected '(' after function name");
    
    std::vector<Parameter> parameters;
    if (!check(TokenType::RPAREN)) {
        do {
            Parameter param = parseParameter();
            
            // 如果参数是&mut引用，将其视为可变
            if (param.type && param.type->kind == Type::Kind::Reference) {
                const ReferenceTypeNode* ref_type = static_cast<const ReferenceTypeNode*>(param.type.get());
                if (ref_type->is_mutable) {
                    mutable_vars_.insert(param.name);
                }
            }
            
            parameters.push_back(std::move(param));
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters");
    
    TypePtr return_type = nullptr;
    if (match({TokenType::ARROW})) {
        return_type = parseType();
    }
    
    consume(TokenType::LBRACE, "Expected '{' before function body");
    auto body = blockStatement();
    
    return std::make_unique<FunctionStmt>(
        name.value, std::move(generic_params), std::move(parameters), std::move(return_type),
        std::move(body), is_public, name.location
    );
}

StmtPtr Parser::letDeclaration() {
    bool is_mutable = match({TokenType::KW_MUT});
    
    // 检查是否是元组解构: let (x, y) = ...
    if (check(TokenType::LPAREN)) {
        auto pattern = parsePattern();
        
        // 如果是元组模式，记录所有绑定的变量为可变
        if (is_mutable && pattern->kind == Pattern::Kind::Tuple) {
            auto tuple_pat = static_cast<TuplePattern*>(pattern.get());
            for (const auto& elem : tuple_pat->elements) {
                if (elem->kind == Pattern::Kind::Identifier) {
                    auto id_pat = static_cast<IdentifierPattern*>(elem.get());
                    mutable_vars_.insert(id_pat->name);
                }
            }
        }
        
        TypePtr type = nullptr;
        if (match({TokenType::COLON})) {
            type = parseType();
        }
        
        ExprPtr initializer = nullptr;
        if (match({TokenType::ASSIGN})) {
            initializer = expression();
        }
        
        consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
        
        return std::make_unique<LetStmt>(
            std::move(pattern), is_mutable, std::move(type), 
            std::move(initializer), pattern->location
        );
    }
    
    // 普通的单变量绑定: let x = ...
    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
    
    // 记录可变变量
    if (is_mutable) {
        mutable_vars_.insert(name.value);
    }
    
    TypePtr type = nullptr;
    if (match({TokenType::COLON})) {
        type = parseType();
    }
    
    ExprPtr initializer = nullptr;
    if (match({TokenType::ASSIGN})) {
        initializer = expression();
        
        // 【类型推断】：如果没有显式类型，从initializer推断
        if (!type && initializer) {
            type = inferTypeFromExpr(initializer.get());
        }
    }
    
    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    
    return std::make_unique<LetStmt>(
        name.value, is_mutable, std::move(type), 
        std::move(initializer), name.location
    );
}

StmtPtr Parser::ifStatement() {
    auto condition = expression();
    
    consume(TokenType::LBRACE, "Expected '{' after if condition");
    auto then_branch = blockStatement();
    
    StmtPtr else_branch = nullptr;
    if (match({TokenType::KW_ELSE})) {
        if (match({TokenType::KW_IF})) {
            else_branch = ifStatement();
        } else {
            consume(TokenType::LBRACE, "Expected '{' after else");
            else_branch = blockStatement();
        }
    }
    
    return std::make_unique<IfStmt>(
        std::move(condition), std::move(then_branch), 
        std::move(else_branch), previous().location
    );
}

StmtPtr Parser::loopStatement() {
    SourceLocation loc = previous().location;
    
    // 检查是否是迭代器循环或范围循环
    if (check(TokenType::IDENTIFIER)) {
        size_t saved_pos = current_;
        std::string var_name = advance().value;
        
        if (match({TokenType::KW_IN})) {
            // loop var in ...
            ExprPtr start_or_iter = expression();
            
            // 检查是否是范围循环
            if (match({TokenType::DOTDOT})) {
                // loop x in 0..100
                ExprPtr end = expression();
                consume(TokenType::LBRACE, "Expected '{' after range");
                auto body = blockStatement();
                
                return std::make_unique<LoopStmt>(
                    var_name, std::move(start_or_iter), std::move(end), 
                    std::move(body), loc
                );
            } else {
                // loop item in array
                consume(TokenType::LBRACE, "Expected '{' after iterable");
                auto body = blockStatement();
                
                return std::make_unique<LoopStmt>(
                    var_name, std::move(start_or_iter),
                    std::move(body), loc
                );
            }
        } else {
            // 不是迭代器循环，回退
            current_ = saved_pos;
        }
    }
    
    // 条件循环或无限循环
    ExprPtr condition = nullptr;
    if (!check(TokenType::LBRACE)) {
        condition = expression();
    }
    
    consume(TokenType::LBRACE, "Expected '{' after loop condition");
    auto body = blockStatement();
    
    return std::make_unique<LoopStmt>(
        std::move(condition), std::move(body), loc
    );
}

StmtPtr Parser::returnStatement() {
    Token keyword = previous();
    ExprPtr value = nullptr;
    
    if (!check(TokenType::SEMICOLON)) {
        value = expression();
    }
    
    consume(TokenType::SEMICOLON, "Expected ';' after return value");
    return std::make_unique<ReturnStmt>(std::move(value), keyword.location);
}

StmtPtr Parser::breakStatement() {
    Token keyword = previous();
    consume(TokenType::SEMICOLON, "Expected ';' after break");
    return std::make_unique<BreakStmt>(keyword.location);
}

StmtPtr Parser::continueStatement() {
    Token keyword = previous();
    consume(TokenType::SEMICOLON, "Expected ';' after continue");
    return std::make_unique<ContinueStmt>(keyword.location);
}

StmtPtr Parser::expressionStatement() {
    auto expr = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression");
    return std::make_unique<ExprStmt>(std::move(expr), previous().location);
}

StmtPtr Parser::blockStatement() {
    std::vector<StmtPtr> statements;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(statement());
    }
    
    consume(TokenType::RBRACE, "Expected '}' after block");
    return std::make_unique<BlockStmt>(std::move(statements), previous().location);
}

StmtPtr Parser::unsafeBlockStatement() {
    Token unsafe_token = previous();
    
    consume(TokenType::LBRACE, "Expected '{' after 'unsafe'");
    
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(statement());
    }
    
    consume(TokenType::RBRACE, "Expected '}' after unsafe block");
    return std::make_unique<UnsafeBlockStmt>(std::move(statements), unsafe_token.location);
}

ExprPtr Parser::expression() {
    return matchExpression();  // is 表达式优先级最低
}

ExprPtr Parser::assignment() {
    auto expr = logicalOr();
    
    // 检查赋值运算符
    if (match({TokenType::ASSIGN, TokenType::PLUS_EQ, TokenType::MINUS_EQ})) {
        Token op = previous();
        auto value = assignment();  // 右结合
        
        // 支持四种赋值目标：Identifier、MemberAccess、Index、Unary(Deref)
        if (expr->kind != Expr::Kind::Identifier && 
            expr->kind != Expr::Kind::MemberAccess && 
            expr->kind != Expr::Kind::Index &&
            expr->kind != Expr::Kind::Unary) {
            error("Invalid assignment target");
            return expr;
        }
        
        // 对于Unary，只允许解引用操作
        if (expr->kind == Expr::Kind::Unary) {
            const UnaryExpr* unary = static_cast<const UnaryExpr*>(expr.get());
            if (unary->op != UnaryExpr::Op::Deref) {
                error("Invalid assignment target: only dereference (*x) can be assigned");
                return expr;
            }
            // *x = value 是合法的
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op.location);
        }
        
        // 对于成员访问，检查对象是否可变
        if (expr->kind == Expr::Kind::MemberAccess && op.type == TokenType::ASSIGN) {
            const MemberAccessExpr* member_expr = static_cast<const MemberAccessExpr*>(expr.get());
            
            // 检查对象是否可变
            if (member_expr->object->kind == Expr::Kind::Identifier) {
                std::string obj_name = static_cast<const IdentifierExpr*>(member_expr->object.get())->name;
                
                // self总是允许（由函数参数的mut决定）
                // 普通变量需要检查mut
                if (obj_name != "self" && mutable_vars_.find(obj_name) == mutable_vars_.end()) {
                    error("Cannot assign to field of immutable variable '" + obj_name + "'. Use 'let mut' to make it mutable.");
                    return expr;
                }
            }
            
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op.location);
        }
        
        // 对于索引访问（数组/字符串索引赋值）
        if (expr->kind == Expr::Kind::Index && op.type == TokenType::ASSIGN) {
            const IndexExpr* index_expr = static_cast<const IndexExpr*>(expr.get());
            
            // 检查数组/字符串是否可变
            if (index_expr->array->kind == Expr::Kind::Identifier) {
                std::string var_name = static_cast<const IdentifierExpr*>(index_expr->array.get())->name;
                
                if (mutable_vars_.find(var_name) == mutable_vars_.end()) {
                    error("Cannot assign to index of immutable variable '" + var_name + "'. Use 'let mut' to make it mutable.");
                    return expr;
                }
            }
            
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op.location);
        }
        
        // 对于普通标识符赋值
        if (expr->kind != Expr::Kind::Identifier) {
            error("Invalid assignment target for compound assignment");
            return expr;
        }
        
        auto target_name = static_cast<IdentifierExpr*>(expr.get())->name;
        
        // 对于 += 和 -=，转换为 a = a + b 或 a = a - b
        if (op.type == TokenType::PLUS_EQ) {
            value = std::make_unique<BinaryExpr>(
                BinaryExpr::Op::Add,
                std::make_unique<IdentifierExpr>(target_name, op.location),
                std::move(value),
                op.location
            );
        } else if (op.type == TokenType::MINUS_EQ) {
            value = std::make_unique<BinaryExpr>(
                BinaryExpr::Op::Sub,
                std::make_unique<IdentifierExpr>(target_name, op.location),
                std::move(value),
                op.location
            );
        }
        
        return std::make_unique<AssignExpr>(target_name, std::move(value), op.location);
    }
    
    return expr;
}

ExprPtr Parser::logicalOr() {
    auto expr = logicalAnd();
    
    while (match({TokenType::OR})) {
        Token op = previous();
        auto right = logicalAnd();
        expr = std::make_unique<BinaryExpr>(
            BinaryExpr::Op::Or, std::move(expr), std::move(right), op.location
        );
    }
    
    return expr;
}

ExprPtr Parser::logicalAnd() {
    auto expr = equality();
    
    while (match({TokenType::AND})) {
        Token op = previous();
        auto right = equality();
        expr = std::make_unique<BinaryExpr>(
            BinaryExpr::Op::And, std::move(expr), std::move(right), op.location
        );
    }
    
    return expr;
}

ExprPtr Parser::equality() {
    auto expr = comparison();
    
    while (match({TokenType::EQ, TokenType::NE})) {
        Token op = previous();
        auto right = comparison();
        BinaryExpr::Op operation = (op.type == TokenType::EQ) ? 
            BinaryExpr::Op::Eq : BinaryExpr::Op::Ne;
        expr = std::make_unique<BinaryExpr>(
            operation, std::move(expr), std::move(right), op.location
        );
    }
    
    return expr;
}

ExprPtr Parser::comparison() {
    auto expr = term();
    
    while (match({TokenType::LT, TokenType::LE, TokenType::GT, TokenType::GE})) {
        Token op = previous();
        auto right = term();
        BinaryExpr::Op operation;
        switch (op.type) {
            case TokenType::LT: operation = BinaryExpr::Op::Lt; break;
            case TokenType::LE: operation = BinaryExpr::Op::Le; break;
            case TokenType::GT: operation = BinaryExpr::Op::Gt; break;
            case TokenType::GE: operation = BinaryExpr::Op::Ge; break;
            default: operation = BinaryExpr::Op::Lt;
        }
        expr = std::make_unique<BinaryExpr>(
            operation, std::move(expr), std::move(right), op.location
        );
    }
    
    return expr;
}

ExprPtr Parser::term() {
    auto expr = factor();
    
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        auto right = factor();
        BinaryExpr::Op operation = (op.type == TokenType::PLUS) ?
            BinaryExpr::Op::Add : BinaryExpr::Op::Sub;
        expr = std::make_unique<BinaryExpr>(
            operation, std::move(expr), std::move(right), op.location
        );
    }
    
    return expr;
}

ExprPtr Parser::factor() {
    auto expr = unary();
    
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        auto right = unary();
        BinaryExpr::Op operation;
        switch (op.type) {
            case TokenType::STAR: operation = BinaryExpr::Op::Mul; break;
            case TokenType::SLASH: operation = BinaryExpr::Op::Div; break;
            case TokenType::PERCENT: operation = BinaryExpr::Op::Mod; break;
            default: operation = BinaryExpr::Op::Mul;
        }
        expr = std::make_unique<BinaryExpr>(
            operation, std::move(expr), std::move(right), op.location
        );
    }
    
    return expr;
}

ExprPtr Parser::unary() {
    // 处理一元操作符：-, !, &, &mut, *
    if (match({TokenType::MINUS, TokenType::NOT, TokenType::AMPERSAND, TokenType::STAR})) {
        Token op = previous();
        
        // 特殊处理 &mut
        if (op.type == TokenType::AMPERSAND && check(TokenType::KW_MUT)) {
            advance();  // 消费 mut
            auto operand = unary();
            
            // 【类型检查】：&mut x 要求 x 必须是可变的
            if (operand->kind == Expr::Kind::Identifier) {
                std::string var_name = static_cast<IdentifierExpr*>(operand.get())->name;
                if (mutable_vars_.find(var_name) == mutable_vars_.end()) {
                    error("Cannot take mutable reference of immutable variable '" + var_name + "'. Use 'let mut' to make it mutable.");
                }
            }
            
            return std::make_unique<UnaryExpr>(
                UnaryExpr::Op::RefMut, std::move(operand), op.location
            );
        }
        
        auto operand = unary();
        
        UnaryExpr::Op operation;
        if (op.type == TokenType::MINUS) {
            operation = UnaryExpr::Op::Neg;
        } else if (op.type == TokenType::NOT) {
            operation = UnaryExpr::Op::Not;
        } else if (op.type == TokenType::AMPERSAND) {
            operation = UnaryExpr::Op::Ref;
        } else { // STAR
            operation = UnaryExpr::Op::Deref;
        }
        
        return std::make_unique<UnaryExpr>(
            operation, std::move(operand), op.location
        );
    }
    
    return postfix();
}

// postfix: 成员访问、方法调用、关联函数、数组索引、as类型转换
ExprPtr Parser::postfix() {
    auto expr = primary();
    
    while (true) {
        // ? 错误传播操作符（最高优先级）
        if (match({TokenType::QUESTION})) {
            expr = std::make_unique<TryExpr>(std::move(expr), previous().location);
            continue;
        }
        
        // as类型转换
        if (match({TokenType::KW_AS})) {
            TypePtr target_type = parseType();
            expr = std::make_unique<CastExpr>(std::move(expr), std::move(target_type), previous().location);
            continue;
        }
        
        if (match({TokenType::DOT})) {
            // 检查是元组字段访问 (.0, .1) 还是普通成员访问 (.field)
            if (check(TokenType::INTEGER)) {
                Token index_token = advance();
                int index = std::stoi(index_token.value);
                expr = std::make_unique<MemberAccessExpr>(
                    std::move(expr), index, index_token.location
                );
            } else {
                Token member = consume(TokenType::IDENTIFIER, "Expected member name or tuple index after '.'");
                expr = std::make_unique<MemberAccessExpr>(
                    std::move(expr), member.value, member.location
                );
            }
        } else if (match({TokenType::LBRACKET})) {
            // 数组索引访问或范围切片: arr[index] 或 arr[start..end]
            Token lbracket = previous();
            
            // 检查是否是 ..end (从开始到end)
            if (check(TokenType::DOTDOT)) {
                advance();  // 消费 ..
                ExprPtr end = expression();
                Token bracket = consume(TokenType::RBRACKET, "Expected ']' after range");
                
                // 创建范围表达式 ..end
                auto range = std::make_unique<RangeExpr>(nullptr, std::move(end), lbracket.location);
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(range), bracket.location);
            } else {
                // 解析第一个表达式
                auto first_expr = expression();
                
                // 检查是否有 ..
                if (match({TokenType::DOTDOT})) {
                    // 这是范围: start..end 或 start..
                    ExprPtr end = nullptr;
                    if (!check(TokenType::RBRACKET)) {
                        end = expression();
                    }
                    Token bracket = consume(TokenType::RBRACKET, "Expected ']' after range");
                    
                    // 创建范围表达式
                    auto range = std::make_unique<RangeExpr>(std::move(first_expr), std::move(end), lbracket.location);
                    expr = std::make_unique<IndexExpr>(std::move(expr), std::move(range), bracket.location);
                } else {
                    // 普通索引: arr[index]
                    Token bracket = consume(TokenType::RBRACKET, "Expected ']' after index");
                    expr = std::make_unique<IndexExpr>(std::move(expr), std::move(first_expr), bracket.location);
                }
            }
        } else if (match({TokenType::DOUBLE_COLON})) {
            // 支持三种情况：
            // 1. module::function() - 模块函数调用
            // 2. Type::Variant(args) - 枚举变体构造
            // 3. Type::Variant - 枚举变体标识符
            Token name_after_colon = consume(TokenType::IDENTIFIER, "Expected name after '::'");
            
            // 检查是否有泛型类型参数: name<T>
            std::vector<TypePtr> type_args;
            bool has_generic = false;
            if (check(TokenType::LT)) {
                size_t saved = current_;
                advance();  // skip <
                
                // 预判：必须是类型
                if (check(TokenType::IDENTIFIER) || check(TokenType::LBRACKET)) {
                    try {
                        do {
                            type_args.push_back(parseType());
                        } while (match({TokenType::COMMA}));
                        
                        if (match({TokenType::GT}) && match({TokenType::LPAREN})) {
                            has_generic = true;
                        }
                    } catch (...) {
                        // 不是泛型，恢复
                        current_ = saved;
                    }
                }
                
                if (!has_generic) {
                    current_ = saved;
                }
            }
            
            // 检查是否是函数调用
            if (!has_generic && match({TokenType::LPAREN})) {
                std::vector<ExprPtr> arguments;
                if (!check(TokenType::RPAREN)) {
                    do {
                        arguments.push_back(expression());
                    } while (match({TokenType::COMMA}));
                }
                consume(TokenType::RPAREN, "Expected ')' after arguments");
                
                if (expr->kind == Expr::Kind::Identifier) {
                    std::string prefix = static_cast<IdentifierExpr*>(expr.get())->name;
                    
                    // 使用符号表查询判断类型（不依赖大小写）：
                    // 1. prefix是Enum → Enum变体构造
                    // 2. prefix是Struct → Struct关联函数
                    // 3. 其他 → 模块调用
                    if (isDefinedEnum(prefix)) {
                        // Type::Variant(args) - 枚举变体
                        return std::make_unique<EnumVariantExpr>(
                            prefix, name_after_colon.value, std::move(arguments), name_after_colon.location
                        );
                    } else if (isDefinedStruct(prefix)) {
                        // Type::method() - Struct关联函数
                        // 创建普通调用表达式
                        auto func_name = std::make_unique<IdentifierExpr>(
                            name_after_colon.value, name_after_colon.location
                        );
                        return std::make_unique<CallExpr>(
                            std::move(func_name), std::move(arguments), name_after_colon.location
                        );
                    } else {
                        // module::function() - 模块调用
                        auto func_name = std::make_unique<IdentifierExpr>(
                            name_after_colon.value, name_after_colon.location
                        );
                        return std::make_unique<CallExpr>(
                            prefix, std::move(func_name), std::move(arguments), name_after_colon.location
                        );
                    }
                }
            } else if (has_generic) {
                // module::func<T>(args) - 跨模块泛型调用
                std::vector<ExprPtr> arguments;
                if (!check(TokenType::RPAREN)) {
                    do {
                        arguments.push_back(expression());
                    } while (match({TokenType::COMMA}));
                }
                consume(TokenType::RPAREN, "Expected ')' after arguments");
                
                if (expr->kind == Expr::Kind::Identifier) {
                    std::string prefix = static_cast<IdentifierExpr*>(expr.get())->name;
                    auto func_name = std::make_unique<IdentifierExpr>(
                        name_after_colon.value, name_after_colon.location
                    );
                    
                    // 使用新的跨模块泛型调用构造函数
                    return std::make_unique<CallExpr>(
                        prefix, std::move(func_name), std::move(type_args), 
                        std::move(arguments), name_after_colon.location
                    );
                }
            }
            
            // 否则只是标识符
            expr = std::make_unique<IdentifierExpr>(name_after_colon.value, name_after_colon.location);
        } else if (check(TokenType::LT) && expr->kind == Expr::Kind::Identifier) {
            // 可能是泛型调用或泛型enum: func<T>(args) 或 Type<T>::Variant
            // 但需要预判避免误认 x < 10 为泛型
            size_t saved_pos = current_;
            advance();  // 跳过 <
            
            // 预判：< 后面必须是 IDENTIFIER 或 LBRACKET 才可能是泛型
            if (!check(TokenType::IDENTIFIER) && !check(TokenType::LBRACKET)) {
                // 不是泛型，恢复位置
                current_ = saved_pos;
                return expr;
            }
            
            // 尝试解析类型参数
            std::vector<TypePtr> type_args;
            bool is_generic = false;
            bool is_enum_variant = false;
            
            try {
                do {
                    type_args.push_back(parseType());
                } while (match({TokenType::COMMA}));
                
                if (match({TokenType::GT})) {
                    // 检查后面是什么
                    if (match({TokenType::LPAREN})) {
                        // 泛型函数调用
                        is_generic = true;
                    } else if (check(TokenType::DOUBLE_COLON)) {
                        // 泛型enum variant: Type<T>::Variant
                        is_enum_variant = true;
                    }
                }
            } catch (...) {
                // 失败：不是泛型
            }
            
            if (is_generic) {
                // 泛型函数调用
                std::vector<ExprPtr> arguments;
                if (!check(TokenType::RPAREN)) {
                    do {
                        arguments.push_back(expression());
                    } while (match({TokenType::COMMA}));
                }
                consume(TokenType::RPAREN, "Expected ')' after arguments");
                
                expr = std::make_unique<CallExpr>(
                    std::move(expr), std::move(type_args), std::move(arguments), expr->location
                );
            } else if (is_enum_variant) {
                // 泛型enum variant: Type<T>::Variant
                std::string base_name = static_cast<IdentifierExpr*>(expr.get())->name;
                
                // 生成mangled name (Option<i32> → Option_i32)
                std::string mangled_name = base_name;
                for (const auto& arg : type_args) {
                    mangled_name += "_";
                    if (arg->kind == Type::Kind::Primitive) {
                        const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(arg.get());
                        switch (prim->prim_type) {
                            case PrimitiveType::I32: mangled_name += "i32"; break;
                            case PrimitiveType::I64: mangled_name += "i64"; break;
                            case PrimitiveType::STRING: mangled_name += "string"; break;
                            default: mangled_name += "T"; break;
                        }
                    } else if (arg->kind == Type::Kind::Named) {
                        const NamedTypeNode* named = static_cast<const NamedTypeNode*>(arg.get());
                        mangled_name += named->name;
                    }
                }
                
                // 更新expr为mangled name
                expr = std::make_unique<IdentifierExpr>(mangled_name, expr->location);
                // 继续处理::，下一轮循环会处理
            } else {
                // 回退，不是泛型
                current_ = saved_pos;
                break;
            }
        } else if (match({TokenType::LPAREN})) {
            // 普通函数调用（包括方法调用）
            std::vector<ExprPtr> arguments;
            if (!check(TokenType::RPAREN)) {
                do {
                    arguments.push_back(expression());
                } while (match({TokenType::COMMA}));
            }
            Token paren = consume(TokenType::RPAREN, "Expected ')' after arguments");
            expr = std::make_unique<CallExpr>(
                std::move(expr), std::move(arguments), paren.location
            );
        } else {
            break;
        }
    }
    
    return expr;
}

ExprPtr Parser::call() {
    auto expr = primary();
    
    while (true) {
        if (match({TokenType::LPAREN})) {
            std::vector<ExprPtr> arguments;
            if (!check(TokenType::RPAREN)) {
                do {
                    arguments.push_back(expression());
                } while (match({TokenType::COMMA}));
            }
            Token paren = consume(TokenType::RPAREN, "Expected ')' after arguments");
            expr = std::make_unique<CallExpr>(
                std::move(expr), std::move(arguments), paren.location
            );
        } else {
            break;
        }
    }
    
    return expr;
}

ExprPtr Parser::primary() {
    if (match({TokenType::KW_TRUE})) {
        return std::make_unique<BooleanExpr>(true, previous().location);
    }
    if (match({TokenType::KW_FALSE})) {
        return std::make_unique<BooleanExpr>(false, previous().location);
    }
    
    if (match({TokenType::INTEGER})) {
        return std::make_unique<IntegerExpr>(
            std::stoll(previous().value), previous().location
        );
    }
    if (match({TokenType::FLOAT})) {
        return std::make_unique<FloatExpr>(
            std::stod(previous().value), previous().location
        );
    }
    if (match({TokenType::STRING})) {
        return std::make_unique<StringExpr>(previous().value, previous().location);
    }
    
    // F-String字符串插值
    if (match({TokenType::F_STRING})) {
        return parseFString();
    }
    
    // 字符字面量（转换为整数）
    if (match({TokenType::CHAR})) {
        Token ch = previous();
        int char_value = static_cast<int>(ch.value[0]);
        return std::make_unique<IntegerExpr>(char_value, ch.location);
    }
    
    // self 关键字作为表达式
    if (match({TokenType::KW_SELF})) {
        return std::make_unique<IdentifierExpr>("self", previous().location);
    }
    
    // if表达式: if condition { then_expr } else { else_expr }
    if (match({TokenType::KW_IF})) {
        ExprPtr condition = expression();
        consume(TokenType::LBRACE, "Expected '{' after if condition");
        ExprPtr then_expr = expression();
        consume(TokenType::RBRACE, "Expected '}' after then expression");
        consume(TokenType::KW_ELSE, "If expression must have else branch");
        consume(TokenType::LBRACE, "Expected '{' after else");
        ExprPtr else_expr = expression();
        consume(TokenType::RBRACE, "Expected '}' after else expression");
        return std::make_unique<IfExpr>(
            std::move(condition), std::move(then_expr), std::move(else_expr), previous().location
        );
    }
    
    // ok(value) - 创建成功值
    if (match({TokenType::KW_OK})) {
        consume(TokenType::LPAREN, "Expected '(' after 'ok'");
        ExprPtr value = expression();
        consume(TokenType::RPAREN, "Expected ')' after value");
        return std::make_unique<OkExpr>(std::move(value), previous().location);
    }
    
    // err(message) - 创建错误
    if (match({TokenType::KW_ERR})) {
        consume(TokenType::LPAREN, "Expected '(' after 'err'");
        ExprPtr message = expression();
        consume(TokenType::RPAREN, "Expected ')' after message");
        return std::make_unique<ErrExpr>(std::move(message), previous().location);
    }
    
    // Self 字面量: Self { x: 1, y: 2 }
    if (match({TokenType::KW_SELF_TYPE})) {
        if (!current_parsing_struct_.empty() && match({TokenType::LBRACE})) {
            // Self { ... } → 替换为 StructName { ... }
            std::vector<FieldInit> fields;
            if (!check(TokenType::RBRACE)) {
                do {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
                    consume(TokenType::COLON, "Expected ':' after field name");
                    ExprPtr value = expression();
                    fields.push_back({field_name.value, std::move(value)});
                } while (match({TokenType::COMMA}));
            }
            consume(TokenType::RBRACE, "Expected '}' after struct fields");
            return std::make_unique<StructLiteralExpr>(
                current_parsing_struct_, std::move(fields), previous().location
            );
        }
        error("Self can only be used inside struct definitions");
        return nullptr;
    }
    
    // 数组字面量: [1, 2, 3]
    if (match({TokenType::LBRACKET})) {
        std::vector<ExprPtr> elements;
        
        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(expression());
            } while (match({TokenType::COMMA}));
        }
        
        Token bracket = consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return std::make_unique<ArrayLiteralExpr>(std::move(elements), bracket.location);
    }
    
    if (match({TokenType::IDENTIFIER})) {
        Token name_token = previous();
        
        // 检查是否是泛型struct literal: Type<T> { field: value, ... }
        if (check(TokenType::LT)) {
            size_t saved_pos = current_;
            advance();  // consume <
            
            // 预判：< 后面必须是 IDENTIFIER 或 LBRACKET 才可能是泛型
            bool looks_like_generic = check(TokenType::IDENTIFIER) || check(TokenType::LBRACKET);
            current_ = saved_pos; // 恢复
            
            if (!looks_like_generic) {
                // 不是泛型，只是标识符
                return std::make_unique<IdentifierExpr>(name_token.value, name_token.location);
            }
            
            // 尝试解析类型参数
            advance(); // 跳过 <
            std::vector<TypePtr> type_args;
            bool is_generic_literal = false;
            
            try {
                do {
                    type_args.push_back(parseType());
                } while (match({TokenType::COMMA}));
                
                if (match({TokenType::GT}) && check(TokenType::LBRACE)) {
                    is_generic_literal = true;
                }
            } catch (...) {
                // 不是泛型literal
            }
            
            if (is_generic_literal && match({TokenType::LBRACE})) {
                // 解析字段
                std::vector<FieldInit> field_inits;
                
                while (!check(TokenType::RBRACE) && !isAtEnd()) {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
                    consume(TokenType::COLON, "Expected ':' after field name");
                    auto field_value = expression();
                    
                    FieldInit field_init;
                    field_init.name = field_name.value;
                    field_init.value = std::move(field_value);
                    field_inits.push_back(std::move(field_init));
                    
                    if (!match({TokenType::COMMA})) break;
                }
                
                consume(TokenType::RBRACE, "Expected '}' after struct literal");
                
                // 【修改】保留原始名称和type_arguments，让CodeGen处理
                // 不在Parser中生成mangled_name
                return std::make_unique<StructLiteralExpr>(
                    name_token.value, std::move(field_inits), 
                    std::move(type_args), name_token.location
                );
            } else {
                // 回退
                current_ = saved_pos;
            }
        }
        
        // 解析可选的泛型参数: Box<i32>
        std::vector<TypePtr> type_arguments;
        if (match({TokenType::LT})) {
            do {
                type_arguments.push_back(parseType());
            } while (match({TokenType::COMMA}));
            consume(TokenType::GT, "Expected '>' after generic arguments");
        }
        
        // 检查是否是普通struct literal: Type { field: value, ... }
        // 或泛型struct: Type<T1, T2> { field: value, ... }
        // 使用符号表：只有已注册的类型才能构造
        if (isRegisteredType(name_token.value)) {
            
            if (match({TokenType::LBRACE})) {
                std::vector<FieldInit> field_inits;
                
                while (!check(TokenType::RBRACE) && !isAtEnd()) {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
                    consume(TokenType::COLON, "Expected ':' after field name");
                    auto field_value = expression();
                    
                    FieldInit field_init;
                    field_init.name = field_name.value;
                    field_init.value = std::move(field_value);
                    field_inits.push_back(std::move(field_init));
                    
                    if (!match({TokenType::COMMA})) break;
                }
                
                consume(TokenType::RBRACE, "Expected '}' after struct literal");
                
                // 如果有显式泛型参数，使用新构造函数
                if (!type_arguments.empty()) {
                    return std::make_unique<StructLiteralExpr>(
                        name_token.value, std::move(field_inits), 
                        std::move(type_arguments), name_token.location
                    );
                } else {
                    return std::make_unique<StructLiteralExpr>(
                        name_token.value, std::move(field_inits), name_token.location
                    );
                }
            }
        }
        
        return std::make_unique<IdentifierExpr>(name_token.value, name_token.location);
    }
    
    // 闭包或元组/括号表达式
    if (check(TokenType::LPAREN)) {
        // 前瞻判断
        if (isClosurePattern()) {
            advance();  // consume (
            return parseClosure();
        }
        
        // 否则是元组或括号表达式
        advance();  // consume (
        Token lparen = previous();
        
        // 空元组 ()
        if (check(TokenType::RPAREN)) {
            advance();
            return std::make_unique<TupleLiteralExpr>(std::vector<ExprPtr>(), lparen.location);
        }
        
        // 解析第一个表达式
        auto first_expr = expression();
        
        // 检查是否有逗号（确认是元组）
        if (match({TokenType::COMMA})) {
            // 这是元组字面量
            std::vector<ExprPtr> elements;
            elements.push_back(std::move(first_expr));
            
            // 解析剩余元素
            if (!check(TokenType::RPAREN)) {
                do {
                    elements.push_back(expression());
                } while (match({TokenType::COMMA}));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after tuple elements");
            return std::make_unique<TupleLiteralExpr>(std::move(elements), lparen.location);
        } else {
            // 只有一个元素且没有逗号，这是括号分组表达式
            consume(TokenType::RPAREN, "Expected ')' after expression");
            return first_expr;
        }
    }
    
    error("Expected expression");
    throw std::runtime_error("Expected expression");
}

// ============================================================================
// 第4部分：类型和模式解析（Type & Pattern Parsing）
// ============================================================================

/**
 * 类型解析
 * 支持：基础类型、数组、切片、泛型参数
 * @param allow_slice 是否允许[T]解析为切片（函数参数中为true）
 */
TypePtr Parser::parseType(bool allow_slice) {
    // 函数类型: fn(T1, T2) -> R
    if (match({TokenType::KW_FN})) {
        Token fn_token = previous();
        
        consume(TokenType::LPAREN, "Expected '(' after 'fn'");
        
        // 解析参数类型列表
        std::vector<TypePtr> param_types;
        if (!check(TokenType::RPAREN)) {
            do {
                param_types.push_back(parseType(allow_slice));
            } while (match({TokenType::COMMA}));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after function parameters");
        
        // 解析返回类型（可选）
        TypePtr return_type;
        if (match({TokenType::ARROW})) {
            return_type = parseType(allow_slice);
        } else {
            // 默认返回 void
            return_type = std::make_unique<PrimitiveTypeNode>(PrimitiveType::VOID, fn_token.location);
        }
        
        return std::make_unique<FunctionTypeNode>(std::move(param_types), std::move(return_type), fn_token.location);
    }
    
    // 引用类型: &T 或 &mut T
    if (match({TokenType::AMPERSAND})) {
        Token amp_token = previous();
        bool is_mutable = match({TokenType::KW_MUT});
        TypePtr inner_type = parseType(allow_slice);
        return std::make_unique<ReferenceTypeNode>(std::move(inner_type), is_mutable, amp_token.location);
    }
    
    // Self类型（在struct方法中使用）
    if (match({TokenType::KW_SELF_TYPE})) {
        return std::make_unique<SelfTypeNode>(previous().location);
    }
    
    // 元组类型: (T, U, V)
    if (check(TokenType::LPAREN)) {
        Token lparen = advance();
        std::vector<TypePtr> element_types;
        
        // 解析第一个类型
        if (!check(TokenType::RPAREN)) {
            element_types.push_back(parseType(allow_slice));
            
            // 检查是否有逗号（确认是元组，不是括号分组）
            if (match({TokenType::COMMA})) {
                // 这是元组类型
                while (!check(TokenType::RPAREN) && !isAtEnd()) {
                    element_types.push_back(parseType(allow_slice));
                    if (!match({TokenType::COMMA})) {
                        break;
                    }
                }
                
                consume(TokenType::RPAREN, "Expected ')' after tuple type");
                return std::make_unique<TupleTypeNode>(std::move(element_types), lparen.location);
            } else {
                // 只有一个元素且没有逗号，这是括号分组，返回内部类型
                consume(TokenType::RPAREN, "Expected ')' after type");
                return std::move(element_types[0]);
            }
        }
        
        // 空元组 ()
        consume(TokenType::RPAREN, "Expected ')' after tuple type");
        return std::make_unique<TupleTypeNode>(std::move(element_types), lparen.location);
    }
    
    // 数组/切片类型: [element_type; size] 或 [element_type]
    if (match({TokenType::LBRACKET})) {
        auto elem_type = parseType(allow_slice);
        
        SourceLocation loc = elem_type->location;
        
        // 检查是否有显式大小: [T; N]
        if (match({TokenType::SEMICOLON})) {
            Token size_token = consume(TokenType::INTEGER, "Expected array size");
            int size = std::stoi(size_token.value);
            loc = size_token.location;
            consume(TokenType::RBRACKET, "Expected ']' after array type");
            
            // [T; N] 始终是固定大小数组
            return std::make_unique<ArrayTypeNode>(std::move(elem_type), size, loc);
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array type");
        
        // [T] 的语义取决于上下文
        if (allow_slice) {
            // 函数参数中：[T] 是切片
            return std::make_unique<SliceTypeNode>(std::move(elem_type), loc);
        } else {
            // 变量声明中：[T] 是待推导大小的数组
            return std::make_unique<ArrayTypeNode>(std::move(elem_type), -1, loc);
        }
    }
    
    if (match({TokenType::IDENTIFIER})) {
        std::string type_name = previous().value;
        
        // 基本类型映射
        static const std::map<std::string, PrimitiveType> primitive_types = {
            {"i8", PrimitiveType::I8}, {"i16", PrimitiveType::I16},
            {"i32", PrimitiveType::I32}, {"i64", PrimitiveType::I64},
            {"i128", PrimitiveType::I128},
            {"u8", PrimitiveType::U8}, {"u16", PrimitiveType::U16},
            {"u32", PrimitiveType::U32}, {"u64", PrimitiveType::U64},
            {"u128", PrimitiveType::U128},
            {"f32", PrimitiveType::F32}, {"f64", PrimitiveType::F64},
            {"bool", PrimitiveType::BOOL}, {"char", PrimitiveType::CHAR},
            {"string", PrimitiveType::STRING}, {"void", PrimitiveType::VOID}
        };
        
        auto it = primitive_types.find(type_name);
        if (it != primitive_types.end()) {
            TypePtr prim_type = std::make_unique<PrimitiveTypeNode>(it->second, previous().location);
            
            // 检查是否是Optional类型: i32?
            if (match({TokenType::QUESTION})) {
                return std::make_unique<OptionalTypeNode>(std::move(prim_type), previous().location);
            }
            
            return prim_type;
        }
        
        // 泛型参数: <T, U>
        // 只有当 < 后面是 IDENTIFIER 或 LBRACKET 时才认为是泛型参数
        std::vector<TypePtr> generic_args;
        if (check(TokenType::LT)) {
            // 预判：peek 看下一个token是否可能是类型的开始
            size_t saved_pos = current_;
            advance(); // 跳过 <
            bool is_generic = check(TokenType::IDENTIFIER) || check(TokenType::LBRACKET);
            current_ = saved_pos; // 恢复位置
            
            if (is_generic && match({TokenType::LT})) {
                do {
                    generic_args.push_back(parseType());
                } while (match({TokenType::COMMA}));
                consume(TokenType::GT, "Expected '>' after generic arguments");
            }
        }
        
        // 自定义类型或泛型类型
        // 只有单字母大写才是泛型参数（T, U, K, V等）
        if (std::isupper(type_name[0]) && type_name.length() == 1 && generic_args.empty()) {
            // 单字母大写是泛型: T, U
            TypePtr generic_type = std::make_unique<GenericTypeNode>(type_name, previous().location);
            
            // 检查是否是Optional类型: T?
            if (match({TokenType::QUESTION})) {
                return std::make_unique<OptionalTypeNode>(std::move(generic_type), previous().location);
            }
            
            return generic_type;
        }
        
        // 多字母大写或其他情况都是命名类型（Status, Point, Option等）
        TypePtr base_type = std::make_unique<NamedTypeNode>(type_name, std::move(generic_args), previous().location);
        
        // 检查是否是Optional类型: T?
        if (match({TokenType::QUESTION})) {
            return std::make_unique<OptionalTypeNode>(std::move(base_type), previous().location);
        }
        
        return base_type;
    }
    
    error("Expected type");
    throw std::runtime_error("Expected type");
}

Parameter Parser::parseParameter() {
    Parameter param;
    param.location = peek().location;
    
    // 检查是否是 self 或 mut self
    if (match({TokenType::KW_MUT})) {
        if (match({TokenType::KW_SELF})) {
            param.name = "self";
            param.is_self = true;
            param.is_mut_self = true;
            param.type = nullptr;
            return param;
        }
        error("Expected 'self' after 'mut'");
    }
    
    if (match({TokenType::KW_SELF})) {
        param.name = "self";
        param.is_self = true;
        param.is_mut_self = false;
        param.type = nullptr;
        return param;
    }
    
    // 普通参数
    Token name = consume(TokenType::IDENTIFIER, "Expected parameter name");
    param.name = name.value;
    param.is_self = false;
    param.is_mut_self = false;
    
    consume(TokenType::COLON, "Expected ':' after parameter name");
    param.type = parseType(true);  // 函数参数允许切片类型
    
    return param;
}

// 解析泛型参数 <T, U, V>
std::vector<GenericParam> Parser::parseGenericParams() {
    std::vector<GenericParam> params;
    
    if (!match({TokenType::LT})) {
        return params;
    }
    
    do {
        Token name = consume(TokenType::IDENTIFIER, "Expected generic parameter name");
        GenericParam param;
        param.name = name.value;
        param.location = name.location;
        
        // 解析接口约束: T: Display + Clone
        if (match({TokenType::COLON})) {
            do {
                Token interface_name = consume(TokenType::IDENTIFIER, "Expected interface name");
                param.interface_constraints.push_back(interface_name.value);
            } while (match({TokenType::PLUS}));
        }
        
        params.push_back(param);
    } while (match({TokenType::COMMA}));
    
    consume(TokenType::GT, "Expected '>' after generic parameters");
    return params;
}

// 解析 type Name = struct { } 或 enum { }
StmtPtr Parser::typeAliasDeclaration(bool is_public) {
    Token name = consume(TokenType::IDENTIFIER, "Expected type name");
    auto generic_params = parseGenericParams();
    
    // 注册类型名
    registerType(name.value);
    
    consume(TokenType::ASSIGN, "Expected '=' after type name");
    
    if (match({TokenType::KW_STRUCT})) {
        return structDeclaration(name, std::move(generic_params), is_public);
    } else if (match({TokenType::KW_ENUM})) {
        return enumDeclaration(name, std::move(generic_params), is_public);
    } else if (match({TokenType::KW_INTERFACE})) {
        return interfaceDeclaration(name, std::move(generic_params), is_public);
    }
    
    error("Expected 'struct', 'enum', or 'interface' after '='");
    return nullptr;
}

// 解析 import "module_path"
StmtPtr Parser::importDeclaration() {
    Token path_token = consume(TokenType::STRING, "Expected module path string after 'import'");
    consume(TokenType::SEMICOLON, "Expected ';' after import statement");
    
    return std::make_unique<ImportStmt>(path_token.value, path_token.location);
}

// 解析extern声明
StmtPtr Parser::externDeclaration() {
    // extern "C" fn name(params) -> type;
    
    // 可选的ABI字符串
    if (match({TokenType::STRING})) {
        // 忽略ABI字符串（目前只支持C）
    }
    
    // 必须是函数声明
    consume(TokenType::KW_FN, "Expected 'fn' after 'extern'");
    
    Token name = consume(TokenType::IDENTIFIER, "Expected function name");
    
    // 解析参数
    consume(TokenType::LPAREN, "Expected '(' after function name");
    std::vector<Parameter> parameters;
    if (!check(TokenType::RPAREN)) {
        do {
            Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
            consume(TokenType::COLON, "Expected ':' after parameter name");
            TypePtr param_type = parseType();
            
            Parameter param;
            param.name = param_name.value;
            param.type = std::move(param_type);
            param.is_self = false;
            param.is_mut_self = false;
            param.location = param_name.location;
            
            parameters.push_back(std::move(param));
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters");
    
    // 解析返回类型
    TypePtr return_type = nullptr;
    if (match({TokenType::ARROW})) {
        return_type = parseType();
    }
    
    consume(TokenType::SEMICOLON, "Expected ';' after extern declaration");
    
    return std::make_unique<ExternStmt>(
        name.value, std::move(parameters), std::move(return_type), name.location
    );
}

// 解析 struct 定义（支持内部方法）
StmtPtr Parser::structDeclaration(Token name_token, std::vector<GenericParam> generic_params, bool is_public) {
    // 注册到Struct符号表（用于识别Struct::method()）
    struct_names_.insert(name_token.value);
    
    // 设置当前解析的struct名（用于Self literal）
    current_parsing_struct_ = name_token.value;
    
    // 解析接口列表: struct(I1, I2, I3)
    std::vector<std::string> interfaces;
    if (match({TokenType::LPAREN})) {
        if (!check(TokenType::RPAREN)) {
            do {
                Token iface_name = consume(TokenType::IDENTIFIER, "Expected interface name");
                interfaces.push_back(iface_name.value);
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RPAREN, "Expected ')' after interfaces");
    }
    
    consume(TokenType::LBRACE, "Expected '{' after struct");
    
    std::vector<StructField> fields;
    std::vector<std::unique_ptr<FunctionStmt>> methods;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // 检查方法的pub修饰符
        bool method_public = false;
        if (match({TokenType::KW_PUB})) {
            method_public = true;
        }
        
        // 检查是否是方法定义
        if (match({TokenType::KW_FN})) {
            auto method = functionDeclaration(method_public);
            methods.push_back(std::unique_ptr<FunctionStmt>(
                static_cast<FunctionStmt*>(method.release())
            ));
        } else {
            // 字段定义
            Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
            consume(TokenType::COLON, "Expected ':' after field name");
            auto field_type = parseType();
            consume(TokenType::COMMA, "Expected ',' after field");
            
            StructField field;
            field.name = field_name.value;
            field.type = std::move(field_type);
            field.location = field_name.location;
            fields.push_back(std::move(field));
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after struct");
    
    return std::make_unique<StructStmt>(
        name_token.value, std::move(generic_params), std::move(interfaces),
        std::move(fields), std::move(methods), is_public, name_token.location
    );
}

// 解析 enum 定义
StmtPtr Parser::enumDeclaration(Token name_token, std::vector<GenericParam> generic_params, bool is_public) {
    // 注册到Enum符号表（用于识别Enum::Variant()）
    enum_names_.insert(name_token.value);
    
    consume(TokenType::LBRACE, "Expected '{' after enum");
    
    std::vector<EnumVariant> variants;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token variant_name = consume(TokenType::IDENTIFIER, "Expected variant name");
        
        EnumVariant variant;
        variant.name = variant_name.value;
        variant.location = variant_name.location;
        
        // 解析关联类型 Variant(T1, T2)
        if (match({TokenType::LPAREN})) {
            if (!check(TokenType::RPAREN)) {
                do {
                    variant.associated_types.push_back(parseType());
                } while (match({TokenType::COMMA}));
            }
            consume(TokenType::RPAREN, "Expected ')' after associated types");
        }
        
        variants.push_back(std::move(variant));
        
        if (!match({TokenType::COMMA})) break;
    }
    
    consume(TokenType::RBRACE, "Expected '}' after enum variants");
    
    return std::make_unique<EnumStmt>(
        name_token.value, std::move(generic_params), 
        std::move(variants), is_public, name_token.location
    );
}

// 解析 interface 定义: type Display = interface { ... }
StmtPtr Parser::interfaceDeclaration(Token name_token, std::vector<GenericParam> generic_params, bool is_public) {
    consume(TokenType::LBRACE, "Expected '{' after interface");
    
    std::vector<MethodSignature> methods;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // 解析方法签名: fn method_name(self, params) -> ReturnType;
        consume(TokenType::KW_FN, "Expected 'fn' in interface");
        
        Token method_name = consume(TokenType::IDENTIFIER, "Expected method name");
        consume(TokenType::LPAREN, "Expected '(' after method name");
        
        // 解析参数
        std::vector<Parameter> params;
        if (!check(TokenType::RPAREN)) {
            do {
                params.push_back(parseParameter());
            } while (match({TokenType::COMMA}));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after parameters");
        
        // 解析返回类型
        TypePtr return_type = nullptr;
        if (match({TokenType::ARROW})) {
            return_type = parseType();
        }
        
        // 解析方法体（默认实现）或分号
        StmtPtr default_body = nullptr;
        if (match({TokenType::LBRACE})) {
            // 有默认实现
            default_body = blockStatement();
        } else {
            // 没有默认实现，必须由实现者提供
            consume(TokenType::SEMICOLON, "Expected ';' after method signature");
        }
        
        MethodSignature sig;
        sig.name = method_name.value;
        sig.parameters = std::move(params);
        sig.return_type = std::move(return_type);
        sig.default_body = std::move(default_body);
        sig.location = method_name.location;
        
        methods.push_back(std::move(sig));
    }
    
    consume(TokenType::RBRACE, "Expected '}' after interface methods");
    
    return std::make_unique<InterfaceStmt>(
        name_token.value, std::move(generic_params),
        std::move(methods), is_public, name_token.location
    );
}

// 解析 support 块: support I for T { ... }
StmtPtr Parser::supportDeclaration() {
    // support<T: Display> InterfaceName for TypeName<T> { methods }
    
    // 1. 解析泛型参数（如 <T: Display>）
    // parseGenericParams() 会自己检查和消费 LT 和 GT
    std::vector<GenericParam> generic_params = parseGenericParams();
    
    // 2. 解析接口名
    Token interface_name = consume(TokenType::IDENTIFIER, "Expected interface name after 'support'");
    
    // 3. 解析 'for'
    consume(TokenType::KW_FOR, "Expected 'for' after interface name");
    
    // 4. 解析类型名
    Token type_name = consume(TokenType::IDENTIFIER, "Expected type name after 'for'");
    
    // 5. 可选：类型的泛型参数 support I for Vec<T>
    std::vector<TypePtr> type_generic_args;
    if (match({TokenType::LT})) {
        do {
            type_generic_args.push_back(parseType());
        } while (match({TokenType::COMMA}));
        consume(TokenType::GT, "Expected '>' after generic arguments");
    }
    
    consume(TokenType::LBRACE, "Expected '{' after support declaration");
    
    // 设置当前struct上下文（支持self参数）
    std::string old_struct = current_parsing_struct_;
    current_parsing_struct_ = type_name.value;
    
    // 解析方法实现
    std::vector<std::unique_ptr<FunctionStmt>> methods;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (match({TokenType::KW_FN})) {
            // fn已被消费，现在解析函数
            Token method_name = consume(TokenType::IDENTIFIER, "Expected method name");
            consume(TokenType::LPAREN, "Expected '(' after method name");
            
            // 解析参数
            std::vector<Parameter> params;
            if (!check(TokenType::RPAREN)) {
                do {
                    params.push_back(parseParameter());
                } while (match({TokenType::COMMA}));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after parameters");
            
            // 解析返回类型
            TypePtr return_type = nullptr;
            if (match({TokenType::ARROW})) {
                return_type = parseType();
            }
            
            // 解析函数体
            consume(TokenType::LBRACE, "Expected '{' before function body");
            auto body = blockStatement();
            
            // 创建FunctionStmt
            auto func = std::make_unique<FunctionStmt>(
                method_name.value, std::vector<GenericParam>{}, std::move(params),
                std::move(return_type), std::move(body), false, method_name.location
            );
            
            methods.push_back(std::move(func));
        } else {
            error("Expected method definition in support block");
            break;
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after support methods");
    
    // 恢复上下文
    current_parsing_struct_ = old_struct;
    
    return std::make_unique<SupportStmt>(
        interface_name.value, type_name.value,
        std::move(type_generic_args), std::move(methods), 
        std::move(generic_params), interface_name.location
    );
}

// 解析 impl 块（已废弃，保留用于向后兼容）
StmtPtr Parser::implDeclaration() {
    error("impl blocks are deprecated. Define methods directly inside struct");
    return nullptr;
}

// 解析模式
PatternPtr Parser::parsePattern() {
    // 元组模式: (x, y, z)
    if (match({TokenType::LPAREN})) {
        Token lparen = previous();
        std::vector<PatternPtr> elements;
        
        // 空元组 ()
        if (check(TokenType::RPAREN)) {
            advance();
            return std::make_unique<TuplePattern>(std::move(elements), lparen.location);
        }
        
        // 解析元组元素
        do {
            elements.push_back(parsePattern());
        } while (match({TokenType::COMMA}));
        
        consume(TokenType::RPAREN, "Expected ')' after tuple pattern");
        
        // 只有一个元素的情况，去掉括号
        if (elements.size() == 1) {
            return std::move(elements[0]);
        }
        
        return std::make_unique<TuplePattern>(std::move(elements), lparen.location);
    }
    
    // _通配符
    if (match({TokenType::IDENTIFIER})) {
        std::string name = previous().value;
        if (name == "_") {
            return std::make_unique<WildcardPattern>(previous().location);
        }
        
        // 检查是否有 :: (完整enum路径)
        if (match({TokenType::DOUBLE_COLON})) {
            std::string variant = consume(TokenType::IDENTIFIER, "Expected variant name").value;
            
            std::vector<PatternPtr> bindings;
            if (match({TokenType::LPAREN})) {
                if (!check(TokenType::RPAREN)) {
                    do {
                        bindings.push_back(parsePattern());
                    } while (match({TokenType::COMMA}));
                }
                consume(TokenType::RPAREN, "Expected ')' after pattern bindings");
            }
            
            return std::make_unique<EnumVariantPattern>(
                name, variant, std::move(bindings), previous().location
            );
        }
        
        // 检查是否是简化的enum变体模式: Some(x) 而非 Option::Some(x)
        if (match({TokenType::LPAREN})) {
            std::vector<PatternPtr> bindings;
            if (!check(TokenType::RPAREN)) {
                do {
                    bindings.push_back(parsePattern());
                } while (match({TokenType::COMMA}));
            }
            consume(TokenType::RPAREN, "Expected ')' after pattern bindings");
            
            // 简化模式：变体名直接用，enum名留空（后续推断）
            return std::make_unique<EnumVariantPattern>(
                "", name, std::move(bindings), previous().location
            );
        }
        
        // 简单标识符模式
        return std::make_unique<IdentifierPattern>(name, previous().location);
    }
    
    error("Expected pattern");
    return nullptr;
}

MatchArm Parser::parseMatchArm() {
    MatchArm arm;
    arm.pattern = parsePattern();
    consume(TokenType::FAT_ARROW, "Expected '=>' after pattern");
    arm.expression = expression();
    return arm;
}

// 解析 is 表达式 (两种形式)
ExprPtr Parser::matchExpression() {
    auto value = assignment();
    
    if (match({TokenType::KW_IS})) {
        Token is_token = previous();
        
        // 形式1: value is pattern (条件判断)
        if (!check(TokenType::LBRACE)) {
            auto pattern = parsePattern();
            return std::make_unique<IsExpr>(
                std::move(value), std::move(pattern), is_token.location
            );
        }
        
        // 形式2: value is { pattern => expr, ... } (完整匹配)
        consume(TokenType::LBRACE, "Expected '{' or pattern after 'is'");
        
        std::vector<MatchArm> arms;
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            arms.push_back(parseMatchArm());
            if (!match({TokenType::COMMA})) break;
        }
        
        consume(TokenType::RBRACE, "Expected '}' after match arms");
        
        return std::make_unique<MatchExpr>(
            std::move(value), std::move(arms), is_token.location
        );
    }
    
    return value;
}

// 类型注册（符号表）
void Parser::registerType(const std::string& name) {
    type_names_.insert(name);
}

bool Parser::isRegisteredType(const std::string& name) const {
    return type_names_.count(name) > 0;
}

/**
 * 克隆类型节点
 */
TypePtr Parser::cloneType(const Type* type) {
    if (!type) return nullptr;
    
    switch (type->kind) {
        case Type::Kind::Primitive: {
            const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(type);
            return std::make_unique<PrimitiveTypeNode>(prim->prim_type, type->location);
        }
        case Type::Kind::Named: {
            const NamedTypeNode* named = static_cast<const NamedTypeNode*>(type);
            std::vector<TypePtr> cloned_args;
            for (const auto& arg : named->generic_args) {
                cloned_args.push_back(cloneType(arg.get()));
            }
            return std::make_unique<NamedTypeNode>(named->name, std::move(cloned_args), type->location);
        }
        case Type::Kind::Array: {
            const ArrayTypeNode* array = static_cast<const ArrayTypeNode*>(type);
            return std::make_unique<ArrayTypeNode>(
                cloneType(array->element_type.get()), array->size, type->location
            );
        }
        case Type::Kind::Slice: {
            const SliceTypeNode* slice = static_cast<const SliceTypeNode*>(type);
            return std::make_unique<SliceTypeNode>(
                cloneType(slice->element_type.get()), type->location
            );
        }
        case Type::Kind::Tuple: {
            const TupleTypeNode* tuple = static_cast<const TupleTypeNode*>(type);
            std::vector<TypePtr> cloned_types;
            for (const auto& elem : tuple->element_types) {
                cloned_types.push_back(cloneType(elem.get()));
            }
            return std::make_unique<TupleTypeNode>(std::move(cloned_types), type->location);
        }
        case Type::Kind::Reference: {
            const ReferenceTypeNode* ref = static_cast<const ReferenceTypeNode*>(type);
            return std::make_unique<ReferenceTypeNode>(
                cloneType(ref->inner_type.get()), ref->is_mutable, type->location
            );
        }
        case Type::Kind::Optional: {
            const OptionalTypeNode* opt = static_cast<const OptionalTypeNode*>(type);
            return std::make_unique<OptionalTypeNode>(
                cloneType(opt->inner_type.get()), type->location
            );
        }
        default:
            return nullptr;
    }
}

/**
 * 从表达式推断类型
 * 支持：引用、字面量、标识符（有限）
 */
TypePtr Parser::inferTypeFromExpr(const Expr* expr) {
    if (!expr) return nullptr;
    
    switch (expr->kind) {
        case Expr::Kind::Unary: {
            const UnaryExpr* unary = static_cast<const UnaryExpr*>(expr);
            
            // 引用类型推断：&x 或 &mut x
            if (unary->op == UnaryExpr::Op::Ref || unary->op == UnaryExpr::Op::RefMut) {
                // 推断operand的类型
                TypePtr inner_type = inferTypeFromExpr(unary->operand.get());
                
                if (!inner_type) {
                    // 如果无法推断，尝试从标识符查找
                    if (unary->operand->kind == Expr::Kind::Identifier) {
                        const IdentifierExpr* id = static_cast<const IdentifierExpr*>(unary->operand.get());
                        // 暂时返回nullptr，后续由codegen处理
                        // TODO: 维护变量类型表以支持更好的推断
                        return nullptr;
                    }
                    return nullptr;
                }
                
                bool is_mutable = (unary->op == UnaryExpr::Op::RefMut);
                return std::make_unique<ReferenceTypeNode>(
                    std::move(inner_type), is_mutable, expr->location
                );
            }
            break;
        }
        
        case Expr::Kind::Integer:
            // 整数字面量 → i32
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I32, expr->location);
        
        case Expr::Kind::Float:
            // 浮点数字面量 → f64
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::F64, expr->location);
        
        case Expr::Kind::Boolean:
            // 布尔字面量 → bool
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::BOOL, expr->location);
        
        case Expr::Kind::String:
            // 字符串字面量 → string
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::STRING, expr->location);
        
        case Expr::Kind::ArrayLiteral: {
            // 数组字面量 → [T; N]
            const ArrayLiteralExpr* array = static_cast<const ArrayLiteralExpr*>(expr);
            if (!array->elements.empty()) {
                TypePtr elem_type = inferTypeFromExpr(array->elements[0].get());
                if (elem_type) {
                    return std::make_unique<ArrayTypeNode>(
                        std::move(elem_type), array->elements.size(), expr->location
                    );
                }
            }
            break;
        }
        
        case Expr::Kind::TupleLiteral: {
            // 元组字面量 → (T, U, V)
            const TupleLiteralExpr* tuple = static_cast<const TupleLiteralExpr*>(expr);
            std::vector<TypePtr> elem_types;
            for (const auto& elem : tuple->elements) {
                TypePtr elem_type = inferTypeFromExpr(elem.get());
                if (elem_type) {
                    elem_types.push_back(std::move(elem_type));
                } else {
                    return nullptr;  // 无法推断某个元素
                }
            }
            return std::make_unique<TupleTypeNode>(std::move(elem_types), expr->location);
        }
        
        case Expr::Kind::StructLiteral: {
            // Struct字面量 → StructType
            const StructLiteralExpr* struct_lit = static_cast<const StructLiteralExpr*>(expr);
            std::vector<TypePtr> empty_generic_args;
            return std::make_unique<NamedTypeNode>(
                struct_lit->type_name, std::move(empty_generic_args), expr->location
            );
        }
        
        case Expr::Kind::EnumVariant: {
            // 枚举变体 → EnumType
            const EnumVariantExpr* enum_variant = static_cast<const EnumVariantExpr*>(expr);
            std::vector<TypePtr> empty_generic_args;
            return std::make_unique<NamedTypeNode>(
                enum_variant->enum_name, std::move(empty_generic_args), expr->location
            );
        }
        
        case Expr::Kind::Binary: {
            // 二元表达式：推断结果类型
            const BinaryExpr* binary = static_cast<const BinaryExpr*>(expr);
            
            // 比较操作符 → bool
            if (binary->op == BinaryExpr::Op::Eq || binary->op == BinaryExpr::Op::Ne ||
                binary->op == BinaryExpr::Op::Lt || binary->op == BinaryExpr::Op::Le ||
                binary->op == BinaryExpr::Op::Gt || binary->op == BinaryExpr::Op::Ge) {
                return std::make_unique<PrimitiveTypeNode>(PrimitiveType::BOOL, expr->location);
            }
            
            // 逻辑操作符 → bool
            if (binary->op == BinaryExpr::Op::And || binary->op == BinaryExpr::Op::Or) {
                return std::make_unique<PrimitiveTypeNode>(PrimitiveType::BOOL, expr->location);
            }
            
            // 算术操作符：从左操作数推断
            return inferTypeFromExpr(binary->left.get());
        }
        
        case Expr::Kind::MemberAccess: {
            // 成员访问：tuple.0 或 obj.field
            const MemberAccessExpr* member = static_cast<const MemberAccessExpr*>(expr);
            
            // 元组字段访问
            if (member->is_tuple_index) {
                TypePtr tuple_type = inferTypeFromExpr(member->object.get());
                if (tuple_type && tuple_type->kind == Type::Kind::Tuple) {
                    const TupleTypeNode* tuple = static_cast<const TupleTypeNode*>(tuple_type.get());
                    if (member->tuple_index < tuple->element_types.size()) {
                        // 克隆元素类型
                        return cloneType(tuple->element_types[member->tuple_index].get());
                    }
                }
            }
            // Struct成员访问：暂时无法推断（需要struct定义信息）
            return nullptr;
        }
        
        case Expr::Kind::Index: {
            // 数组/切片索引：推断元素类型
            const IndexExpr* index = static_cast<const IndexExpr*>(expr);
            TypePtr array_type = inferTypeFromExpr(index->array.get());
            
            if (array_type) {
                if (array_type->kind == Type::Kind::Array) {
                    const ArrayTypeNode* arr = static_cast<const ArrayTypeNode*>(array_type.get());
                    return cloneType(arr->element_type.get());
                } else if (array_type->kind == Type::Kind::Slice) {
                    const SliceTypeNode* slice = static_cast<const SliceTypeNode*>(array_type.get());
                    return cloneType(slice->element_type.get());
                }
            }
            break;
        }
        
        case Expr::Kind::Cast: {
            // 类型转换：目标类型
            const CastExpr* cast = static_cast<const CastExpr*>(expr);
            return cloneType(cast->target_type.get());
        }
        
        case Expr::Kind::IfExpr: {
            // if表达式：从then分支推断
            const IfExpr* if_expr = static_cast<const IfExpr*>(expr);
            return inferTypeFromExpr(if_expr->then_expr.get());
        }
        
        default:
            break;
    }
    
    return nullptr;
}

} // namespace pawc

