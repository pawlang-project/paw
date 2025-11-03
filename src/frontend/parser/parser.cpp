//===--- parser.cpp - Parser Implementation ----------------------*- C++ -*-===//

#include "parser.h"
#include "frontend/parser/ast/pattern.h"

namespace pawc {

Parser::Parser(const std::vector<Token>& tokens, DiagnosticEngine* diag_engine,
               TypeSystem* type_system)
    : tokens_(tokens), current_(0), diag_engine_(diag_engine),
      type_system_(type_system) {}

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> stmts;
    
    while (!isAtEnd()) {
        try {
            stmts.push_back(parseStatement());
        } catch (...) {
            synchronize();
        }
    }
    
    return stmts;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Token处理
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Token Parser::advance() {
    if (!isAtEnd()) current_++;
    return tokens_[current_ - 1];
}

Token Parser::peek() const {
    return tokens_[current_];
}

Token Parser::peekNext() const {
    if (current_ + 1 < tokens_.size()) {
        return tokens_[current_ + 1];
    }
    return tokens_.back();
}

bool Parser::check(TokenType type) const {
    return !isAtEnd() && peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    error(message);
    throw std::runtime_error(message);
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 语句解析
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

StmtPtr Parser::parseStatement() {
    // 检查pub可见性修饰符
    bool is_public = match(TokenType::PUB);
    
    if (match(TokenType::LET)) return parseVarDecl();
    if (match(TokenType::FN)) return parseFunctionDecl(is_public);
    if (match(TokenType::TYPE)) return parseTypeDecl(is_public);
    if (match(TokenType::SUPPORT)) return parseSupportDecl();
    if (match(TokenType::RETURN)) return parseReturnStmt();
    if (match(TokenType::IF)) return parseIfStmt();  // if语句支持
    if (match(TokenType::LOOP)) return parseLoopStmt();
    if (match(TokenType::BREAK)) return parseBreakStmt();
    if (match(TokenType::CONTINUE)) return parseContinueStmt();
    if (check(TokenType::LBRACE)) return parseBlockStmt();
    
    // 如果有pub但后面不是fn/type，报错
    if (is_public) {
        error("pub can only be used with fn or type declarations");
    }
    
    return parseExprStmt();
}

StmtPtr Parser::parseVarDecl() {
    bool is_mutable = match(TokenType::TILDE);  // let ~x
    
    // 检查是否是元组解构: let (a, b) = ...
    if (check(TokenType::LPAREN)) {
        advance();  // 消费 '('
        
        // 解析变量名列表
        std::vector<std::string> names;
        if (!check(TokenType::RPAREN)) {
            do {
                Token var_name = consume(TokenType::IDENTIFIER, "Expected variable name in destructuring");
                names.push_back(var_name.lexeme);
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after destructuring pattern");
        
        // 必须有初始化表达式
        consume(TokenType::EQ, "Expected '=' in destructuring declaration");
        ExprPtr init = parseExpression();
        
        consume(TokenType::SEMICOLON, "Expected ';' after destructuring declaration");
        
        return std::make_unique<DestructuringDecl>(std::move(names), is_mutable, std::move(init));
    }
    
    // 检查是否是结构体解构: let StructName { field1, field2 } = ...
    if (check(TokenType::IDENTIFIER)) {
        Token name_token = peek();
        // Lookahead: 检查identifier后是否有 '{'
        size_t saved_pos = current_;
        advance(); // 消费identifier
        
        if (check(TokenType::LBRACE) && !name_token.lexeme.empty() && std::isupper(name_token.lexeme[0])) {
            // 结构体解构（结构体名首字母大写）
            std::string struct_name = name_token.lexeme;
            advance(); // 消费 '{'
            
            // 解析字段名列表
            std::vector<std::string> field_names;
            if (!check(TokenType::RBRACE)) {
                do {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name in struct destructuring");
                    field_names.push_back(field_name.lexeme);
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RBRACE, "Expected '}' after struct destructuring pattern");
            
            // 必须有初始化表达式
            consume(TokenType::EQ, "Expected '=' in struct destructuring declaration");
            ExprPtr init = parseExpression();
            
            consume(TokenType::SEMICOLON, "Expected ';' after struct destructuring declaration");
            
            return std::make_unique<StructDestructuringDecl>(struct_name, std::move(field_names), 
                                                            is_mutable, std::move(init));
        }
        
        // 回退，不是结构体解构
        current_ = saved_pos;
    }
    
    // 普通变量声明
    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
    
    // 🔧 G4: 类型推导支持 - 类型变为可选
    Type* type = nullptr;
    if (match(TokenType::COLON)) {
        type = parseType();
    }
    // 如果没有类型，从initializer推导
    
    ExprPtr init = nullptr;
    if (match(TokenType::EQ)) {
        init = parseExpression();
    }
    
    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    
    return std::make_unique<VarDecl>(name.lexeme, type, is_mutable, std::move(init));
}

StmtPtr Parser::parseFunctionDecl(bool is_public) {
    Token name = consume(TokenType::IDENTIFIER, "Expected function name");
    
    // 解析泛型参数 <T, U>
    auto generic_params = parseGenericParams();
    
    // 保存当前泛型参数上下文
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LPAREN, "Expected '(' after function name");
    
    std::vector<FunctionDecl::Param> params;
    if (!check(TokenType::RPAREN)) {
        do {
            bool is_mutable = match(TokenType::TILDE);
            Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
            consume(TokenType::COLON, "Expected ':' after parameter name");
            Type* param_type = parseType();
            
            params.push_back({param_name.lexeme, param_type, is_mutable});
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after parameters");
    
    Type* return_type = type_system_->getVoidType();
    if (match(TokenType::ARROW)) {
        return_type = parseType();
    }
    
    // 解析where约束
    auto where_clauses = parseWhereClauses();
    
    StmtPtr body = parseBlockStmt();
    
    // 恢复泛型参数上下文
    current_generic_params_ = saved_generic_params;
    
    auto func_decl = std::make_unique<FunctionDecl>(name.lexeme, std::move(generic_params),
                                                     std::move(params), return_type, 
                                                     std::move(body), std::move(where_clauses),
                                                     is_public);
    
    // 🔧 G3: 泛型函数注册（基础支持）
    if (!generic_params.empty()) {
        // 这是泛型函数，注册为泛型模板
        // 注意：实例化由Monomorphization Pass处理（future work）
        std::vector<std::string> type_param_names;
        for (const auto& gp : generic_params) {
            type_param_names.push_back(gp.name);
        }
        GenericTemplate* tmpl = new GenericTemplate(name.lexeme, type_param_names, func_decl.get());
        type_system_->registerGenericTemplate(tmpl);
    }
    
    return func_decl;
}

StmtPtr Parser::parseTypeDecl(bool is_public) {
    // type Name<T, U> = struct/enum/interface { ... }
    Token name = consume(TokenType::IDENTIFIER, "Expected type name after 'type'");
    
    // 解析泛型参数 <T, U>
    auto generic_params = parseGenericParams();
    
    consume(TokenType::EQ, "Expected '=' after type name");
    
    // 暂时忽略is_public，仅用于语法识别
    // TODO: 在StructDecl/EnumDecl/InterfaceDecl中添加is_public支持
    (void)is_public;  // 避免未使用警告
    
    if (match(TokenType::STRUCT)) {
        return parseStructDecl(name.lexeme, std::move(generic_params));
    } else if (match(TokenType::ENUM)) {
        return parseEnumDecl(name.lexeme, std::move(generic_params));
    } else if (match(TokenType::INTERFACE)) {
        return parseInterfaceDecl(name.lexeme, std::move(generic_params));
    } else {
        error("Expected 'struct', 'enum', or 'interface' after type name");
        throw std::runtime_error("Parse error");
    }
}

StmtPtr Parser::parseStructDecl(const std::string& name, std::vector<GenericParam> generic_params) {
    // struct { field1: Type1, field2: Type2, ... }
    
    // 保存当前泛型参数上下文
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LBRACE, "Expected '{' after 'struct'");
    
    std::vector<StructDecl::Field> fields;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
        consume(TokenType::COLON, "Expected ':' after field name");
        Type* field_type = parseType();
        
        fields.push_back({field_name.lexeme, field_type});
        
        // 逗号是可选的（支持尾随逗号）
        if (!match(TokenType::COMMA)) {
            // 如果没有逗号，必须是}
            if (!check(TokenType::RBRACE)) {
                error("Expected ',' or '}' after field");
            }
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after struct fields");
    match(TokenType::SEMICOLON);  // 分号是可选的
    
    // 创建StructDecl AST节点
    auto struct_decl = std::make_unique<StructDecl>(name, std::move(generic_params), fields);
    
    // 🔧 G1: 泛型系统 - 区分泛型struct和普通struct
    if (!generic_params.empty()) {
        // 这是泛型struct，注册为泛型模板（不立即实例化）
        std::vector<std::string> type_param_names;
        for (const auto& gp : generic_params) {
            type_param_names.push_back(gp.name);
        }
        GenericTemplate* tmpl = new GenericTemplate(name, type_param_names, struct_decl.get());
        type_system_->registerGenericTemplate(tmpl);
    } else {
        // 普通struct，立即注册类型
        StructType* struct_type = new StructType(name, fields);
        type_system_->registerStruct(struct_type);
    }
    
    // 恢复泛型参数上下文
    current_generic_params_ = saved_generic_params;
    
    return struct_decl;
}

StmtPtr Parser::parseEnumDecl(const std::string& name, std::vector<GenericParam> generic_params) {
    // enum { Variant1, Variant2(Type), ... }
    
    // 保存当前泛型参数上下文
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LBRACE, "Expected '{' after 'enum'");
    
    std::vector<EnumVariant> variants;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token variant_name = consume(TokenType::IDENTIFIER, "Expected variant name");
        
        Type* data_type = nullptr;
        // 检查是否有关联数据: Variant(Type)
        if (match(TokenType::LPAREN)) {
            data_type = parseType();
            consume(TokenType::RPAREN, "Expected ')' after variant type");
        }
        
        variants.push_back(EnumVariant(variant_name.lexeme, data_type));
        
        // 逗号是可选的
        if (!match(TokenType::COMMA)) {
            if (!check(TokenType::RBRACE)) {
                error("Expected ',' or '}' after variant");
            }
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after enum variants");
    match(TokenType::SEMICOLON);  // 分号是可选的
    
    // 创建EnumDecl AST节点
    auto enum_decl = std::make_unique<EnumDecl>(name, std::vector<GenericParam>(generic_params), std::move(variants));
    
    // 🔧 泛型系统：区分泛型enum和普通enum
    if (!generic_params.empty()) {
        // 这是泛型enum，注册为泛型模板（不立即实例化）
        std::vector<std::string> type_param_names;
        for (const auto& gp : generic_params) {
            type_param_names.push_back(gp.name);
        }
        GenericTemplate* tmpl = new GenericTemplate(name, type_param_names, enum_decl.get());
        type_system_->registerGenericTemplate(tmpl);
    } else {
        // 普通enum，立即注册类型
        std::vector<std::pair<std::string, Type*>> variant_types;
        for (const auto& v : enum_decl->getVariants()) {
            variant_types.push_back({v.name, v.data_type});
        }
        EnumType* enum_type = new EnumType(name, variant_types);
        type_system_->registerEnum(enum_type);
    }
    
    // 恢复泛型参数上下文
    current_generic_params_ = saved_generic_params;
    
    return enum_decl;
}

StmtPtr Parser::parseInterfaceDecl(const std::string& name, std::vector<GenericParam> generic_params) {
    // interface { fn method1(); fn method2(); ... }
    
    // 保存当前泛型参数上下文
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LBRACE, "Expected '{' after 'interface'");
    
    std::vector<InterfaceMethod> methods;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // 解析方法声明: fn name(params) -> ReturnType;
        consume(TokenType::FN, "Expected 'fn' for interface method");
        Token method_name = consume(TokenType::IDENTIFIER, "Expected method name");
        
        consume(TokenType::LPAREN, "Expected '(' after method name");
        
        // 解析参数
        std::vector<FunctionDecl::Param> params;
        if (!check(TokenType::RPAREN)) {
            // 第一个参数可能是self
            if (check(TokenType::SELF_LOWER)) {
                advance(); // consume 'self'
                // self不需要类型注解，类型由support块推导
                // 如果有更多参数，需要逗号
                if (match(TokenType::COMMA) && !check(TokenType::RPAREN)) {
                    do {
                        bool is_mutable = match(TokenType::TILDE);
                        Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                        consume(TokenType::COLON, "Expected ':' after parameter name");
                        Type* param_type = parseType();
                        
                        params.push_back({param_name.lexeme, param_type, is_mutable});
                    } while (match(TokenType::COMMA));
                }
            } else {
                do {
                    bool is_mutable = match(TokenType::TILDE);
                    Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    consume(TokenType::COLON, "Expected ':' after parameter name");
                    Type* param_type = parseType();
                    
                    params.push_back({param_name.lexeme, param_type, is_mutable});
                } while (match(TokenType::COMMA));
            }
        }
        
        consume(TokenType::RPAREN, "Expected ')' after parameters");
        
        // 解析返回类型（可选）
        Type* return_type = type_system_->getVoidType();
        if (match(TokenType::ARROW)) {
            return_type = parseType();
        }
        
        // 接口方法声明以分号结束
        consume(TokenType::SEMICOLON, "Expected ';' after interface method");
        
        methods.push_back(InterfaceMethod(method_name.lexeme, std::move(params), return_type));
    }
    
    consume(TokenType::RBRACE, "Expected '}' after interface methods");
    match(TokenType::SEMICOLON);  // 分号是可选的
    
    auto interface_decl = std::make_unique<InterfaceDecl>(name, std::move(generic_params), std::move(methods));
    
    // 🔧 G2: 泛型Interface支持
    if (!generic_params.empty()) {
        // 这是泛型interface，注册为泛型模板
        std::vector<std::string> type_param_names;
        for (const auto& gp : generic_params) {
            type_param_names.push_back(gp.name);
        }
        GenericTemplate* tmpl = new GenericTemplate(name, type_param_names, interface_decl.get());
        type_system_->registerGenericTemplate(tmpl);
    } else {
        // 普通interface，立即注册类型
        std::vector<InterfaceType::MethodSignature> method_signatures;
        for (const auto& method : interface_decl->getMethods()) {
            std::vector<Type*> param_types;
            for (const auto& param : method.params) {
                param_types.push_back(param.type);
            }
            method_signatures.emplace_back(method.name, std::move(param_types), method.return_type);
        }
        InterfaceType* interface_type = new InterfaceType(name, std::move(method_signatures));
        type_system_->registerInterface(interface_type);
    }
    
    // 恢复泛型参数上下文
    current_generic_params_ = saved_generic_params;
    
    return interface_decl;
}

StmtPtr Parser::parseSupportDecl() {
    // support TypeName<T> with InterfaceName<U> where T: Trait { fn method() { body } ... }
    Token type_name = consume(TokenType::IDENTIFIER, "Expected type name after 'support'");
    
    // 解析类型的泛型参数 <T, U>
    auto type_generic_params = parseGenericParams();
    
    consume(TokenType::WITH, "Expected 'with' after type name");
    Token interface_name = consume(TokenType::IDENTIFIER, "Expected interface name after 'with'");
    
    // 解析接口的泛型参数（如果适用）
    auto interface_generic_params = parseGenericParams();
    
    // 解析where约束
    auto where_clauses = parseWhereClauses();
    
    consume(TokenType::LBRACE, "Expected '{' after interface name");
    
    // 解析方法实现
    std::vector<std::unique_ptr<FunctionDecl>> methods;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // 解析方法定义: fn name(params) -> ReturnType { body }
        consume(TokenType::FN, "Expected 'fn' for method");
        Token method_name = consume(TokenType::IDENTIFIER, "Expected method name");
        
        consume(TokenType::LPAREN, "Expected '(' after method name");
        
        // 解析参数（支持self）
        std::vector<FunctionDecl::Param> params;
        bool has_self = false;
        
        if (!check(TokenType::RPAREN)) {
            // 第一个参数可能是self
            if (check(TokenType::SELF_LOWER)) {
                advance(); // consume 'self'
                has_self = true;
                // self参数将由TypeChecker推导类型
                // 如果有更多参数，需要逗号
                if (match(TokenType::COMMA) && !check(TokenType::RPAREN)) {
                    do {
                        bool is_mutable = match(TokenType::TILDE);
                        Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                        consume(TokenType::COLON, "Expected ':' after parameter name");
                        Type* param_type = parseType();
                        
                        params.push_back({param_name.lexeme, param_type, is_mutable});
                    } while (match(TokenType::COMMA));
                }
            } else {
                do {
                    bool is_mutable = match(TokenType::TILDE);
                    Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    consume(TokenType::COLON, "Expected ':' after parameter name");
                    Type* param_type = parseType();
                    
                    params.push_back({param_name.lexeme, param_type, is_mutable});
                } while (match(TokenType::COMMA));
            }
        }
        
        consume(TokenType::RPAREN, "Expected ')' after parameters");
        
        // 解析返回类型（可选）
        Type* return_type = type_system_->getVoidType();
        if (match(TokenType::ARROW)) {
            return_type = parseType();
        }
        
        // 解析方法体
        StmtPtr body = parseBlockStmt();
        
        // 方法本身没有泛型参数（泛型参数来自类型）
        methods.push_back(std::make_unique<FunctionDecl>(
            method_name.lexeme, std::vector<GenericParam>{}, 
            std::move(params), return_type, std::move(body)));
    }
    
    consume(TokenType::RBRACE, "Expected '}' after support methods");
    
    return std::make_unique<SupportDecl>(type_name.lexeme, std::move(type_generic_params),
                                         interface_name.lexeme, std::move(interface_generic_params),
                                         std::move(methods), std::move(where_clauses));
}

StmtPtr Parser::parseReturnStmt() {
    ExprPtr value = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        value = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after return");
    return std::make_unique<ReturnStmt>(std::move(value));
}

StmtPtr Parser::parseIfStmt() {
    ExprPtr condition = parseExpression();
    
    // if语句的then分支必须是block（用大括号）
    // parseBlockStmt会自己消费大括号
    if (!check(TokenType::LBRACE)) {
        error("Expected '{' after if condition");
        return nullptr;
    }
    StmtPtr then_stmt = parseBlockStmt();
    
    StmtPtr else_stmt = nullptr;
    if (match(TokenType::ELSE)) {
        // else后面可以是block或另一个if语句
        if (match(TokenType::IF)) {
            // else if - 递归解析
            else_stmt = parseIfStmt();
        } else {
            // else block
            if (!check(TokenType::LBRACE)) {
                error("Expected '{' after else");
                return nullptr;
            }
            else_stmt = parseBlockStmt();
        }
    }
    
    return std::make_unique<IfStmt>(std::move(condition), std::move(then_stmt),
                                    std::move(else_stmt));
}

StmtPtr Parser::parseLoopStmt() {
    // loop有三种形式：
    // 1. loop { } - 无限循环
    // 2. loop condition { } - 条件循环
    // 3. loop i in iterator { } - 迭代循环
    
    // 检查是否是迭代循环 loop i in ...
    if (check(TokenType::IDENTIFIER)) {
        // 可能是loop i in 或 loop condition {
        Token lookahead = peekNext();
        
        if (lookahead.type == TokenType::IN) {
            // loop i in iterator { }
            Token var_name = advance();
            consume(TokenType::IN, "Expected 'in'");
            ExprPtr iterator = parseExpression();
            
            consume(TokenType::LBRACE, "Expected '{' before loop body");
            std::vector<StmtPtr> body_stmts;
            while (!check(TokenType::RBRACE) && !isAtEnd()) {
                body_stmts.push_back(parseStatement());
            }
            consume(TokenType::RBRACE, "Expected '}' after loop body");
            
            StmtPtr body = std::make_unique<BlockStmt>(std::move(body_stmts));
            return std::make_unique<ForStmt>(var_name.lexeme, std::move(iterator), std::move(body));
        }
    }
    
    // 检查是否是条件循环或无限循环
    if (check(TokenType::LBRACE)) {
        // loop { } - 无限循环
        StmtPtr body = parseBlockStmt();
        return std::make_unique<LoopStmt>(std::move(body));
    } else {
        // loop condition { } - 条件循环
        ExprPtr condition = parseExpression();
        StmtPtr body = parseBlockStmt();
        return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
    }
}

StmtPtr Parser::parseWhileStmt() {
    // 已废弃，使用loop代替
    ExprPtr condition = parseExpression();
    StmtPtr body = parseBlockStmt();
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
}

StmtPtr Parser::parseBreakStmt() {
    consume(TokenType::SEMICOLON, "Expected ';' after break");
    return std::make_unique<BreakStmt>();
}

StmtPtr Parser::parseContinueStmt() {
    consume(TokenType::SEMICOLON, "Expected ';' after continue");
    return std::make_unique<ContinueStmt>();
}

StmtPtr Parser::parseBlockStmt() {
    consume(TokenType::LBRACE, "Expected '{'");
    
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // 检查是否是块的最后一个位置（可能是隐式返回）
        // 向前看：解析一个表达式，然后检查是否直接跟着'}'
        size_t saved_pos = current_;
        
        // 尝试解析表达式（可能无分号）
        try {
            // 先检查是否是语句关键字（let, fn, return, if等）
            if (check(TokenType::LET) || check(TokenType::FN) || check(TokenType::TYPE) ||
                check(TokenType::SUPPORT) || check(TokenType::RETURN) || check(TokenType::IF) ||
                check(TokenType::LOOP) || check(TokenType::BREAK) || check(TokenType::CONTINUE) ||
                check(TokenType::LBRACE)) {
                // 按正常语句解析
                stmts.push_back(parseStatement());
            } else {
                // 可能是表达式
                // 先解析表达式
                ExprPtr expr = parseExpression();
                
                // 检查后面是分号还是'}'
                if (check(TokenType::RBRACE)) {
                    // 隐式返回：无分号，直接到'}'
                    stmts.push_back(std::make_unique<ReturnStmt>(std::move(expr)));
                    break;  // 结束块解析
                } else if (match(TokenType::SEMICOLON)) {
                    // 普通表达式语句
                    stmts.push_back(std::make_unique<ExprStmt>(std::move(expr)));
                } else {
                    // 错误：期待';'或'}'
                    error("Expected ';' after expression");
                    // 尝试恢复
                    synchronize();
                }
            }
        } catch (...) {
            // 解析失败，回退按正常方式解析
            current_ = saved_pos;
            stmts.push_back(parseStatement());
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}'");
    
    return std::make_unique<BlockStmt>(std::move(stmts));
}

StmtPtr Parser::parseExprStmt() {
    ExprPtr expr = parseExpression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression");
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::vector<StmtPtr> Parser::parseBlockStmtsWithImplicitReturn() {
    // 解析块中的语句，允许最后一个表达式无分号（用于BlockExpr）
    std::vector<StmtPtr> stmts;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // 检查是否是语句关键字（使用check而不是match）
        if (check(TokenType::LET) || check(TokenType::FN) || check(TokenType::TYPE) ||
            check(TokenType::SUPPORT) || check(TokenType::RETURN) ||
            check(TokenType::LOOP) || check(TokenType::BREAK) || check(TokenType::CONTINUE) ||
            check(TokenType::LBRACE)) {
            // 按正常语句解析
            stmts.push_back(parseStatement());
        } else {
            // 可能是表达式
            ExprPtr expr = parseExpression();
            
            // 检查后面是分号还是'}'
            if (check(TokenType::RBRACE)) {
                // 无分号，最后一个表达式
                stmts.push_back(std::make_unique<ExprStmt>(std::move(expr)));
                break;
            } else if (match(TokenType::SEMICOLON)) {
                // 有分号，普通表达式语句
                stmts.push_back(std::make_unique<ExprStmt>(std::move(expr)));
            } else {
                // 错误：期待';'或'}'
                error("Expected ';' after expression");
                synchronize();
            }
        }
    }
    
    return stmts;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 表达式解析（运算符优先级）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    ExprPtr expr = parseLogicalOr();
    
    if (match(TokenType::EQ)) {
        ExprPtr value = parseAssignment();
        return std::make_unique<BinaryExpr>(TokenType::EQ, std::move(expr),
                                            std::move(value));
    }
    
    return expr;
}

ExprPtr Parser::parseLogicalOr() {
    ExprPtr expr = parseLogicalAnd();
    
    while (match(TokenType::OR_OR)) {
        ExprPtr right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpr>(TokenType::OR_OR, std::move(expr),
                                            std::move(right));
    }
    
    return expr;
}

ExprPtr Parser::parseLogicalAnd() {
    ExprPtr expr = parseEquality();
    
    while (match(TokenType::AND_AND)) {
        ExprPtr right = parseEquality();
        expr = std::make_unique<BinaryExpr>(TokenType::AND_AND, std::move(expr),
                                            std::move(right));
    }
    
    return expr;
}

ExprPtr Parser::parseEquality() {
    ExprPtr expr = parseComparison();
    
    while (match(TokenType::EQ_EQ) || match(TokenType::NOT_EQ)) {
        TokenType op = tokens_[current_ - 1].type;
        ExprPtr right = parseComparison();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

ExprPtr Parser::parseComparison() {
    ExprPtr expr = parseRange();
    
    while (match(TokenType::LESS) || match(TokenType::LESS_EQ) ||
           match(TokenType::GREATER) || match(TokenType::GREATER_EQ)) {
        TokenType op = tokens_[current_ - 1].type;
        ExprPtr right = parseRange();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

ExprPtr Parser::parseRange() {
    ExprPtr expr = parseTerm();
    
    // 检查范围运算符 .. 或 ..=
    if (match(TokenType::DOT_DOT)) {
        bool inclusive = false;
        if (match(TokenType::EQ)) {
            inclusive = true;
        }
        
        // Range可能有或没有end
        ExprPtr end = nullptr;
        if (!check(TokenType::LBRACE) && !check(TokenType::SEMICOLON) && 
            !check(TokenType::RPAREN) && !check(TokenType::RBRACKET) &&
            !check(TokenType::COMMA)) {
            end = parseTerm();
        }
        
        return std::make_unique<RangeExpr>(std::move(expr), std::move(end), inclusive);
    }
    
    return expr;
}

ExprPtr Parser::parseTerm() {
    ExprPtr expr = parseFactor();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op = tokens_[current_ - 1].type;
        ExprPtr right = parseFactor();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

ExprPtr Parser::parseFactor() {
    ExprPtr expr = parseUnary();
    
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT)) {
        TokenType op = tokens_[current_ - 1].type;
        ExprPtr right = parseUnary();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

ExprPtr Parser::parseUnary() {
    if (match(TokenType::MINUS) || match(TokenType::BANG) || match(TokenType::TILDE)) {
        TokenType op = tokens_[current_ - 1].type;
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryExpr>(op, std::move(operand));
    }
    
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();
    
    // 检查结构体字面量: IDENTIFIER { ... }
    // 要求结构体名首字母大写（类型命名约定）
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.get())) {
        std::string name = ident->getName();
        if (check(TokenType::LBRACE) && !name.empty() && std::isupper(name[0])) {
            // 这是结构体字面量（首字母大写的类型名）
            std::string struct_name = name;
            advance(); // consume '{'
            
            std::vector<FieldInit> fields;
            
            // 解析字段初始化列表（支持简写）
            if (!check(TokenType::RBRACE)) {
                do {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
                    
                    // 检查是否是简写形式（字段名与变量名相同）
                    if (check(TokenType::COLON)) {
                        // 完整形式: field: value
                        consume(TokenType::COLON, "Expected ':' after field name");
                        ExprPtr value = parseExpression();
                        fields.push_back(FieldInit(field_name.lexeme, std::move(value)));
                    } else if (check(TokenType::COMMA) || check(TokenType::RBRACE)) {
                        // 简写形式: field （等价于 field: field）
                        ExprPtr value = std::make_unique<IdentifierExpr>(field_name.lexeme);
                        fields.push_back(FieldInit(field_name.lexeme, std::move(value)));
                    } else {
                        error("Expected ':' or ',' after field name");
                    }
                } while (match(TokenType::COMMA) && !check(TokenType::RBRACE));
            }
            
            consume(TokenType::RBRACE, "Expected '}' after struct fields");
            return std::make_unique<StructLiteral>(struct_name, std::move(fields));
        }
    }
    
    while (true) {
        if (match(TokenType::COLON_COLON)) {
            // 静态访问: Type::Variant 或 Type::method
            if (auto* id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                Token member = consume(TokenType::IDENTIFIER, "Expected member name after '::'");
                expr = std::make_unique<StaticAccessExpr>(id->getName(), member.lexeme);
            } else {
                error("Expected type name before '::'");
            }
        }
        else if (check(TokenType::LESS) && dynamic_cast<IdentifierExpr*>(expr.get())) {
            // 泛型函数调用: identity<i32>(42)
            advance(); // consume '<'
            
            std::vector<Type*> type_args;
            do {
                type_args.push_back(parseType());
            } while (match(TokenType::COMMA));
            
            consume(TokenType::GREATER, "Expected '>' after type arguments");
            consume(TokenType::LPAREN, "Expected '(' after type arguments");
            
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments");
            
            auto call_expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
            call_expr->setTypeArgs(type_args);
            expr = std::move(call_expr);
        }
        else if (match(TokenType::LPAREN)) {
            // 普通函数调用
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments");
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
        }
        else if (match(TokenType::DOT)) {
            // 支持两种成员访问：
            // 1. struct.field_name (IDENTIFIER)
            // 2. tuple.0 (INT_LITERAL)
            if (check(TokenType::INT_LITERAL)) {
                Token index = advance();
                expr = std::make_unique<MemberExpr>(std::move(expr), index.lexeme);
            } else {
                Token member = consume(TokenType::IDENTIFIER, "Expected member name or tuple index");
                expr = std::make_unique<MemberExpr>(std::move(expr), member.lexeme);
            }
        }
        else if (match(TokenType::LBRACKET)) {
            ExprPtr index = parseExpression();
            consume(TokenType::RBRACKET, "Expected ']' after index");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
        }
        else if (match(TokenType::IS)) {
            // match表达式: expr is { pattern => expr, ... }
            consume(TokenType::LBRACE, "Expected '{' after 'is'");
            
            std::vector<MatchArm> arms;
            
            // 解析每个分支
            while (!check(TokenType::RBRACE) && !isAtEnd()) {
                // 解析模式
                auto pattern = parsePattern();
                
                // 消费 =>
                consume(TokenType::FAT_ARROW, "Expected '=>' after pattern");
                
                // 解析表达式
                ExprPtr arm_expr = parseExpression();
                
                // 创建分支
                arms.push_back(MatchArm(std::move(pattern), std::move(arm_expr)));
                
                // 逗号是可选的
                if (!match(TokenType::COMMA)) {
                    if (!check(TokenType::RBRACE)) {
                        error("Expected ',' or '}' after match arm");
                    }
                }
            }
            
            consume(TokenType::RBRACE, "Expected '}' after match arms");
            expr = std::make_unique<MatchExpr>(std::move(expr), std::move(arms));
        }
        else if (match(TokenType::QUESTION)) {
            // ? 操作符: expr?
            // 用于Optional类型，自动unwrap或返回null
            expr = std::make_unique<TryExpr>(std::move(expr), TokenType::QUESTION);
        }
        else if (match(TokenType::BANG)) {
            // ! 操作符: expr!
            // 用于Result类型，自动提取或传播错误
            expr = std::make_unique<TryExpr>(std::move(expr), TokenType::BANG);
        }
        else if (match(TokenType::AS)) {
            // as 类型转换: expr as Type
            Type* target_type = parseType();
            expr = std::make_unique<CastExpr>(std::move(expr), target_type);
        }
        else {
            break;
        }
    }
    
    return expr;
}

ExprPtr Parser::parsePrimary() {
    // 字面量
    if (match(TokenType::TRUE)) {
        return std::make_unique<BoolLiteral>(true);
    }
    if (match(TokenType::FALSE)) {
        return std::make_unique<BoolLiteral>(false);
    }
    if (match(TokenType::INT_LITERAL)) {
        return std::make_unique<IntLiteral>(tokens_[current_ - 1].lexeme);
    }
    if (match(TokenType::FLOAT_LITERAL)) {
        return std::make_unique<FloatLiteral>(tokens_[current_ - 1].lexeme);
    }
    if (match(TokenType::STRING_LITERAL)) {
        return std::make_unique<StringLiteral>(tokens_[current_ - 1].lexeme);
    }
    if (match(TokenType::CHAR_LITERAL)) {
        return std::make_unique<CharLiteral>(tokens_[current_ - 1].lexeme[0]);
    }
    
    // null字面量
    if (match(TokenType::NULL_KW)) {
        return std::make_unique<NullLiteral>();
    }
    
    // ok(value) 构造器
    if (match(TokenType::OK)) {
        consume(TokenType::LPAREN, "Expected '(' after 'ok'");
        auto value = parseExpression();
        consume(TokenType::RPAREN, "Expected ')' after ok value");
        
        std::vector<ExprPtr> args;
        args.push_back(std::move(value));
        
        return std::make_unique<CallExpr>(
            std::make_unique<IdentifierExpr>("ok"),
            std::move(args)
        );
    }
    
    // err(message) 构造器
    if (match(TokenType::ERR)) {
        consume(TokenType::LPAREN, "Expected '(' after 'err'");
        auto message = parseExpression();
        consume(TokenType::RPAREN, "Expected ')' after err message");
        
        std::vector<ExprPtr> args;
        args.push_back(std::move(message));
        
        return std::make_unique<CallExpr>(
            std::make_unique<IdentifierExpr>("err"),
            std::move(args)
        );
    }
    
    if (match(TokenType::IDENTIFIER)) {
        return std::make_unique<IdentifierExpr>(tokens_[current_ - 1].lexeme);
    }
    
    // self表达式
    if (match(TokenType::SELF_LOWER)) {
        return std::make_unique<SelfExpr>();
    }
    
    // 数组字面量 [1, 2, 3]
    if (match(TokenType::LBRACKET)) {
        std::vector<ExprPtr> elements;
        
        // 处理空数组 []
        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return std::make_unique<ArrayLiteral>(std::move(elements));
    }
    
    // 括号表达式、元组或闭包 (expr) / (1, 2, 3) / (x: i32) -> i32 { }
    if (match(TokenType::LPAREN)) {
        // 空元组或无参数闭包 () 或 () -> T { }
        if (match(TokenType::RPAREN)) {
            // 检查是否是闭包 () -> T { }
            if (match(TokenType::ARROW)) {
                // 无参数闭包
                Type* return_type = parseType();
                consume(TokenType::LBRACE, "Expected '{' for closure body");
                std::vector<StmtPtr> stmts = parseBlockStmtsWithImplicitReturn();
                consume(TokenType::RBRACE, "Expected '}' after closure body");
                
                ExprPtr body = std::make_unique<BlockExpr>(std::move(stmts));
                
                return std::make_unique<ClosureExpr>(
                    std::vector<ClosureExpr::Param>{},
                    return_type,
                    std::move(body)
                );
            }
            // 空元组 ()
            return std::make_unique<TupleExpr>(std::vector<ExprPtr>{});
        }
        
        // Lookahead: 检查是否是闭包
        // 闭包模式: (identifier : Type ...)
        // 元组/括号: (expression ...)
        bool is_closure = false;
        if (check(TokenType::IDENTIFIER)) {
            size_t saved_pos = current_;
            advance(); // 跳过identifier
            if (check(TokenType::COLON)) {
                is_closure = true;
            }
            current_ = saved_pos; // 回退
        }
        
        if (is_closure) {
            // 解析闭包 (x: i32, y: i32) -> i32 { body }
            std::vector<ClosureExpr::Param> params;
            
            do {
                bool is_mutable = match(TokenType::TILDE);
                Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                
                // 🔧 类型推导支持：参数类型变为可选
                Type* param_type = nullptr;
                if (match(TokenType::COLON)) {
                    param_type = parseType();
                }
                // 如果没有类型，param_type为nullptr，等待TypeChecker推导
                
                params.push_back(ClosureExpr::Param(param_name.lexeme, param_type, is_mutable));
            } while (match(TokenType::COMMA));
            
            consume(TokenType::RPAREN, "Expected ')' after closure parameters");
            
            // 返回类型（可选）
            Type* return_type = nullptr;
            if (match(TokenType::ARROW)) {
                return_type = parseType();
            }
            
            // 闭包体 - 解析为BlockExpr
            consume(TokenType::LBRACE, "Expected '{' for closure body");
            std::vector<StmtPtr> stmts = parseBlockStmtsWithImplicitReturn();
            consume(TokenType::RBRACE, "Expected '}' after closure body");
            
            ExprPtr body = std::make_unique<BlockExpr>(std::move(stmts));
            
            return std::make_unique<ClosureExpr>(
                std::move(params),
                return_type,
                std::move(body)
            );
        }
        
        // 普通括号表达式或元组
        ExprPtr first = parseExpression();
        
        // 检查是否为元组 (至少有一个逗号)
        if (match(TokenType::COMMA)) {
            std::vector<ExprPtr> elements;
            elements.push_back(std::move(first));
            
            // 处理 (expr,) 和 (expr, expr, ...)
            if (!check(TokenType::RPAREN)) {
                do {
                    elements.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after tuple elements");
            return std::make_unique<TupleExpr>(std::move(elements));
        }
        
        // 普通括号表达式
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return first;
    }
    
    // if表达式 if cond { expr } else { expr }
    if (match(TokenType::IF)) {
        ExprPtr condition = parseExpression();
        
        // 解析then分支（块表达式，支持隐式返回）
        consume(TokenType::LBRACE, "Expected '{' after if condition");
        std::vector<StmtPtr> then_stmts = parseBlockStmtsWithImplicitReturn();
        consume(TokenType::RBRACE, "Expected '}' after then block");
        ExprPtr then_expr = std::make_unique<BlockExpr>(std::move(then_stmts));
        
        // 解析else分支（可选）
        ExprPtr else_expr = nullptr;
        if (match(TokenType::ELSE)) {
            consume(TokenType::LBRACE, "Expected '{' after else");
            std::vector<StmtPtr> else_stmts = parseBlockStmtsWithImplicitReturn();
            consume(TokenType::RBRACE, "Expected '}' after else block");
            else_expr = std::make_unique<BlockExpr>(std::move(else_stmts));
        }
        
        return std::make_unique<IfExpr>(std::move(condition), std::move(then_expr), std::move(else_expr));
    }
    
    // 块表达式 { ... }
    if (match(TokenType::LBRACE)) {
        std::vector<StmtPtr> stmts = parseBlockStmtsWithImplicitReturn();
        consume(TokenType::RBRACE, "Expected '}'");
        return std::make_unique<BlockExpr>(std::move(stmts));
    }
    
    error("Expected expression");
    throw std::runtime_error("Parse error");
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 类型解析
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Type* Parser::parseType() {
    // === 切片类型: [T] ===
    // 注意：PawLang使用[T]表示切片（动态大小），[T; N]表示数组（固定大小）
    if (match(TokenType::LBRACKET)) {
        Type* element_type = parseType();
        
        // 检查是否是固定大小数组 [T; N]
        if (match(TokenType::SEMICOLON)) {
            // 固定大小数组 [T; N]
            if (!check(TokenType::INT_LITERAL)) {
                error("Expected array size after ';'");
                consume(TokenType::RBRACKET, "Expected ']' after array type");
                return type_system_->getSliceType(element_type);
            }
            
            Token size_token = advance();
            size_t size = std::stoull(size_token.lexeme);
            consume(TokenType::RBRACKET, "Expected ']' after array size");
            
            // 创建固定大小数组类型
            return type_system_->getArrayType(element_type, size);
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array element type");
        
        // 创建切片类型（动态大小）
        return type_system_->getSliceType(element_type);
    }
    
    // === 函数类型: fn(T1, T2) -> R ===
    if (match(TokenType::FN)) {
        // 已消费 'fn'
        
        consume(TokenType::LPAREN, "Expected '(' after 'fn'");
        
        // 解析参数类型列表
        std::vector<Type*> param_types;
        if (!check(TokenType::RPAREN)) {
            do {
                param_types.push_back(parseType());
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after function parameter types");
        
        // 解析返回类型
        Type* return_type = type_system_->getVoidType();
        if (match(TokenType::ARROW)) {
            return_type = parseType();
        }
        
        // 创建FunctionType
        return type_system_->getFunctionType(param_types, return_type);
    }
    
    // 检查Self类型
    if (match(TokenType::SELF_UPPER)) {
        // Self类型：在support上下文中表示当前类型
        // TODO: 需要从上下文获取实际类型
        // 暂时返回特殊的SelfType标记
        return type_system_->getSelfType();
    }
    
    Token type_token = advance();
    
    // 首先检查是否是当前泛型参数
    for (const auto& generic_param : current_generic_params_) {
        if (type_token.lexeme == generic_param.name) {
            // 创建GenericType
            auto* generic_type = type_system_->getGenericType(type_token.lexeme);
            
            // 处理T? (Optional类型) 和 T! (Result类型)
            if (match(TokenType::QUESTION)) {
                return type_system_->getOptionalType(generic_type);
            }
            if (match(TokenType::BANG)) {
                return type_system_->getResultType(generic_type);
            }
            
            return generic_type;
        }
    }
    
    // 🔧 M6: 泛型类型使用解析: Option<i32>
    if (check(TokenType::LESS)) {
        // 可能是泛型类型！先尝试查找泛型模板
        auto* tmpl = type_system_->lookupGenericTemplate(type_token.lexeme);
        
        if (tmpl) {
            // 是泛型模板！解析类型参数
            match(TokenType::LESS);  // consume '<'
            std::vector<Type*> type_args;
            
            do {
                type_args.push_back(parseType());
            } while (match(TokenType::COMMA));
            
            consume(TokenType::GREATER, "Expected '>' after generic type arguments");
            
            // 实例化泛型类型
            Type* instance_type = type_system_->instantiateGeneric(type_token.lexeme, type_args);
            
            if (!instance_type) {
                error("Failed to instantiate generic type: " + type_token.lexeme);
                return type_system_->getVoidType();
            }
            
            // 处理T? (Optional类型) 和 T! (Result类型)
            if (match(TokenType::QUESTION)) {
                return type_system_->getOptionalType(instance_type);
            }
            if (match(TokenType::BANG)) {
                return type_system_->getResultType(instance_type);
            }
            
            return instance_type;
        }
    }
    
    // 普通类型查找
    Type* base_type = type_system_->lookupType(type_token.lexeme);
    if (!base_type) {
        error("Unknown type: " + type_token.lexeme);
        return type_system_->getVoidType();
    }
    
    // 处理T? (Optional类型) 和 T! (Result类型)
    if (match(TokenType::QUESTION)) {
        return type_system_->getOptionalType(base_type);
    }
    if (match(TokenType::BANG)) {
        return type_system_->getResultType(base_type);
    }
    
    return base_type;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 错误处理
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void Parser::error(const std::string& message) {
    if (diag_engine_) {
        diag_engine_->reportError(message, peek().location);
    }
}

void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        if (tokens_[current_ - 1].type == TokenType::SEMICOLON) {
            return;
        }
        
        switch (peek().type) {
            case TokenType::FN:
            case TokenType::LET:
            case TokenType::IF:
            case TokenType::LOOP:
            case TokenType::RETURN:
                return;
            default:
                advance();
        }
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 泛型参数和where约束解析
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::vector<GenericParam> Parser::parseGenericParams() {
    // 解析 <T, U, V>
    std::vector<GenericParam> params;
    
    if (!match(TokenType::LESS)) {
        return params;  // 没有泛型参数
    }
    
    do {
        Token param_name = consume(TokenType::IDENTIFIER, "Expected generic parameter name");
        params.push_back(GenericParam(param_name.lexeme));
    } while (match(TokenType::COMMA));
    
    consume(TokenType::GREATER, "Expected '>' after generic parameters");
    
    return params;
}

std::vector<WhereClause> Parser::parseWhereClauses() {
    // 解析 where T: Display, U: Debug
    std::vector<WhereClause> clauses;
    
    // 检查当前token是否是WHERE关键字
    if (!match(TokenType::WHERE)) {
        return clauses;  // 没有where子句
    }
    
    // 已消费 'where'
    
    // 解析where子句列表
    do {
        // 检查是否到达函数体（'{'）
        if (check(TokenType::LBRACE)) {
            break;  // 正常结束，准备解析函数体
        }
        
        // 解析 T: Interface
        // 必须是IDENTIFIER（类型参数）
        if (!check(TokenType::IDENTIFIER)) {
            // where后面不是IDENTIFIER，可能是其他token，安全退出
            break;
        }
        
        Token type_param = advance();  // 类型参数名（如T）
        
        // 期望冒号
        if (!check(TokenType::COLON)) {
            // 没有冒号，可能格式错误，安全退出
            break;
        }
        advance();  // 消费冒号
        
        // 期望接口名
        if (!check(TokenType::IDENTIFIER)) {
            // 没有接口名，安全退出
            break;
        }
        
        Token interface_name = advance();  // 接口名（如Display）
        
        // 添加where子句
        clauses.push_back(WhereClause(type_param.lexeme, interface_name.lexeme));
        
        // 检查是否有更多约束（逗号分隔）
        if (!match(TokenType::COMMA)) {
            // 没有逗号，where子句结束
            break;
        }
        
        // 继续解析下一个约束
    } while (!isAtEnd());
    
    return clauses;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pattern解析
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::unique_ptr<Pattern> Parser::parsePattern() {
    // 通配符模式 _
    // 在PawLang中，_作为标识符使用
    if (check(TokenType::IDENTIFIER) && peek().lexeme == "_") {
        advance();
        return std::make_unique<WildcardPattern>();
    }
    
    // 字面量模式
    if (check(TokenType::INT_LITERAL) || check(TokenType::FLOAT_LITERAL) ||
        check(TokenType::STRING_LITERAL) || check(TokenType::CHAR_LITERAL) ||
        check(TokenType::TRUE) || check(TokenType::FALSE)) {
        return parseLiteralPattern();
    }
    
    // 元组模式 (pattern, pattern, ...)
    if (match(TokenType::LPAREN)) {
        return parseTuplePattern();
    }
    
    // 枚举构造器关键字: ok(...), err(...), some(...), none
    if (check(TokenType::OK) || check(TokenType::ERR)) {
        return parseEnumConstructorPattern();
    }
    
    // 变量绑定或枚举模式
    if (check(TokenType::IDENTIFIER)) {
        return parseVariableOrEnumPattern();
    }
    
    error("Expected pattern");
    throw std::runtime_error("Parse error");
}

std::unique_ptr<Pattern> Parser::parseLiteralPattern() {
    if (match(TokenType::TRUE)) {
        return std::make_unique<LiteralPattern>(
            LiteralPattern::Kind::Bool, "true");
    }
    if (match(TokenType::FALSE)) {
        return std::make_unique<LiteralPattern>(
            LiteralPattern::Kind::Bool, "false");
    }
    if (match(TokenType::INT_LITERAL)) {
        return std::make_unique<LiteralPattern>(
            LiteralPattern::Kind::Int, tokens_[current_ - 1].lexeme);
    }
    if (match(TokenType::FLOAT_LITERAL)) {
        return std::make_unique<LiteralPattern>(
            LiteralPattern::Kind::Float, tokens_[current_ - 1].lexeme);
    }
    if (match(TokenType::STRING_LITERAL)) {
        return std::make_unique<LiteralPattern>(
            LiteralPattern::Kind::String, tokens_[current_ - 1].lexeme);
    }
    if (match(TokenType::CHAR_LITERAL)) {
        return std::make_unique<LiteralPattern>(
            LiteralPattern::Kind::Char, tokens_[current_ - 1].lexeme);
    }
    
    error("Expected literal pattern");
    throw std::runtime_error("Parse error");
}

// 解析枚举构造器关键字模式: ok(...), err(...)
std::unique_ptr<Pattern> Parser::parseEnumConstructorPattern() {
    Token constructor = advance();  // OK or ERR
    
    // 映射关键字到标准枚举变体名
    std::string variant_name;
    if (constructor.type == TokenType::OK) {
        variant_name = "Ok";
    } else if (constructor.type == TokenType::ERR) {
        variant_name = "Err";
    }
    
    // 必须有参数列表
    consume(TokenType::LPAREN, "Expected '(' after enum constructor");
    
    // 解析内部模式
    std::vector<std::unique_ptr<Pattern>> inner_patterns;
    
    if (!check(TokenType::RPAREN)) {
        do {
            inner_patterns.push_back(parsePattern());
        } while (match(TokenType::COMMA) && !check(TokenType::RPAREN));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after enum pattern");
    
    // 创建枚举模式，标记为别名
    return std::make_unique<EnumPattern>(
        "", variant_name, std::move(inner_patterns), true
    );
}

std::unique_ptr<Pattern> Parser::parseVariableOrEnumPattern() {
    Token first = consume(TokenType::IDENTIFIER, "Expected identifier");
    
    // 1. 检查静态访问: Option::Some(x)
    if (match(TokenType::COLON_COLON)) {
        std::string type_name = first.lexeme;
        Token variant = consume(TokenType::IDENTIFIER, "Expected variant name after '::'");
        
        if (match(TokenType::LPAREN)) {
            // Option::Some(x, y, ...)
            std::vector<std::unique_ptr<Pattern>> inner_patterns;
            
            if (!check(TokenType::RPAREN)) {
                do {
                    inner_patterns.push_back(parsePattern());
                } while (match(TokenType::COMMA) && !check(TokenType::RPAREN));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after enum pattern");
            return std::make_unique<EnumPattern>(
                type_name, variant.lexeme, std::move(inner_patterns), false
            );
        }
        
        // Option::Some (无参数)
        return std::make_unique<EnumPattern>(
            type_name, variant.lexeme, std::vector<std::unique_ptr<Pattern>>{}, false
        );
    }
    
    // 2. 检查枚举构造器: Some(...) 或 ok(...)
    if (match(TokenType::LPAREN)) {
        // 检查是否是别名（小写开头）
        bool is_alias = !first.lexeme.empty() && std::islower(first.lexeme[0]);
        
        // 规范化别名: ok → Ok, err → Err, some → Some, none → None
        std::string variant_name = first.lexeme;
        if (is_alias) {
            variant_name = normalizeEnumAlias(first.lexeme);
        }
        
        // 解析多个内部模式
        std::vector<std::unique_ptr<Pattern>> inner_patterns;
        
        if (!check(TokenType::RPAREN)) {
            do {
                inner_patterns.push_back(parsePattern());
            } while (match(TokenType::COMMA) && !check(TokenType::RPAREN));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after enum pattern");
        return std::make_unique<EnumPattern>(
            "", variant_name, std::move(inner_patterns), is_alias
        );
    }
    
    // 3. 检查结构体模式: Point { x, y } 或 Point { x: a, y: b }
    if (match(TokenType::LBRACE)) {
        return parseStructPattern(first.lexeme);
    }
    
    // 4. 简单变量绑定或枚举变体 (需要类型检查时区分)
    //    例如: None, Active, 或普通变量 x
    return std::make_unique<VariablePattern>(first.lexeme);
}

// 枚举别名规范化: ok → Ok, err → Err, some → Some, none → None
std::string Parser::normalizeEnumAlias(const std::string& alias) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"ok", "Ok"},
        {"err", "Err"},
        {"some", "Some"},
        {"none", "None"},
    };
    
    auto it = aliases.find(alias);
    if (it != aliases.end()) {
        return it->second;
    }
    
    // 如果不是已知别名，返回首字母大写版本
    if (!alias.empty()) {
        std::string result = alias;
        result[0] = std::toupper(result[0]);
        return result;
    }
    
    return alias;
}

std::unique_ptr<Pattern> Parser::parseTuplePattern() {
    // 已经消费了 LPAREN
    std::vector<std::unique_ptr<Pattern>> elements;
    
    // 空元组模式 ()
    if (match(TokenType::RPAREN)) {
        return std::make_unique<TuplePattern>(std::move(elements));
    }
    
    // 解析第一个模式
    elements.push_back(parsePattern());
    
    // 解析剩余模式
    while (match(TokenType::COMMA)) {
        // 允许尾随逗号
        if (check(TokenType::RPAREN)) {
            break;
        }
        elements.push_back(parsePattern());
    }
    
    consume(TokenType::RPAREN, "Expected ')' after tuple pattern");
    return std::make_unique<TuplePattern>(std::move(elements));
}

//  结构体模式解析: Point { x, y } 或 Point { x: a, y: b }
std::unique_ptr<Pattern> Parser::parseStructPattern(const std::string& struct_name) {
    // 已经消费了 '{', 解析字段列表
    
    std::vector<StructPattern::FieldPattern> fields;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token field_name = consume(TokenType::IDENTIFIER, "Expected field name in struct pattern");
        
        PatternPtr pattern;
        
        // 检查是重绑定形式 (x: a) 还是简写形式 (x)
        if (match(TokenType::COLON)) {
            // 重绑定形式: x: a
            pattern = parsePattern();
        } else {
            // 简写形式: x (等价于 x: x)
            pattern = std::make_unique<VariablePattern>(field_name.lexeme);
        }
        
        fields.push_back(StructPattern::FieldPattern(field_name.lexeme, std::move(pattern)));
        
        // 逗号是可选的（支持尾随逗号）
        if (!match(TokenType::COMMA)) {
            // 如果没有逗号，必须是 }
            if (!check(TokenType::RBRACE)) {
                error("Expected ',' or '}' after struct pattern field");
            }
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after struct pattern fields");
    
    return std::make_unique<StructPattern>(struct_name, std::move(fields));
}

} // namespace pawc
