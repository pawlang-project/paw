//===--- parser.cpp - Parser Implementation ----------------------*- C++ -*-===//
/// @file parser.cpp
/// @brief Recursive descent parser implementation
///
/// Complete syntax analysis for PawLang including expressions, statements,
/// patterns, and type declarations.

#include "parser.h"
#include "frontend/parser/ast/pattern.h"

namespace pawc {

/// Initialize parser with token stream
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
// Token handling
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
// Statement parsing
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

StmtPtr Parser::parseStatement() {
    // Check pub visibility modifier
    bool is_public = match(TokenType::PUB);
    
    if (match(TokenType::LET)) return parseVarDecl();
    if (match(TokenType::FN)) return parseFunctionDecl(is_public);
    if (match(TokenType::TYPE)) return parseTypeDecl(is_public);
    if (match(TokenType::SUPPORT)) return parseSupportDecl();
    if (match(TokenType::RETURN)) return parseReturnStmt();
    if (match(TokenType::IF)) return parseIfStmt();  // if statement support
    if (match(TokenType::LOOP)) return parseLoopStmt();
    if (match(TokenType::BREAK)) return parseBreakStmt();
    if (match(TokenType::CONTINUE)) return parseContinueStmt();
    if (check(TokenType::LBRACE)) return parseBlockStmt();
    
    // If has pub but not followed by fn/type, error
    if (is_public) {
        error("pub can only be used with fn or type declarations");
    }
    
    return parseExprStmt();
}

StmtPtr Parser::parseVarDecl() {
    bool is_mutable = match(TokenType::TILDE);  // let ~x
    
    // Check if it's tuple destructuring: let (a, b) = ...
    if (check(TokenType::LPAREN)) {
        advance();  // Consume '('
        
        // Parse variable name list
        std::vector<std::string> names;
        if (!check(TokenType::RPAREN)) {
            do {
                Token var_name = consume(TokenType::IDENTIFIER, "Expected variable name in destructuring");
                names.push_back(var_name.lexeme);
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after destructuring pattern");
        
        // Must have initialization expression
        consume(TokenType::EQ, "Expected '=' in destructuring declaration");
        ExprPtr init = parseExpression();
        
        consume(TokenType::SEMICOLON, "Expected ';' after destructuring declaration");
        
        return std::make_unique<DestructuringDecl>(std::move(names), is_mutable, std::move(init));
    }
    
    // Check if it's struct destructuring: let StructName { field1, field2 } = ...
    if (check(TokenType::IDENTIFIER)) {
        Token name_token = peek();
        // Lookahead: check if identifier is followed by '{'
        size_t saved_pos = current_;
        advance(); // Consume identifier
        
        if (check(TokenType::LBRACE) && !name_token.lexeme.empty() && std::isupper(name_token.lexeme[0])) {
            // Struct destructuring (struct name starts with capital letter)
            std::string struct_name = name_token.lexeme;
            advance(); // Consume '{'
            
            // Parse field name list
            std::vector<std::string> field_names;
            if (!check(TokenType::RBRACE)) {
                do {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name in struct destructuring");
                    field_names.push_back(field_name.lexeme);
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RBRACE, "Expected '}' after struct destructuring pattern");
            
            // Must have initialization expression
            consume(TokenType::EQ, "Expected '=' in struct destructuring declaration");
            ExprPtr init = parseExpression();
            
            consume(TokenType::SEMICOLON, "Expected ';' after struct destructuring declaration");
            
            return std::make_unique<StructDestructuringDecl>(struct_name, std::move(field_names), 
                                                            is_mutable, std::move(init));
        }
        
        // Backtrack, not struct destructuring
        current_ = saved_pos;
    }
    
    // Regular variable declaration
    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
    
    // Type inference support - type becomes optional
    Type* type = nullptr;
    if (match(TokenType::COLON)) {
        type = parseType();
    }
    // If no type, infer from initializer
    
    ExprPtr init = nullptr;
    if (match(TokenType::EQ)) {
        init = parseExpression();
    }
    
    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    
    return std::make_unique<VarDecl>(name.lexeme, type, is_mutable, std::move(init));
}

StmtPtr Parser::parseFunctionDecl(bool is_public) {
    Token name = consume(TokenType::IDENTIFIER, "Expected function name");
    
    // Parse generic parameters <T, U>
    auto generic_params = parseGenericParams();
    
    // Save current generic parameter context
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LPAREN, "Expected '(' after function name");
    
    std::vector<FunctionDecl::Param> params;
    if (!check(TokenType::RPAREN)) {
        do {
            // Self type support: check if it's self parameter shorthand
            // Support three forms: self, &self, &~self
            if (check(TokenType::AMP) || check(TokenType::SELF_LOWER)) {
                bool is_ref = false;
                bool is_mut = false;
                
                // Check reference prefix: &~self or &self
                if (match(TokenType::AMP)) {
                    is_ref = true;
                    is_mut = match(TokenType::TILDE);  // &~
                }
                
                // Now should be self
                if (match(TokenType::SELF_LOWER)) {
                    Type* self_type = nullptr;
                    
                    // Forbid explicit type annotation
                    if (check(TokenType::COLON)) {
                        error("self parameter does not support explicit type annotation. Use: self, &self, or &~self");
                        return nullptr;
                    }
                    
                    // Infer type from prefix
                    if (is_ref) {
                        // &self or &~self
                        self_type = type_system_->getReferenceType(
                            type_system_->getSelfType(), 
                            is_mut
                        );
                    } else {
                        // self (value passing)
                        self_type = type_system_->getSelfType();
                    }
                    
                    params.push_back({"self", self_type, false});
                } else {
                    // If encountered & but not self, backtrack and process as regular parameter
                    current_--;
                    bool is_mutable = match(TokenType::TILDE);
                    Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    consume(TokenType::COLON, "Expected ':' after parameter name");
                    Type* param_type = parseType();
                    params.push_back({param_name.lexeme, param_type, is_mutable});
                }
            } else {
                // Regular parameter
                bool is_mutable = match(TokenType::TILDE);
                Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                consume(TokenType::COLON, "Expected ':' after parameter name");
                Type* param_type = parseType();
                
                params.push_back({param_name.lexeme, param_type, is_mutable});
            }
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after parameters");
    
    Type* return_type = type_system_->getVoidType();
    if (match(TokenType::ARROW)) {
        return_type = parseType();
    }
    
    // Parse where constraints
    auto where_clauses = parseWhereClauses();
    
    StmtPtr body = parseBlockStmt();
    
    // Restore generic parameter context
    current_generic_params_ = saved_generic_params;
    
    auto func_decl = std::make_unique<FunctionDecl>(name.lexeme, std::move(generic_params),
                                                     std::move(params), return_type, 
                                                     std::move(body), std::move(where_clauses),
                                                     is_public);
    
    // Generic function registration (base support)
    if (!generic_params.empty()) {
        // This is generic function, register as generic template
        // Note: Instantiation handled by Monomorphization Pass (future work)
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
    
    // parsinggenericparameter <T, U>
    auto generic_params = parseGenericParams();
    
    consume(TokenType::EQ, "Expected '=' after type name");
    
    // Temporarily ignore is_public, only used for syntax recognition
    // TODO: Add is_public support in StructDecl/EnumDecl/InterfaceDecl
    (void)is_public;  // Avoid unused warning
    
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
    
    // Save current generic parameter context
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LBRACE, "Expected '{' after 'struct'");
    
    std::vector<StructDecl::Field> fields;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
        consume(TokenType::COLON, "Expected ':' after field name");
        Type* field_type = parseType();
        
        fields.push_back({field_name.lexeme, field_type});
        
        // Comma is optional (support trailing comma)
        if (!match(TokenType::COMMA)) {
            // If no comma, must be }
            if (!check(TokenType::RBRACE)) {
                error("Expected ',' or '}' after field");
            }
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after struct fields");
    match(TokenType::SEMICOLON);  // Semicolon is optional
    
    // Create StructDecl AST node
    auto struct_decl = std::make_unique<StructDecl>(name, std::move(generic_params), fields);
    
    // Generic system - distinguish generic struct from regular struct
    if (!generic_params.empty()) {
        // This is generic struct, register as generic template (don't instantiate immediately)
        std::vector<std::string> type_param_names;
        for (const auto& gp : generic_params) {
            type_param_names.push_back(gp.name);
        }
        GenericTemplate* tmpl = new GenericTemplate(name, type_param_names, struct_decl.get());
        type_system_->registerGenericTemplate(tmpl);
    } else {
        // Regular struct, immediately register type
        StructType* struct_type = new StructType(name, fields);
        type_system_->registerStruct(struct_type);
    }
    
    // Restore generic parameter context
    current_generic_params_ = saved_generic_params;
    
    return struct_decl;
}

StmtPtr Parser::parseEnumDecl(const std::string& name, std::vector<GenericParam> generic_params) {
    // enum { Variant1, Variant2(Type), ... }
    
    // Save current generic parameter context
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LBRACE, "Expected '{' after 'enum'");
    
    std::vector<EnumVariant> variants;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token variant_name = consume(TokenType::IDENTIFIER, "Expected variant name");
        
        // Check if has associated data: Variant(Type1, Type2, ...)
        std::vector<Type*> data_types;
        if (match(TokenType::LPAREN)) {
            // Parse first type
            data_types.push_back(parseType());
            
            // Parse subsequent types (if have commas)
            while (match(TokenType::COMMA)) {
                data_types.push_back(parseType());
            }
            
            consume(TokenType::RPAREN, "Expected ')' after variant types");
        }
        
        variants.push_back(EnumVariant(variant_name.lexeme, data_types));
        
        // Comma is optional
        if (!match(TokenType::COMMA)) {
            if (!check(TokenType::RBRACE)) {
                error("Expected ',' or '}' after variant");
            }
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after enum variants");
    match(TokenType::SEMICOLON);  // Semicolon is optional
    
    // Create EnumDecl AST node
    auto enum_decl = std::make_unique<EnumDecl>(name, std::vector<GenericParam>(generic_params), std::move(variants));
    
    // Generic system: distinguish generic enum from regular enum
    if (!generic_params.empty()) {
        // This is generic enum, register as generic template (don't instantiate immediately)
        std::vector<std::string> type_param_names;
        for (const auto& gp : generic_params) {
            type_param_names.push_back(gp.name);
        }
        GenericTemplate* tmpl = new GenericTemplate(name, type_param_names, enum_decl.get());
        type_system_->registerGenericTemplate(tmpl);
    } else {
        // Regular enum, immediately register type
        std::vector<std::pair<std::string, Type*>> variant_types;
        for (const auto& v : enum_decl->getVariants()) {
            // Multi-parameter variant wrapped as tuple type
            Type* variant_type = nullptr;
            if (v.data_types.empty()) {
                variant_type = nullptr;
            } else if (v.data_types.size() == 1) {
                variant_type = v.data_types[0];
            } else {
                // Multi-parameter: wrap as tuple
                variant_type = type_system_->getTupleType(v.data_types);
            }
            variant_types.push_back({v.name, variant_type});
        }
        EnumType* enum_type = new EnumType(name, variant_types);
        type_system_->registerEnum(enum_type);
    }
    
    // Restore generic parameter context
    current_generic_params_ = saved_generic_params;
    
    return enum_decl;
}

StmtPtr Parser::parseInterfaceDecl(const std::string& name, std::vector<GenericParam> generic_params) {
    // interface { fn method1(); fn method2(); ... }
    
    // Save current generic parameter context
    auto saved_generic_params = current_generic_params_;
    current_generic_params_ = generic_params;
    
    consume(TokenType::LBRACE, "Expected '{' after 'interface'");
    
    std::vector<InterfaceMethod> methods;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // Parse method declaration: fn name(params) -> ReturnType;
        consume(TokenType::FN, "Expected 'fn' for interface method");
        Token method_name = consume(TokenType::IDENTIFIER, "Expected method name");
        
        consume(TokenType::LPAREN, "Expected '(' after method name");
        
        // Parse parameters
        std::vector<FunctionDecl::Param> params;
        if (!check(TokenType::RPAREN)) {
            // Self type support: check if it's self parameter shorthand
            // Support three forms: self, &self, &~self
            if (check(TokenType::AMP) || check(TokenType::SELF_LOWER)) {
                bool is_ref = false;
                bool is_mut = false;
                
                // Check reference prefix: &~self or &self
                if (match(TokenType::AMP)) {
                    is_ref = true;
                    is_mut = match(TokenType::TILDE);  // &~
                }
                
                // Now should be self
                if (match(TokenType::SELF_LOWER)) {
                    Type* self_type = nullptr;
                    
                    // Forbid explicit type annotation
                    if (check(TokenType::COLON)) {
                        error("self parameter does not support explicit type annotation. Use: self, &self, or &~self");
                        return nullptr;
                    }
                    
                    // Infer type from prefix
                    if (is_ref) {
                        // &self or &~self
                        self_type = type_system_->getReferenceType(
                            type_system_->getSelfType(), 
                            is_mut
                        );
                    } else {
                        // self (value passing)
                        self_type = type_system_->getSelfType();
                    }
                    
                    params.push_back({"self", self_type, false});
                } else {
                    // If encountered & but not self, backtrack and process as regular parameter
                    current_--;
                    bool is_mutable = match(TokenType::TILDE);
                    Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    consume(TokenType::COLON, "Expected ':' after parameter name");
                    Type* param_type = parseType();
                    params.push_back({param_name.lexeme, param_type, is_mutable});
                }
                
                // If have more parameters, need comma
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
        
        // Parse return type (optional)
        Type* return_type = type_system_->getVoidType();
        if (match(TokenType::ARROW)) {
            return_type = parseType();
        }
        
        // Support default method implementation
        ExprPtr body = nullptr;
        if (check(TokenType::LBRACE)) {
            // Has method body (default implementation)
            body = parsePrimary();  // BlockExpr
        } else {
            // No method body (pure declaration)
            consume(TokenType::SEMICOLON, "Expected ';' after interface method");
        }
        
        methods.push_back(InterfaceMethod(method_name.lexeme, std::move(params), return_type, std::move(body)));
    }
    
    consume(TokenType::RBRACE, "Expected '}' after interface methods");
    match(TokenType::SEMICOLON);  // Semicolon is optional
    
    // Extract generic parameter names first (before move!)
    std::vector<std::string> generic_param_names;
    for (const auto& gp : generic_params) {
        generic_param_names.push_back(gp.name);
    }
    
    // Generic interface support - unified handling
    // Build method signatures
    std::vector<InterfaceType::MethodSignature> method_signatures;
    for (const auto& method : methods) {
        std::vector<Type*> param_types;
        std::vector<std::string> param_names;  // Save parameter names
        for (const auto& param : method.params) {
            param_types.push_back(param.type);
            param_names.push_back(param.name);  // Save parameter name
        }
        // Pass default implementation info (including parameter names)
        method_signatures.emplace_back(
            method.name, 
            std::move(param_types), 
            method.return_type,
            method.hasDefaultImpl(),      // has_default_impl
            method.body.get(),            // default_body
            std::move(param_names)        // param_names
        );
    }
    
    auto interface_decl = std::make_unique<InterfaceDecl>(name, std::move(generic_params), std::move(methods));
    
    // Always register InterfaceType (whether generic or not)
    InterfaceType* interface_type = new InterfaceType(name, std::move(method_signatures), generic_param_names);
    type_system_->registerInterface(interface_type);
    
    // If generic, additionally register as template (for monomorphization use)
    if (!generic_param_names.empty()) {
        GenericTemplate* tmpl = new GenericTemplate(name, generic_param_names, interface_decl.get());
        type_system_->registerGenericTemplate(tmpl);
    }
    
    // Restore generic parameter context
    current_generic_params_ = saved_generic_params;
    
    return interface_decl;
}

StmtPtr Parser::parseSupportDecl() {
    // support TypeName<T> with InterfaceName<U> where T: Trait { fn method() { body } ... }
    Token type_name = consume(TokenType::IDENTIFIER, "Expected type name after 'support'");
    
    // Parse type's generic parameters <T, U>
    auto type_generic_params = parseGenericParams();
    
    consume(TokenType::WITH, "Expected 'with' after type name");
    Token interface_name = consume(TokenType::IDENTIFIER, "Expected interface name after 'with'");
    
    // Parse interface's generic parameters (if applicable)
    auto interface_generic_params = parseGenericParams();
    
    // Parse where constraints
    auto where_clauses = parseWhereClauses();
    
    consume(TokenType::LBRACE, "Expected '{' after interface name");
    
    // Parse method implementations
    std::vector<std::unique_ptr<FunctionDecl>> methods;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // Parse method definition: fn name(params) -> ReturnType { body }
        consume(TokenType::FN, "Expected 'fn' for method");
        Token method_name = consume(TokenType::IDENTIFIER, "Expected method name");
        
        consume(TokenType::LPAREN, "Expected '(' after method name");
        
        // Parse parameters (support self)
        std::vector<FunctionDecl::Param> params;
        bool has_self = false;
        
        if (!check(TokenType::RPAREN)) {
            // Self type support: check if it's self parameter shorthand
            // Support three forms: self, &self, &~self
            if (check(TokenType::AMP) || check(TokenType::SELF_LOWER)) {
                bool is_ref = false;
                bool is_mut = false;
                
                // Check reference prefix: &~self or &self
                if (match(TokenType::AMP)) {
                    is_ref = true;
                    is_mut = match(TokenType::TILDE);  // &~
                }
                
                // Now should be self
                if (match(TokenType::SELF_LOWER)) {
                    has_self = true;
                    Type* self_type = nullptr;
                    
                    // Forbid explicit type annotation
                    if (check(TokenType::COLON)) {
                        error("self parameter does not support explicit type annotation. Use: self, &self, or &~self");
                        return nullptr;
                    }
                    
                    // Infer type from prefix
                    if (is_ref) {
                        // &self or &~self
                        self_type = type_system_->getReferenceType(
                            type_system_->getSelfType(), 
                            is_mut
                        );
                    } else {
                        // self (value passing)
                        self_type = type_system_->getSelfType();
                    }
                    
                    params.push_back({"self", self_type, false});
                } else {
                    // If encountered & but not self, backtrack and process as regular parameter
                    current_--;
                    bool is_mutable = match(TokenType::TILDE);
                    Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                    consume(TokenType::COLON, "Expected ':' after parameter name");
                    Type* param_type = parseType();
                    params.push_back({param_name.lexeme, param_type, is_mutable});
                }
                
                // If have more parameters, need comma
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
                // Regular parameters
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
        
        // Parse return type (optional)
        Type* return_type = type_system_->getVoidType();
        if (match(TokenType::ARROW)) {
            return_type = parseType();
        }
        
        // Parse method body
        StmtPtr body = parseBlockStmt();
        
        // Method itself has no generic parameters (generic parameters come from type)
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
    
    // if statement's then branch must be a block (with braces)
    // parseBlockStmt will consume braces itself
    if (!check(TokenType::LBRACE)) {
        error("Expected '{' after if condition");
        return nullptr;
    }
    StmtPtr then_stmt = parseBlockStmt();
    
    StmtPtr else_stmt = nullptr;
    if (match(TokenType::ELSE)) {
        // else can be followed by block or another if statement
        if (match(TokenType::IF)) {
            // else if - recursive parse
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
    // loop has three forms:
    // 1. loop { } - infinite loop
    // 2. loop condition { } - conditional loop
    // 3. loop i in iterator { } - iteration loop
    
    // Check if it's iteration loop: loop i in ...
    if (check(TokenType::IDENTIFIER)) {
        // Could be: loop i in or loop condition {
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
    
    // Check if it's conditional loop or infinite loop
    if (check(TokenType::LBRACE)) {
        // loop { } - infinite loop
        StmtPtr body = parseBlockStmt();
        return std::make_unique<LoopStmt>(std::move(body));
    } else {
        // loop condition { } - conditional loop
        ExprPtr condition = parseExpression();
        StmtPtr body = parseBlockStmt();
        return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
    }
}

StmtPtr Parser::parseWhileStmt() {
    // Deprecated, use loop instead
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
        // Check if it's the last position in block (could be implicit return)
        // Lookahead: parse an expression, then check if directly followed by '}'
        size_t saved_pos = current_;
        
        // Try parsing expression (may not have semicolon)
        try {
            // First check if it's a statement keyword (let, fn, return, if, etc.)
            if (check(TokenType::LET) || check(TokenType::FN) || check(TokenType::TYPE) ||
                check(TokenType::SUPPORT) || check(TokenType::RETURN) || check(TokenType::IF) ||
                check(TokenType::LOOP) || check(TokenType::BREAK) || check(TokenType::CONTINUE) ||
                check(TokenType::LBRACE)) {
                // Parse as normal statement
                stmts.push_back(parseStatement());
            } else {
                // Could be expression
                // Parse expression first
                ExprPtr expr = parseExpression();
                
                // Check if followed by semicolon or '}'
                if (check(TokenType::RBRACE)) {
                    // Implicit return: no semicolon, directly to '}'
                    stmts.push_back(std::make_unique<ReturnStmt>(std::move(expr)));
                    break;  // End block parsing
                } else if (match(TokenType::SEMICOLON)) {
                    // Regular expression statement
                    stmts.push_back(std::make_unique<ExprStmt>(std::move(expr)));
                } else {
                    // Error: expect ';' or '}'
                    error("Expected ';' after expression");
                    // Try to recover
                    synchronize();
                }
            }
        } catch (...) {
            // Parsing failure, backtrack and parse normally
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
    // Parse statements in block, allow last expression without semicolon (for BlockExpr)
    std::vector<StmtPtr> stmts;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // Check if it's a statement keyword (use check not match)
        if (check(TokenType::LET) || check(TokenType::FN) || check(TokenType::TYPE) ||
            check(TokenType::SUPPORT) || check(TokenType::RETURN) ||
            check(TokenType::LOOP) || check(TokenType::BREAK) || check(TokenType::CONTINUE) ||
            check(TokenType::LBRACE)) {
            // Parse as normal statement
            stmts.push_back(parseStatement());
        } else {
            // Could be expression
            ExprPtr expr = parseExpression();
            
            // Check if followed by semicolon or '}'
            if (check(TokenType::RBRACE)) {
                // No semicolon, last expression
                stmts.push_back(std::make_unique<ExprStmt>(std::move(expr)));
                break;
            } else if (match(TokenType::SEMICOLON)) {
                // Has semicolon, regular expression statement
                stmts.push_back(std::make_unique<ExprStmt>(std::move(expr)));
            } else {
                // Error: expect ';' or '}'
                error("Expected ';' after expression");
                synchronize();
            }
        }
    }
    
    return stmts;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Expression parsing (operator precedence)
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
    
    // Check range operator .. or ..=
    if (match(TokenType::DOT_DOT)) {
        bool inclusive = false;
        if (match(TokenType::EQ)) {
            inclusive = true;
        }
        
        // Range may or may not have end
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
    
    // Check struct literal: IDENTIFIER { ... }
    // Require struct type name to be capitalized（typesmingnameapproximatelyfixed）
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.get())) {
        std::string name = ident->getName();
        if (check(TokenType::LBRACE) && !name.empty() && std::isupper(name[0])) {
            // This is a struct literal (first character capitalized)ypesname）
            std::string struct_name = name;
            advance(); // consume '{'
            
            std::vector<FieldInit> fields;
            
            // Parsing field initializer list (support shorthand)
            if (!check(TokenType::RBRACE)) {
                do {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
                    
                    // Checkyesnoyesshorthandformstyle/form（fieldnamewithvariablenamesame）
                    if (check(TokenType::COLON)) {
                        // Complete form: field: value
                        consume(TokenType::COLON, "Expected ':' after field name");
                        ExprPtr value = parseExpression();
                        fields.push_back(FieldInit(field_name.lexeme, std::move(value)));
                    } else if (check(TokenType::COMMA) || check(TokenType::RBRACE)) {
                        // Shorthand form: field (equivalent to field: field)
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
            // Static access: Type::Variant or Type::method
            if (auto* id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                Token member = consume(TokenType::IDENTIFIER, "Expected member name after '::'");
                expr = std::make_unique<StaticAccessExpr>(id->getName(), member.lexeme);
            } else {
                error("Expected type name before '::'");
            }
        }
        else if (check(TokenType::LESS)) {
            // Generic function call: f<i32>(x)
            // Check if it's generic type parameter (not comparison operator)
            // Simple strategy: if expr is IdentifierExpr and followed by <, try parsing type parameters
            if (auto* id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                // Possibly generic function call
                match(TokenType::LESS);  // consume '<'
                
                std::vector<Type*> type_args;
                do {
                    type_args.push_back(parseType());
                } while (match(TokenType::COMMA));
                
                consume(TokenType::GREATER, "Expected '>' after generic type arguments");
                
                // Now should have function call '('
                if (match(TokenType::LPAREN)) {
                    std::vector<ExprPtr> args;
                    if (!check(TokenType::RPAREN)) {
                        do {
                            args.push_back(parseExpression());
                        } while (match(TokenType::COMMA));
                    }
                    consume(TokenType::RPAREN, "Expected ')' after arguments");
                    
                    // Create generic function call (save type parameters)
                    auto call_expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
                    call_expr->setTypeArgs(type_args);  // Save type parameters
                    expr = std::move(call_expr);
                } else {
                    error("Expected '(' after generic type arguments");
                }
            } else {
                // Not generic call, break
                break;
            }
        }
        else if (match(TokenType::LPAREN)) {
            // Regular function call (no type parameters)
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
        // Support two kinds of member access:
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
            // match expression: expr is { pattern => expr, ... }
            consume(TokenType::LBRACE, "Expected '{' after 'is'");
            
            std::vector<MatchArm> arms;
            
            // Parse each arm
            while (!check(TokenType::RBRACE) && !isAtEnd()) {
                // Parse pattern
                auto pattern = parsePattern();
                
                // Check if has guard condition (if condition)
                ExprPtr guard = nullptr;
                if (match(TokenType::IF)) {
                    guard = parseExpression();
                }
                
                // Consume =>
                consume(TokenType::FAT_ARROW, "Expected '=>' after pattern");
                
                // Parse expression
                ExprPtr arm_expr = parseExpression();
                
                // Create arm
                arms.push_back(MatchArm(std::move(pattern), std::move(arm_expr), std::move(guard)));
                
                // Comma is optional
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
            // ? operator: expr?
            // Used for Optional types, auto unwrap or return null
            expr = std::make_unique<TryExpr>(std::move(expr), TokenType::QUESTION);
        }
        else if (match(TokenType::BANG)) {
            // ! operator: expr!
            // Used for Result types, auto extract or propagate error
            expr = std::make_unique<TryExpr>(std::move(expr), TokenType::BANG);
        }
        else if (match(TokenType::AS)) {
            // as type conversion: expr as Type
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
    // Literals
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
    
    // none literal - Optional none/null
    if (match(TokenType::NONE)) {
        return std::make_unique<NoneLiteral>();
    }
    
    // ok(value) constructor
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
    
    // err(message) constructor
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
    
    // some(value) constructor - Optional value
    if (match(TokenType::SOME)) {
        consume(TokenType::LPAREN, "Expected '(' after 'some'");
        auto value = parseExpression();
        consume(TokenType::RPAREN, "Expected ')' after some value");
        
        std::vector<ExprPtr> args;
        args.push_back(std::move(value));
        
        return std::make_unique<CallExpr>(
            std::make_unique<IdentifierExpr>("some"),
            std::move(args)
        );
    }
    
    if (match(TokenType::IDENTIFIER)) {
        return std::make_unique<IdentifierExpr>(tokens_[current_ - 1].lexeme);
    }
    
    // self expression
    if (match(TokenType::SELF_LOWER)) {
        return std::make_unique<SelfExpr>();
    }
    
    // array literal [1, 2, 3]
    if (match(TokenType::LBRACKET)) {
        std::vector<ExprPtr> elements;
        
        // Handle empty array []
        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return std::make_unique<ArrayLiteral>(std::move(elements));
    }
    
    // Parenthesized expression, tuple, or closure: (expr) / (1, 2, 3) / (x: i32) -> i32 { }
    if (match(TokenType::LPAREN)) {
        // Empty tuple or parameterless closure: () or () -> T { }
        if (match(TokenType::RPAREN)) {
            // Check if it's closure: () -> T { }
            if (match(TokenType::ARROW)) {
                // Parameterless closure
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
            // Empty tuple ()
            return std::make_unique<TupleExpr>(std::vector<ExprPtr>{});
        }
        
        // Lookahead: check if it's closure
        // Closure pattern: (identifier : Type ...)
        // tuple/parentheses: (expression ...)
        bool is_closure = false;
        if (check(TokenType::IDENTIFIER)) {
            size_t saved_pos = current_;
            advance(); // Skip identifier
            if (check(TokenType::COLON)) {
                is_closure = true;
            }
            current_ = saved_pos; // Backtrack
        }
        
        if (is_closure) {
            // Parse closure: (x: i32, y: i32) -> i32 { body }
            std::vector<ClosureExpr::Param> params;
            
            do {
                bool is_mutable = match(TokenType::TILDE);
                Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                
                // Type inference support: parameter type becomes optional
                Type* param_type = nullptr;
                if (match(TokenType::COLON)) {
                    param_type = parseType();
                }
                // If no type, param_type is nullptr, wait for TypeChecker inference
                
                params.push_back(ClosureExpr::Param(param_name.lexeme, param_type, is_mutable));
            } while (match(TokenType::COMMA));
            
            consume(TokenType::RPAREN, "Expected ')' after closure parameters");
            
            // Return type (optional)
            Type* return_type = nullptr;
            if (match(TokenType::ARROW)) {
                return_type = parseType();
            }
            
            // Closure body - parse as BlockExpr
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
        
        // Regular parenthesized expression or tuple
        ExprPtr first = parseExpression();
        
        // Check if it's tuple (at least one comma)
        if (match(TokenType::COMMA)) {
            std::vector<ExprPtr> elements;
            elements.push_back(std::move(first));
            
            // Handle (expr,) and (expr, expr, ...)
            if (!check(TokenType::RPAREN)) {
                do {
                    elements.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RPAREN, "Expected ')' after tuple elements");
            return std::make_unique<TupleExpr>(std::move(elements));
        }
        
        // Regular parenthesized expression
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return first;
    }
    
    // if expression: if cond { expr } else { expr }
    if (match(TokenType::IF)) {
        ExprPtr condition = parseExpression();
        
        // Parse then branch (block expression, support implicit return)
        consume(TokenType::LBRACE, "Expected '{' after if condition");
        std::vector<StmtPtr> then_stmts = parseBlockStmtsWithImplicitReturn();
        consume(TokenType::RBRACE, "Expected '}' after then block");
        ExprPtr then_expr = std::make_unique<BlockExpr>(std::move(then_stmts));
        
        // Parse else branch (optional)
        ExprPtr else_expr = nullptr;
        if (match(TokenType::ELSE)) {
            consume(TokenType::LBRACE, "Expected '{' after else");
            std::vector<StmtPtr> else_stmts = parseBlockStmtsWithImplicitReturn();
            consume(TokenType::RBRACE, "Expected '}' after else block");
            else_expr = std::make_unique<BlockExpr>(std::move(else_stmts));
        }
        
        return std::make_unique<IfExpr>(std::move(condition), std::move(then_expr), std::move(else_expr));
    }
    
    // block expression: { ... }
    if (match(TokenType::LBRACE)) {
        std::vector<StmtPtr> stmts = parseBlockStmtsWithImplicitReturn();
        consume(TokenType::RBRACE, "Expected '}'");
        return std::make_unique<BlockExpr>(std::move(stmts));
    }
    
    error("Expected expression");
    throw std::runtime_error("Parse error");
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Type parsing
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Type* Parser::parseType() {
    // === Tuple type: (T1, T2, T3, ...) ===
    if (match(TokenType::LPAREN)) {
        std::vector<Type*> element_types;
        
        // Parse tuple element types
        if (!check(TokenType::RPAREN)) {
            do {
                element_types.push_back(parseType());
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after tuple type");
        
        // Create tuple type
        if (element_types.empty()) {
            // () represents void/unit type
            return type_system_->getVoidType();
        }
        return type_system_->getTupleType(element_types);
    }
    
    // === Reference type: &T, &~T ===
    if (match(TokenType::AMP)) {
        // Check if it's mutable reference: &~T
        bool is_mutable = match(TokenType::TILDE);
        
        // Parse referenced type
        Type* pointee_type = parseType();
        
        // Create reference type
        return type_system_->getReferenceType(pointee_type, is_mutable);
    }
    
    // === Slice type: [T] ===
    // Note: PawLang uses [T] for slice (dynamic size), [T; N] for array (fixed size)
    if (match(TokenType::LBRACKET)) {
        Type* element_type = parseType();
        
        // Check if it's fixed-size array: [T; N]
        if (match(TokenType::SEMICOLON)) {
            // Fixed-size array [T; N]
            if (!check(TokenType::INT_LITERAL)) {
                error("Expected array size after ';'");
                consume(TokenType::RBRACKET, "Expected ']' after array type");
                return type_system_->getSliceType(element_type);
            }
            
            Token size_token = advance();
            size_t size = std::stoull(size_token.lexeme);
            consume(TokenType::RBRACKET, "Expected ']' after array size");
            
            // Create fixed-size array type
            return type_system_->getArrayType(element_type, size);
        }
        
        consume(TokenType::RBRACKET, "Expected ']' after array element type");
        
        // Create slice type (dynamic size)
        return type_system_->getSliceType(element_type);
    }
    
    // === functiontypes: fn(T1, T2) -> R ===
    if (match(TokenType::FN)) {
        // Already consumed 'fn'
        
        consume(TokenType::LPAREN, "Expected '(' after 'fn'");
        
        // Parse parameter types list
        std::vector<Type*> param_types;
        if (!check(TokenType::RPAREN)) {
            do {
                param_types.push_back(parseType());
            } while (match(TokenType::COMMA));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after function parameter types");
        
        // Parse return type
        Type* return_type = type_system_->getVoidType();
        if (match(TokenType::ARROW)) {
            return_type = parseType();
        }
        
        // Create FunctionType
        return type_system_->getFunctionType(param_types, return_type);
    }
    
    // Check Self type
    if (match(TokenType::SELF_UPPER)) {
        // Self type: represents current type in support context
        // TODO: Need to get actual type from context
        // Temporarily return special SelfType marker
        return type_system_->getSelfType();
    }
    
    Token type_token = advance();
    
    // First check if it's current generic parameter
    for (const auto& generic_param : current_generic_params_) {
        if (type_token.lexeme == generic_param.name) {
            // Create GenericType
            auto* generic_type = type_system_->getGenericType(type_token.lexeme);
            
            // Handle T? (Optional type) and T! (Result type)
            if (match(TokenType::QUESTION)) {
                return type_system_->getOptionalType(generic_type);
            }
            if (match(TokenType::BANG)) {
                return type_system_->getResultType(generic_type);
            }
            
            return generic_type;
        }
    }
    
    // Generic type usage parse: Option<i32>
    if (check(TokenType::LESS)) {
        // Possibly generic type! Try looking up generic template first
        auto* tmpl = type_system_->lookupGenericTemplate(type_token.lexeme);
        
        if (tmpl) {
            // Is generic template! Parse type parameters
            match(TokenType::LESS);  // consume '<'
            std::vector<Type*> type_args;
            
            do {
                type_args.push_back(parseType());
            } while (match(TokenType::COMMA));
            
            consume(TokenType::GREATER, "Expected '>' after generic type arguments");
            
            // Instantiate generic type
            Type* instance_type = type_system_->instantiateGeneric(type_token.lexeme, type_args);
            
            if (!instance_type) {
                error("Failed to instantiate generic type: " + type_token.lexeme);
                return type_system_->getVoidType();
            }
            
            // Handle T? (Optional type) and T! (Result type)
            if (match(TokenType::QUESTION)) {
                return type_system_->getOptionalType(instance_type);
            }
            if (match(TokenType::BANG)) {
                return type_system_->getResultType(instance_type);
            }
            
            return instance_type;
        }
    }
    
    // Regular type lookup
    Type* base_type = type_system_->lookupType(type_token.lexeme);
    if (!base_type) {
        error("Unknown type: " + type_token.lexeme);
        return type_system_->getVoidType();
    }
    
    // Handle T? (Optional type) and T! (Result type)
    if (match(TokenType::QUESTION)) {
        return type_system_->getOptionalType(base_type);
    }
    if (match(TokenType::BANG)) {
        return type_system_->getResultType(base_type);
    }
    
    return base_type;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Error handling
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
// Generic parameters and where constraints parsing
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::vector<GenericParam> Parser::parseGenericParams() {
    // Parse <T, U, V>
    std::vector<GenericParam> params;
    
    if (!match(TokenType::LESS)) {
        return params;  // No generic parameters
    }
    
    do {
        Token param_name = consume(TokenType::IDENTIFIER, "Expected generic parameter name");
        params.push_back(GenericParam(param_name.lexeme));
    } while (match(TokenType::COMMA));
    
    consume(TokenType::GREATER, "Expected '>' after generic parameters");
    
    return params;
}

std::vector<WhereClause> Parser::parseWhereClauses() {
    // Parse where T: Display, U: Debug
    std::vector<WhereClause> clauses;
    
    // Check if current token is WHERE keyword
    if (!match(TokenType::WHERE)) {
        return clauses;  // No where clauses
    }
    
    // Already consumed 'where'
    
    // Parse where clause list
    do {
        // Check if reached function body ('{')
        if (check(TokenType::LBRACE)) {
            break;  // Normal end, ready to parse function body
        }
        
        // Parse T: Interface
        // Must be IDENTIFIER (type parameter)
        if (!check(TokenType::IDENTIFIER)) {
            // Not IDENTIFIER after where, possibly other token, safe exit
            break;
        }
        
        Token type_param = advance();  // Type parameter name (like T)
        
        // Expect colon
        if (!check(TokenType::COLON)) {
            // No colon, possibly format error, safe exit
            break;
        }
        advance();  // Consume colon
        
        // Expect interface name
        if (!check(TokenType::IDENTIFIER)) {
            // No interface name, safe exit
            break;
        }
        
        Token interface_name = advance();  // Interface name (like Display)
        
        // Add where clause
        clauses.push_back(WhereClause(type_param.lexeme, interface_name.lexeme));
        
        // Support + syntax: T: Display + Debug
        // If there's +, continue parsing more interfaces
        while (match(TokenType::PLUS)) {
            if (!check(TokenType::IDENTIFIER)) {
                error("Expected interface name after '+'");
                break;
            }
            Token next_interface = advance();
            clauses.push_back(WhereClause(type_param.lexeme, next_interface.lexeme));
        }
        
        // Check if there are more constraints (comma-separated)
        if (!match(TokenType::COMMA)) {
            // No comma, where clause ends
            break;
        }
        
        // Continue parsing next constraint
    } while (!isAtEnd());
    
    return clauses;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pattern parsing
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::unique_ptr<Pattern> Parser::parsePattern() {
    // Parse base pattern
    auto pattern = parseBasePattern();
    
    // Check if it's OR pattern: pattern | pattern | pattern
    if (match(TokenType::PIPE)) {
        std::vector<std::unique_ptr<Pattern>> alternatives;
        alternatives.push_back(std::move(pattern));
        
        do {
            alternatives.push_back(parseBasePattern());
        } while (match(TokenType::PIPE));
        
        return std::make_unique<OrPattern>(std::move(alternatives));
    }
    
    return pattern;
}

std::unique_ptr<Pattern> Parser::parseBasePattern() {
    // Wildcard pattern _
    if (check(TokenType::IDENTIFIER) && peek().lexeme == "_") {
        advance();
        return std::make_unique<WildcardPattern>();
    }
    
    // Literal pattern (possibly followed by range)
    if (check(TokenType::INT_LITERAL) || check(TokenType::FLOAT_LITERAL) ||
        check(TokenType::STRING_LITERAL) || check(TokenType::CHAR_LITERAL) ||
        check(TokenType::TRUE) || check(TokenType::FALSE)) {
        auto lit = parseLiteralPattern();
        
        // Check if it's range pattern: 1..10 or 1..=10
        if (check(TokenType::DOT_DOT)) {
            return parseRangePattern(std::move(lit));
        }
        
        return lit;
    }
    
    // Array pattern: [pattern, pattern, ...]
    if (match(TokenType::LBRACKET)) {
        return parseArrayPattern();
    }
    
    // Tuple pattern: (pattern, pattern, ...)
    if (match(TokenType::LPAREN)) {
        return parseTuplePattern();
    }
    
    // Enum constructor keywords: ok(...), err(...), some(...), none
    if (check(TokenType::OK) || check(TokenType::ERR) || 
        check(TokenType::SOME) || check(TokenType::NONE)) {
        return parseEnumConstructorPattern();
    }
    
    // Variable binding or enum pattern
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

// Parse range pattern: start..end or start..=end
std::unique_ptr<Pattern> Parser::parseRangePattern(std::unique_ptr<Pattern> start) {
    bool inclusive = false;
    
    // Consume ..
    if (!match(TokenType::DOT_DOT)) {
        error("Expected '..' for range pattern");
        return nullptr;
    }
    
    // Check if it's ..= (inclusive range)
    if (match(TokenType::EQ)) {
        inclusive = true;
    }
    
    // Parse end value
    auto end = parseLiteralPattern();
    
    return std::make_unique<RangePattern>(std::move(start), std::move(end), inclusive);
}

// Parse array/slice pattern: [pattern, ..] / [pattern, ..rest] / [a, .., z]
std::unique_ptr<Pattern> Parser::parseArrayPattern() {
    std::vector<std::unique_ptr<Pattern>> prefix;
    std::unique_ptr<Pattern> rest = nullptr;
    std::vector<std::unique_ptr<Pattern>> suffix;
    bool has_rest = false;
    
    if (!check(TokenType::RBRACKET)) {
        do {
            // Check rest pattern: .. or ..rest
            if (match(TokenType::DOT_DOT)) {
                has_rest = true;
                
                // Check if has name
                if (check(TokenType::IDENTIFIER)) {
                    // Named form: [a, ..rest] or [a, ..rest, z]
                    Token rest_name = advance();
                    rest = std::make_unique<VariablePattern>(rest_name.lexeme);
                } else {
                    // Anonymous form: [a, ..] or [a, .., z]
                    rest = nullptr;
                }
                
                // Check if has suffix elements
                if (match(TokenType::COMMA) && !check(TokenType::RBRACKET)) {
                    // Has suffix: [a, .., z] or [a, ..rest, z]
                    do {
                        suffix.push_back(parseBasePattern());
                    } while (match(TokenType::COMMA) && !check(TokenType::RBRACKET));
                }
                
                break;  // Can't have another .. after rest
            }
            
            // Regular element
            prefix.push_back(parseBasePattern());
        } while (match(TokenType::COMMA) && !check(TokenType::RBRACKET));
    }
    
    consume(TokenType::RBRACKET, "Expected ']'");
    
    // If has rest, return SlicePattern
    if (has_rest) {
        return std::make_unique<SlicePattern>(
            std::move(prefix), 
            std::move(rest),
            std::move(suffix)
        );
    }
    
    // Otherwise return fixed-size ArrayPattern
    return std::make_unique<ArrayPattern>(
        std::move(prefix), 
        prefix.size()
    );
}

// Parse slice pattern helper method (for future extension)
std::unique_ptr<Pattern> Parser::parseSlicePattern(std::vector<std::unique_ptr<Pattern>> prefix) {
    // TODO: Implementing ...rest syntax requires adding DOT_DOT_DOT token first
    // Current version: SlicePattern used for TypeChecker, not created in Parser yet
    return nullptr;
}

// parsingenumconstructorkeywordpattern: ok(...), err(...), some(...), none
std::unique_ptr<Pattern> Parser::parseEnumConstructorPattern() {
    Token constructor = advance();  // OK, ERR, SOME, or NONE
    
    // mapkeywordtostandardenumvariabletype name
    std::string variant_name;
    if (constructor.type == TokenType::OK) {
        variant_name = "Ok";
    } else if (constructor.type == TokenType::ERR) {
        variant_name = "Err";
    } else if (constructor.type == TokenType::SOME) {
        variant_name = "Some";
    } else if (constructor.type == TokenType::NONE) {
        variant_name = "None";
    }
    
    // none specialhandle：noparametervariant
    if (constructor.type == TokenType::NONE) {
        // none nohasparameter，directlyreturn
        return std::make_unique<EnumPattern>(
            "Option",  // assumptiontypesnameis/as Option
            variant_name,
            std::vector<std::unique_ptr<Pattern>>()
        );
    }
    
    // Other typesconstructormusthasparameterlist
    consume(TokenType::LPAREN, "Expected '(' after enum constructor");
    
    // parsinginternal/insidepattern
    std::vector<std::unique_ptr<Pattern>> inner_patterns;
    
    if (!check(TokenType::RPAREN)) {
        do {
            inner_patterns.push_back(parsePattern());
        } while (match(TokenType::COMMA) && !check(TokenType::RPAREN));
    }
    
    consume(TokenType::RPAREN, "Expected ')' after enum pattern");
    
    // createenumpattern，markeris/asalias
    return std::make_unique<EnumPattern>(
        "", variant_name, std::move(inner_patterns), true
    );
}

std::unique_ptr<Pattern> Parser::parseVariableOrEnumPattern() {
    Token first = consume(TokenType::IDENTIFIER, "Expected identifier");
    
    // 1. checkstaticvisit: Option::Some(x)
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
        
        // Option::Some (noparameter)
        return std::make_unique<EnumPattern>(
            type_name, variant.lexeme, std::vector<std::unique_ptr<Pattern>>{}, false
        );
    }
    
    // 2. checkenumconstructor: Some(...) or ok(...)
    if (match(TokenType::LPAREN)) {
        // Check if it's an alias (lowercase prefix)
        bool is_alias = !first.lexeme.empty() && std::islower(first.lexeme[0]);
        
        // Normalize alias: ok → Ok, err → Err, some → Some, none → Nonee → None
        std::string variant_name = first.lexeme;
        if (is_alias) {
            variant_name = normalizeEnumAlias(first.lexeme);
        }
        
        // parsingmany/muchindividual/pieceinternal/insidepattern
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
    
    // 3. Check struct pattern: Point { x, y } or Point { x: a, y: b }: b }
    if (match(TokenType::LBRACE)) {
        return parseStructPattern(first.lexeme);
    }
    
    // 4. Simple single variable bind or enum variant (needs type check to distinguish)
    //    Example: None, Active, or regular variable x
    return std::make_unique<VariablePattern>(first.lexeme);
}

// Enum alias normalization: ok → Ok, err → Err, some → Some, none → None none → None
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
    
    // If not known alias, return capitalized version
    if (!alias.empty()) {
        std::string results = alias;
        results[0] = std::toupper(results[0]);
        return results;
    }
    
    return alias;
}

std::unique_ptr<Pattern> Parser::parseTuplePattern() {
    // Already consumed LPAREN
    std::vector<std::unique_ptr<Pattern>> elements;
    
    // Empty tuple pattern ()
    if (match(TokenType::RPAREN)) {
        return std::make_unique<TuplePattern>(std::move(elements));
    }
    
    // Parse first pattern
    elements.push_back(parsePattern());
    
    // Parse remaining patterns
    while (match(TokenType::COMMA)) {
        // Allow trailing comma
        if (check(TokenType::RPAREN)) {
            break;
        }
        elements.push_back(parsePattern());
    }
    
    consume(TokenType::RPAREN, "Expected ')' after tuple pattern");
    return std::make_unique<TuplePattern>(std::move(elements));
}

// Struct pattern parsing: Point { x, y } or Point { x: a, y: b }
std::unique_ptr<Pattern> Parser::parseStructPattern(const std::string& struct_name) {
    // Already consumed '{', parse field list
    
    std::vector<StructPattern::FieldPattern> fields;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token field_name = consume(TokenType::IDENTIFIER, "Expected field name in struct pattern");
        
        PatternPtr pattern;
        
        // Check if it's rebinding form (x: a) or shorthand form (x)
        if (match(TokenType::COLON)) {
            // Rebinding form: x: a
            pattern = parsePattern();
        } else {
            // Shorthand form: x (equivalent to x: x)
            pattern = std::make_unique<VariablePattern>(field_name.lexeme);
        }
        
        fields.push_back(StructPattern::FieldPattern(field_name.lexeme, std::move(pattern)));
        
        // Comma is optional (support trailing comma)
        if (!match(TokenType::COMMA)) {
            // If no comma, must be }
            if (!check(TokenType::RBRACE)) {
                error("Expected ',' or '}' after struct pattern field");
            }
        }
    }
    
    consume(TokenType::RBRACE, "Expected '}' after struct pattern fields");
    
    return std::make_unique<StructPattern>(struct_name, std::move(fields));
}

} // namespace pawc
