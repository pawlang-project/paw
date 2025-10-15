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
#include <stdexcept>

namespace pawc {

// ============================================================================
// 第1部分：核心接口和工具函数
// ============================================================================

Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), current_(0) {}

Program Parser::parse() {
    Program program;
    
    while (!isAtEnd()) {
        try {
            if (auto stmt = statement()) {
                program.statements.push_back(std::move(stmt));
            }
        } catch (const std::exception& e) {
            synchronize();
        }
    }
    
    program.errors = std::move(errors_);
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
    errors_.push_back(CompilerError(message, peek().location));
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
    
    // 解析泛型参数 <T, U>
    std::vector<GenericParam> generic_params;
    if (match({TokenType::LT})) {
        do {
            Token type_param = consume(TokenType::IDENTIFIER, "Expected type parameter name");
            GenericParam param;
            param.name = type_param.value;
            param.location = type_param.location;
            generic_params.push_back(param);
        } while (match({TokenType::COMMA}));
        consume(TokenType::GT, "Expected '>' after generic parameters");
    }
    
    consume(TokenType::LPAREN, "Expected '(' after function name");
    
    std::vector<Parameter> parameters;
    if (!check(TokenType::RPAREN)) {
        do {
            parameters.push_back(parseParameter());
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

ExprPtr Parser::expression() {
    return matchExpression();  // is 表达式优先级最低
}

ExprPtr Parser::assignment() {
    auto expr = logicalOr();
    
    // 检查赋值运算符
    if (match({TokenType::ASSIGN, TokenType::PLUS_EQ, TokenType::MINUS_EQ})) {
        Token op = previous();
        auto value = assignment();  // 右结合
        
        // 支持三种赋值目标：Identifier、MemberAccess、Index
        if (expr->kind != Expr::Kind::Identifier && 
            expr->kind != Expr::Kind::MemberAccess && 
            expr->kind != Expr::Kind::Index) {
            error("Invalid assignment target");
            return expr;
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
    if (match({TokenType::MINUS, TokenType::NOT})) {
        Token op = previous();
        auto operand = unary();
        UnaryExpr::Op operation = (op.type == TokenType::MINUS) ?
            UnaryExpr::Op::Neg : UnaryExpr::Op::Not;
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
        // as类型转换（优先处理）
        if (match({TokenType::KW_AS})) {
            TypePtr target_type = parseType();
            expr = std::make_unique<CastExpr>(std::move(expr), std::move(target_type), previous().location);
            continue;
        }
        
        if (match({TokenType::DOT})) {
            Token member = consume(TokenType::IDENTIFIER, "Expected member name after '.'");
            expr = std::make_unique<MemberAccessExpr>(
                std::move(expr), member.value, member.location
            );
        } else if (match({TokenType::LBRACKET})) {
            // 数组索引访问: arr[index]
            auto index = expression();
            Token bracket = consume(TokenType::RBRACKET, "Expected ']' after index");
            expr = std::make_unique<IndexExpr>(
                std::move(expr), std::move(index), bracket.location
            );
        } else if (match({TokenType::DOUBLE_COLON})) {
            // 支持三种情况：
            // 1. module::function() - 模块函数调用
            // 2. Type::Variant(args) - 枚举变体构造
            // 3. Type::Variant - 枚举变体标识符
            Token name_after_colon = consume(TokenType::IDENTIFIER, "Expected name after '::'");
            
            // 检查是否是函数调用
            if (match({TokenType::LPAREN})) {
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
            }
            
            // 否则只是标识符
            expr = std::make_unique<IdentifierExpr>(name_after_colon.value, name_after_colon.location);
        } else if (check(TokenType::LT) && expr->kind == Expr::Kind::Identifier) {
            // 可能是泛型调用或泛型enum: func<T>(args) 或 Type<T>::Variant
            size_t saved_pos = current_;
            advance();  // 消耗 <
            
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
            
            // 尝试解析类型参数
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
                
                // 生成泛型类型名（Box<i32> → Box_i32）
                std::string mangled_name = name_token.value;
                for (const auto& arg : type_args) {
                    mangled_name += "_";
                    if (arg->kind == Type::Kind::Named) {
                        const NamedTypeNode* named = static_cast<const NamedTypeNode*>(arg.get());
                        mangled_name += named->name;
                    } else if (arg->kind == Type::Kind::Primitive) {
                        const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(arg.get());
                        switch (prim->prim_type) {
                            case PrimitiveType::I32: mangled_name += "i32"; break;
                            case PrimitiveType::I64: mangled_name += "i64"; break;
                            case PrimitiveType::STRING: mangled_name += "string"; break;
                            default: mangled_name += "T"; break;
                        }
                    }
                }
                
                return std::make_unique<StructLiteralExpr>(
                    mangled_name, std::move(field_inits), name_token.location
                );
            } else {
                // 回退
                current_ = saved_pos;
            }
        }
        
        // 检查是否是普通struct literal: Type { field: value, ... }
        // 使用符号表：只有已注册的类型才能构造
        if (isRegisteredType(name_token.value) && match({TokenType::LBRACE})) {
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
            
            return std::make_unique<StructLiteralExpr>(
                name_token.value, std::move(field_inits), name_token.location
            );
        }
        
        return std::make_unique<IdentifierExpr>(name_token.value, name_token.location);
    }
    
    if (match({TokenType::LPAREN})) {
        auto expr = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    error("Expected expression");
    throw std::runtime_error("Expected expression");
}

// ============================================================================
// 第4部分：类型和模式解析（Type & Pattern Parsing）
// ============================================================================

/**
 * 类型解析
 * 支持：基础类型、数组、泛型参数
 */
TypePtr Parser::parseType() {
    // Self类型（在struct方法中使用）
    if (match({TokenType::KW_SELF_TYPE})) {
        return std::make_unique<SelfTypeNode>(previous().location);
    }
    
    // 数组类型: [element_type; size] 或 [element_type]
    if (match({TokenType::LBRACKET})) {
        auto elem_type = parseType();
        
        int size = -1;  // -1表示大小待推导
        SourceLocation loc = elem_type->location;
        
        // 检查是否有显式大小
        if (match({TokenType::SEMICOLON})) {
            Token size_token = consume(TokenType::INTEGER, "Expected array size");
            size = std::stoi(size_token.value);
            loc = size_token.location;
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array type");
        
        return std::make_unique<ArrayTypeNode>(std::move(elem_type), size, loc);
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
            return std::make_unique<PrimitiveTypeNode>(it->second, previous().location);
        }
        
        // 泛型参数: <T, U>
        std::vector<TypePtr> generic_args;
        if (match({TokenType::LT})) {
            do {
                generic_args.push_back(parseType());
            } while (match({TokenType::COMMA}));
            consume(TokenType::GT, "Expected '>' after generic arguments");
        }
        
        // 自定义类型或泛型类型
        // 只有单字母大写才是泛型参数（T, U, K, V等）
        if (std::isupper(type_name[0]) && type_name.length() == 1 && generic_args.empty()) {
            // 单字母大写是泛型: T, U
            return std::make_unique<GenericTypeNode>(type_name, previous().location);
        }
        
        // 多字母大写或其他情况都是命名类型（Status, Point, Option等）
        return std::make_unique<NamedTypeNode>(type_name, std::move(generic_args), previous().location);
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
    param.type = parseType();
    
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
    }
    
    error("Expected 'struct' or 'enum' after '='");
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
        name_token.value, std::move(generic_params), 
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

// 解析 impl 块（已废弃，保留用于向后兼容）
StmtPtr Parser::implDeclaration() {
    error("impl blocks are deprecated. Define methods directly inside struct");
    return nullptr;
}

// 解析模式
PatternPtr Parser::parsePattern() {
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

} // namespace pawc

