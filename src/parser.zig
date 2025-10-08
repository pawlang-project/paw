//! Parser - Paw Language Parser
//! 
//! This module implements the syntax analysis phase of the Paw compiler.
//! It converts a stream of tokens into an Abstract Syntax Tree (AST).
//!
//! Structure:
//!   - Parser struct (lines 1-100)
//!   - Declaration parsing (lines 100-500)
//!   - Statement parsing (lines 500-900)
//!   - Expression parsing (lines 900-1500)
//!   - Helper functions (lines 1500-1700)
//!
//! Features:
//!   - Context-aware parsing (type table for generic disambiguation)
//!   - Two-pass parsing (collect types, then parse)
//!   - String interpolation support
//!   - Error propagation (?) operator
//!   - Pattern matching (is expression)

const std = @import("std");
const Token = @import("token.zig").Token;
const TokenType = @import("token.zig").TokenType;
const ast = @import("ast.zig");

// ============================================================================
// Parser Structure
// ============================================================================

pub const Parser = struct {
    allocator: std.mem.Allocator,
    tokens: []Token,
    current: usize,
    // 🆕 类型名集合（用于消除泛型歧义）
    known_types: std.StringHashMap(void),

    pub fn init(allocator: std.mem.Allocator, tokens: []Token) Parser {
        return Parser{
            .allocator = allocator,
            .tokens = tokens,
            .current = 0,
            .known_types = std.StringHashMap(void).init(allocator),
        };
    }

    pub fn deinit(self: *Parser) void {
        self.known_types.deinit();
    }
    
    // 🆕 第一遍：快速收集所有类型名（用于消除泛型歧义）
    fn collectTypes(self: *Parser) !void {
        const start_pos = self.current;
        defer self.current = start_pos;  // 确保恢复位置
        
        while (!self.isAtEnd()) {
            // 只关注 type 关键字
            if (self.check(.keyword_type)) {
                _ = self.advance();  // 消费 type
                
                // 检查是否是 pub
                if (self.check(.keyword_pub)) {
                    _ = self.advance();
                }
                
                // 获取类型名
                if (self.check(.identifier)) {
                    const name = self.advance();
                    try self.known_types.put(name.lexeme, {});
                }
                
                // 快速跳过定义体（不需要完整解析）
                var brace_count: i32 = 0;
                var found_brace = false;
                
                while (!self.isAtEnd()) {
                    if (self.check(.lbrace)) {
                        _ = self.advance();
                        brace_count += 1;
                        found_brace = true;
                    } else if (self.check(.rbrace)) {
                        _ = self.advance();
                        brace_count -= 1;
                        if (brace_count == 0 and found_brace) {
                            break;  // 完成一个类型定义
                        }
                    } else if (self.check(.assign)) {
                        // type alias 形式：type Alias = OtherType
                        _ = self.advance();
                        // 跳过到分号或下一个声明
                        while (!self.isAtEnd() and !self.check(.keyword_type) and 
                               !self.check(.keyword_fn) and !self.check(.keyword_import)) {
                            _ = self.advance();
                        }
                        break;
                    } else {
                        _ = self.advance();
                    }
                }
            } else {
                _ = self.advance();
            }
        }
    }

    pub fn parse(self: *Parser) !ast.Program {
        // 🆕 第一遍：收集所有类型名
        try self.collectTypes();
        
        // 🆕 重置位置，准备第二遍解析
        self.current = 0;
        
        // 第二遍：完整解析（现在 known_types 已经有所有类型了）
        var declarations = std.ArrayList(ast.TopLevelDecl).init(self.allocator);
        
        while (!self.isAtEnd()) {
            const decl = try self.parseTopLevelDecl();
            try declarations.append(decl);
        }
        
        return ast.Program{
            .declarations = try declarations.toOwnedSlice(),
        };
    }

    // ============================================================================
    // Declaration Parsing
    // ============================================================================
    
    fn parseTopLevelDecl(self: *Parser) !ast.TopLevelDecl {
        const is_public = self.match(.keyword_pub);
        
        // Paw 统一语法：只支持 type 和 fn
        if (self.match(.keyword_type)) {
            const type_decl = try self.parseTypeDecl(is_public);
            return ast.TopLevelDecl{ .type_decl = type_decl };
        } else if (self.match(.keyword_fn)) {
            const func = try self.parseFunctionDecl(is_public, false);
            return ast.TopLevelDecl{ .function = func };
        } else if (self.match(.keyword_import)) {
            const import_decl = try self.parseImportDecl();
            return ast.TopLevelDecl{ .import_decl = import_decl };
        } else {
            return error.UnexpectedToken;
        }
    }

    fn parseFunctionDecl(self: *Parser, is_public: bool, is_async: bool) !ast.FunctionDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8).init(self.allocator);
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        // 解析参数
        _ = try self.consume(.lparen);
        var params = std.ArrayList(ast.Param).init(self.allocator);
        
        while (!self.check(.rparen) and !self.isAtEnd()) {
            // 🆕 支持 self 和 mut self 参数
            var param_name: []const u8 = undefined;
            var param_type: ast.Type = undefined;
            
            if (self.match(.keyword_mut)) {
                // mut self 或 mut identifier: Type
                if (self.match(.keyword_self)) {
                    param_name = "self";
                    param_type = ast.Type{ .named = "Self" };
                } else {
                    const pname = try self.consume(.identifier);
                    _ = try self.consume(.colon);
                    param_name = pname.lexeme;
                    param_type = try self.parseType();
                }
            } else if (self.match(.keyword_self)) {
                // self (不可变)
                param_name = "self";
                param_type = ast.Type{ .named = "Self" };
            } else {
                // 普通参数
                const pname = try self.consume(.identifier);
                _ = try self.consume(.colon);
                param_name = pname.lexeme;
                param_type = try self.parseType();
            }
            
            try params.append(ast.Param{
                .name = param_name,
                .type = param_type,
            });
            
            if (!self.match(.comma)) break;
        }
        
        _ = try self.consume(.rparen);
        
        // 解析返回类型
        _ = try self.consume(.arrow);
        const return_type = try self.parseType();
        
        // 解析函数体
        _ = try self.consume(.lbrace);
        const body = try self.parseStmtList();
        _ = try self.consume(.rbrace);

        return ast.FunctionDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(),
            .params = try params.toOwnedSlice(),
            .return_type = return_type,
            .body = body,
            .is_public = is_public,
            .is_async = is_async,
        };
    }

    // 新增：解析 type 统一类型定义
    fn parseTypeDecl(self: *Parser, is_public: bool) !ast.TypeDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8).init(self.allocator);
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }
        
        _ = try self.consume(.assign);
        
        // 判断类型种类（struct, enum, trait 现在是标识符而非关键字）
        const type_kind_tok = try self.consume(.identifier);
        const type_kind = type_kind_tok.lexeme;
        
        var kind: ast.TypeDeclKind = undefined;
        
        if (std.mem.eql(u8, type_kind, "struct")) {
            _ = try self.consume(.lbrace);
            
            var fields = std.ArrayList(ast.StructField).init(self.allocator);
            var methods = std.ArrayList(ast.FunctionDecl).init(self.allocator);
            
            while (!self.check(.rbrace) and !self.isAtEnd()) {
                const field_is_pub = self.match(.keyword_pub);
                
                // 检查是否是方法定义
                if (self.check(.keyword_fn)) {
                    _ = self.advance();
                    const method = try self.parseFunctionDecl(field_is_pub, false);
                    try methods.append(method);
                } else {
                    // 字段定义
                    const field_name = try self.consume(.identifier);
                    _ = try self.consume(.colon);
                    const field_type = try self.parseType();
                    
                    try fields.append(ast.StructField{
                        .name = field_name.lexeme,
                        .type = field_type,
                        .is_public = field_is_pub,
                        .is_mut = false,
                    });
                    
                    _ = self.match(.comma);
                }
            }
            
            _ = try self.consume(.rbrace);
            
            kind = ast.TypeDeclKind{ .struct_type = .{
                .fields = try fields.toOwnedSlice(),
                .methods = try methods.toOwnedSlice(),
            }};
        } else if (std.mem.eql(u8, type_kind, "enum")) {
            _ = try self.consume(.lbrace);
            
            var variants = std.ArrayList(ast.EnumVariant).init(self.allocator);
            var methods = std.ArrayList(ast.FunctionDecl).init(self.allocator);
            
            while (!self.check(.rbrace) and !self.isAtEnd()) {
                const variant_is_pub = self.match(.keyword_pub);
                
                if (self.check(.keyword_fn)) {
                    _ = self.advance();
                    const method = try self.parseFunctionDecl(variant_is_pub, false);
                    try methods.append(method);
                } else {
                    const variant_name = try self.consume(.identifier);
                    
                    var var_fields = std.ArrayList(ast.Type).init(self.allocator);
                    if (self.match(.lparen)) {
                        while (!self.check(.rparen) and !self.isAtEnd()) {
                            const field_type = try self.parseType();
                            try var_fields.append(field_type);
                            if (!self.match(.comma)) break;
                        }
                        _ = try self.consume(.rparen);
                    }
                    
                    try variants.append(ast.EnumVariant{
                        .name = variant_name.lexeme,
                        .fields = try var_fields.toOwnedSlice(),
                    });
                    
                    _ = self.match(.comma);
                }
            }
            
            _ = try self.consume(.rbrace);
            
            kind = ast.TypeDeclKind{ .enum_type = .{
                .variants = try variants.toOwnedSlice(),
                .methods = try methods.toOwnedSlice(),
            }};
        } else if (std.mem.eql(u8, type_kind, "trait")) {
            _ = try self.consume(.lbrace);
            
            var method_sigs = std.ArrayList(ast.FunctionSignature).init(self.allocator);
            while (!self.check(.rbrace) and !self.isAtEnd()) {
                _ = try self.consume(.keyword_fn);
                const method_name = try self.consume(.identifier);
                
                _ = try self.consume(.lparen);
                var params = std.ArrayList(ast.Param).init(self.allocator);
                while (!self.check(.rparen) and !self.isAtEnd()) {
                    // 🆕 支持 self 和 mut self
                    var param_name: []const u8 = undefined;
                    var param_type: ast.Type = undefined;
                    
                    if (self.match(.keyword_mut)) {
                        if (self.match(.keyword_self)) {
                            param_name = "self";
                            param_type = ast.Type{ .named = "Self" };
                        } else {
                            const pname = try self.consume(.identifier);
                            _ = try self.consume(.colon);
                            param_name = pname.lexeme;
                            param_type = try self.parseType();
                        }
                    } else if (self.match(.keyword_self)) {
                        param_name = "self";
                        param_type = ast.Type{ .named = "Self" };
                    } else {
                        const pname = try self.consume(.identifier);
                        _ = try self.consume(.colon);
                        param_name = pname.lexeme;
                        param_type = try self.parseType();
                    }
                    
                    try params.append(ast.Param{
                        .name = param_name,
                        .type = param_type,
                    });
                    
                    if (!self.match(.comma)) break;
                }
                _ = try self.consume(.rparen);
                
                _ = try self.consume(.arrow);
                const return_type = try self.parseType();
                
                try method_sigs.append(ast.FunctionSignature{
                    .name = method_name.lexeme,
                    .params = try params.toOwnedSlice(),
                    .return_type = return_type,
                });
            }
            
            _ = try self.consume(.rbrace);
            
            kind = ast.TypeDeclKind{ .trait_type = .{
                .methods = try method_sigs.toOwnedSlice(),
            }};
        } else {
            return error.ExpectedTypeKind;
        }
        
        return ast.TypeDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(),
            .kind = kind,
            .is_public = is_public,
        };
    }
    
    fn parseStructDecl(self: *Parser, is_public: bool) !ast.StructDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8).init(self.allocator);
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var fields = std.ArrayList(ast.StructField).init(self.allocator);
        var methods = std.ArrayList(ast.FunctionDecl).init(self.allocator);
        
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const field_is_pub = self.match(.keyword_pub);
            
            if (self.check(.keyword_fn)) {
                _ = self.advance();
                const method = try self.parseFunctionDecl(field_is_pub, false);
                try methods.append(method);
            } else {
                const field_name = try self.consume(.identifier);
                _ = try self.consume(.colon);
                const field_type = try self.parseType();
                
                try fields.append(ast.StructField{
                    .name = field_name.lexeme,
                    .type = field_type,
                    .is_public = field_is_pub,
                    .is_mut = false,
                });
                
                _ = self.match(.comma);
            }
        }
        
        _ = try self.consume(.rbrace);

        return ast.StructDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(),
            .fields = try fields.toOwnedSlice(),
            .methods = try methods.toOwnedSlice(),
            .is_public = is_public,
        };
    }

    fn parseEnumDecl(self: *Parser, is_public: bool) !ast.EnumDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8).init(self.allocator);
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var variants = std.ArrayList(ast.EnumVariant).init(self.allocator);
        var methods = std.ArrayList(ast.FunctionDecl).init(self.allocator);
        
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const variant_is_pub = self.match(.keyword_pub);
            
            if (self.check(.keyword_fn)) {
                _ = self.advance();
                const method = try self.parseFunctionDecl(variant_is_pub, false);
                try methods.append(method);
            } else {
                const variant_name = try self.consume(.identifier);
                
                var fields = std.ArrayList(ast.Type).init(self.allocator);
                if (self.match(.lparen)) {
                    while (!self.check(.rparen) and !self.isAtEnd()) {
                        const field_type = try self.parseType();
                        try fields.append(field_type);
                        if (!self.match(.comma)) break;
                    }
                    _ = try self.consume(.rparen);
                }
                
                try variants.append(ast.EnumVariant{
                    .name = variant_name.lexeme,
                    .fields = try fields.toOwnedSlice(),
                });
                
                _ = self.match(.comma);
            }
        }
        
        _ = try self.consume(.rbrace);

        return ast.EnumDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(),
            .variants = try variants.toOwnedSlice(),
            .methods = try methods.toOwnedSlice(),
            .is_public = is_public,
        };
    }

    fn parseTraitDecl(self: *Parser, is_public: bool) !ast.TraitDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8).init(self.allocator);
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var methods = std.ArrayList(ast.FunctionSignature).init(self.allocator);
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            _ = try self.consume(.keyword_fn);
            const method_name = try self.consume(.identifier);
            
            _ = try self.consume(.lparen);
            var params = std.ArrayList(ast.Param).init(self.allocator);
            
            while (!self.check(.rparen) and !self.isAtEnd()) {
                // 🆕 支持 self 和 mut self
                var param_name: []const u8 = undefined;
                var param_type: ast.Type = undefined;
                
                if (self.match(.keyword_mut)) {
                    if (self.match(.keyword_self)) {
                        param_name = "self";
                        param_type = ast.Type{ .named = "Self" };
                    } else {
                        const pname = try self.consume(.identifier);
                        _ = try self.consume(.colon);
                        param_name = pname.lexeme;
                        param_type = try self.parseType();
                    }
                } else if (self.match(.keyword_self)) {
                    param_name = "self";
                    param_type = ast.Type{ .named = "Self" };
                } else {
                    const pname = try self.consume(.identifier);
                    _ = try self.consume(.colon);
                    param_name = pname.lexeme;
                    param_type = try self.parseType();
                }
                
                try params.append(ast.Param{
                    .name = param_name,
                    .type = param_type,
                });
                
                if (!self.match(.comma)) break;
            }
            
            _ = try self.consume(.rparen);
            _ = try self.consume(.arrow);
            const return_type = try self.parseType();
            _ = try self.consume(.semicolon);
            
            try methods.append(ast.FunctionSignature{
                .name = method_name.lexeme,
                .params = try params.toOwnedSlice(),
                .return_type = return_type,
            });
        }
        
        _ = try self.consume(.rbrace);

        return ast.TraitDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(),
            .methods = try methods.toOwnedSlice(),
            .is_public = is_public,
        };
    }

    fn parseImplDecl(self: *Parser) !ast.ImplDecl {
        const trait_name_tok = try self.consume(.identifier);
        
        // 解析类型参数
        var type_args = std.ArrayList(ast.Type).init(self.allocator);
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_arg = try self.parseType();
                try type_args.append(type_arg);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var methods = std.ArrayList(ast.FunctionDecl).init(self.allocator);
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const func = try self.parseFunctionDecl(false, false);
            try methods.append(func);
        }
        
        _ = try self.consume(.rbrace);

        return ast.ImplDecl{
            .trait_name = trait_name_tok.lexeme,
            .type_args = try type_args.toOwnedSlice(),
            .target_type = ast.Type.void, // 简化处�?
            .methods = try methods.toOwnedSlice(),
        };
    }

    fn parseImportDecl(self: *Parser) !ast.ImportDecl {
        const path = try self.consume(.string_literal);
        _ = self.match(.semicolon);
        
        return ast.ImportDecl{
            .path = path.lexeme[1 .. path.lexeme.len - 1], // 去掉引号
        };
    }

    fn parseType(self: *Parser) !ast.Type {
        // 有符号整数类型（8-128位）
        if (self.match(.type_i8)) return ast.Type.i8;
        if (self.match(.type_i16)) return ast.Type.i16;
        if (self.match(.type_i32)) return ast.Type.i32;
        if (self.match(.type_i64)) return ast.Type.i64;
        if (self.match(.type_i128)) return ast.Type.i128;
        
        // 无符号整数类型（8-128位）
        if (self.match(.type_u8)) return ast.Type.u8;
        if (self.match(.type_u16)) return ast.Type.u16;
        if (self.match(.type_u32)) return ast.Type.u32;
        if (self.match(.type_u64)) return ast.Type.u64;
        if (self.match(.type_u128)) return ast.Type.u128;
        
        // 浮点类型
        if (self.match(.type_f32)) return ast.Type.f32;
        if (self.match(.type_f64)) return ast.Type.f64;
        
        // 其他类型
        if (self.match(.type_bool)) return ast.Type.bool;
        if (self.match(.type_char)) return ast.Type.char;
        if (self.match(.type_string)) return ast.Type.string;
        if (self.match(.type_void)) return ast.Type.void;
        
        // 🆕 数组类型 [T] 或 [T; N]
        if (self.match(.lbracket)) {
            const elem_type = try self.parseType();
            
            var size: ?usize = null;
            if (self.match(.semicolon)) {
                // 固定大小数组 [T; N]
                const size_token = try self.consume(.int_literal);
                size = try std.fmt.parseInt(usize, size_token.lexeme, 10);
            }
            
            _ = try self.consume(.rbracket);
            
            const elem_type_ptr = try self.allocator.create(ast.Type);
            elem_type_ptr.* = elem_type;
            
            return ast.Type{
                .array = .{
                    .element = elem_type_ptr,
                    .size = size,
                },
            };
        }
        
        if (self.check(.identifier)) {
            const name = self.advance();
            
            // 检查是否有泛型参数
            if (self.match(.lt)) {
                var type_args = std.ArrayList(ast.Type).init(self.allocator);
                while (!self.check(.gt)) {
                    const type_arg = try self.parseType();
                    try type_args.append(type_arg);
                    if (!self.match(.comma)) break;
                }
                _ = try self.consume(.gt);
                
                const generic_inst = try self.allocator.create(ast.Type);
                generic_inst.* = ast.Type{
                    .generic_instance = .{
                        .name = name.lexeme,
                        .type_args = try type_args.toOwnedSlice(),
                    },
                };
                return generic_inst.*;
            }
            
            return ast.Type{ .named = name.lexeme };
        }
        
        return error.ExpectedType;
    }

    fn parseStmtList(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})![]ast.Stmt {
        var stmts = std.ArrayList(ast.Stmt).init(self.allocator);
        
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const stmt = try self.parseStmt();
            try stmts.append(stmt);
        }
        
        return try stmts.toOwnedSlice();
    }

    // ============================================================================
    // Statement Parsing
    // ============================================================================
    
    fn parseStmt(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Stmt {
        if (self.match(.keyword_let)) {
            return try self.parseLetStmt();
        } else if (self.match(.keyword_return)) {
            return try self.parseReturnStmt();
        } else if (self.match(.keyword_loop)) {
            return try self.parseLoopStmt();
        } else if (self.match(.keyword_break)) {
            _ = self.match(.semicolon);
            return ast.Stmt{ .break_stmt = null };
        } else {
            // 🆕 尝试解析赋值语句或表达式语句
            const expr = try self.parseExpr();
            
            // 检查是否是赋值或复合赋值
            if (self.check(.assign)) {
                _ = self.advance();  // 消费 =
                const value = try self.parseExpr();
                _ = self.match(.semicolon);
                return ast.Stmt{
                    .assign = .{
                        .target = expr,
                        .value = value,
                    },
                };
            } else if (self.check(.add_assign) or self.check(.sub_assign) or 
                       self.check(.mul_assign) or self.check(.div_assign) or 
                       self.check(.mod_assign)) {
                const op_token = self.advance();
                const op = switch (op_token.type) {
                    .add_assign => ast.CompoundAssignOp.add_assign,
                    .sub_assign => ast.CompoundAssignOp.sub_assign,
                    .mul_assign => ast.CompoundAssignOp.mul_assign,
                    .div_assign => ast.CompoundAssignOp.div_assign,
                    .mod_assign => ast.CompoundAssignOp.mod_assign,
                    else => unreachable,
                };
                const value = try self.parseExpr();
                _ = self.match(.semicolon);
                return ast.Stmt{
                    .compound_assign = .{
                        .target = expr,
                        .op = op,
                        .value = value,
                    },
                };
            } else {
                // 只是表达式语句
                _ = self.match(.semicolon);
                return ast.Stmt{ .expr = expr };
            }
        }
    }

    fn parseLetStmt(self: *Parser) !ast.Stmt {
        const is_mut = self.match(.keyword_mut);
        const name = try self.consume(.identifier);
        
        var type_annotation: ?ast.Type = null;
        if (self.match(.colon)) {
            type_annotation = try self.parseType();
        }
        
        var init_expr: ?ast.Expr = null;
        if (self.match(.assign)) {
            init_expr = try self.parseExpr();
        }
        
        _ = self.match(.semicolon);
        
        return ast.Stmt{
            .let_decl = .{
                .name = name.lexeme,
                .is_mut = is_mut,
                .type = type_annotation,
                .init = init_expr,
            },
        };
    }

    fn parseReturnStmt(self: *Parser) !ast.Stmt {
        var value: ?ast.Expr = null;
        
        if (!self.check(.semicolon) and !self.check(.rbrace)) {
            value = try self.parseExpr();
        }
        
        _ = self.match(.semicolon);
        
        return ast.Stmt{ .return_stmt = value };
    }

    // 新增：解析 loop 统一循环语句
    fn parseLoopStmt(self: *Parser) !ast.Stmt {
        // loop { } - 无限循环
        // loop condition { } - 条件循环
        // loop i in iter { } - 遍历循环（🆕 移除 for）
        
        var condition: ?ast.Expr = null;
        var iterator: ?ast.LoopIterator = null;
        
        // 🆕 智能判断：遍历 vs 条件
        if (self.check(.identifier)) {
            // 🆕 前瞻检查：下一个是否是 "in" 关键字
            const binding_token = self.tokens[self.current];
            const next_idx = self.current + 1;
            const is_iterator = next_idx < self.tokens.len and 
                               self.tokens[next_idx].type == .keyword_in;
            
            if (is_iterator) {
                // loop i in collection { } 形式（🆕 移除 for）
                _ = self.advance();  // 消费 identifier (binding)
                _ = try self.consume(.keyword_in);  // 消费 in
                const iterable = try self.parseExpr();
                
                iterator = ast.LoopIterator{
                    .binding = binding_token.lexeme,
                    .iterable = iterable,
                };
            } else {
                // 不是 "in"，说明是条件表达式
                condition = try self.parseExpr();
            }
        } else if (!self.check(.lbrace)) {
            // 🆕 如果不是 { 开头且不是 identifier，也可能是条件表达式
            // 例如：loop (x > 0) { } 或 loop true { }
            condition = try self.parseExpr();
        }
        
        _ = try self.consume(.lbrace);
        const body = try self.parseStmtList();
        _ = try self.consume(.rbrace);
        
        return ast.Stmt{
            .loop_stmt = .{
                .condition = condition,
                .iterator = iterator,
                .body = body,
            },
        };
    }

    // 删除旧的 while/for 语句解析
    fn parseForStmt_DEPRECATED(self: *Parser) !ast.Stmt {
        _ = try self.consume(.lparen);
        
        const init_stmt = if (!self.check(.semicolon)) blk: {
            const stmt = try self.parseStmt();
            const ptr = try self.allocator.create(ast.Stmt);
            ptr.* = stmt;
            break :blk ptr;
        } else null;
        
        _ = try self.consume(.semicolon);
        
        const condition = if (!self.check(.semicolon))
            try self.parseExpr()
        else
            null;
        
        _ = try self.consume(.semicolon);
        
        const step = if (!self.check(.rparen))
            try self.parseExpr()
        else
            null;
        
        _ = try self.consume(.rparen);
        
        _ = try self.consume(.lbrace);
        const body = try self.parseStmtList();
        _ = try self.consume(.rbrace);
        
        return ast.Stmt{
            .for_loop = .{
                .init = init_stmt,
                .condition = condition,
                .step = step,
                .body = body,
            },
        };
    }

    // ============================================================================
    // Expression Parsing
    // ============================================================================
    
    fn parseExpr(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        return try self.parseIs();
    }
    
    // 新增：解析 is 表达式（最低优先级，在逻辑运算之后）
    fn parseIs(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        const expr = try self.parseAs();
        
        // value is { pattern -> result }
        if (self.match(.keyword_is)) {
            _ = try self.consume(.lbrace);
            
            var arms = std.ArrayList(ast.IsArm).init(self.allocator);
            
            while (!self.check(.rbrace) and !self.isAtEnd()) {
                const pattern = try self.parsePattern();
                
                // 可选的 if guard
                var guard: ?ast.Expr = null;
                if (self.match(.keyword_if)) {
                    guard = try self.parseExpr();
                }
                
                _ = try self.consume(.fat_arrow);
                const body = try self.parseExpr();
                
                try arms.append(ast.IsArm{
                    .pattern = pattern,
                    .guard = guard,
                    .body = body,
                });
                
                // 可选的逗号或换行
                _ = self.match(.comma);
            }
            
            _ = try self.consume(.rbrace);
            
            const value_ptr = try self.allocator.create(ast.Expr);
            value_ptr.* = expr;
            
            return ast.Expr{
                .is_expr = .{
                    .value = value_ptr,
                    .arms = try arms.toOwnedSlice(),
                },
            };
        }
        
        return expr;
    }
    
    // 新增：解析 as 类型转换
    fn parseAs(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        const expr = try self.parseLogicalOr();
        
        // value as Type
        if (self.match(.keyword_as)) {
            const target_type = try self.parseType();
            
            const value_ptr = try self.allocator.create(ast.Expr);
            value_ptr.* = expr;
            
            return ast.Expr{
                .as_expr = .{
                    .value = value_ptr,
                    .target_type = target_type,
                },
            };
        }
        
        return expr;
    }

    fn parseLogicalOr(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        var expr = try self.parseLogicalAnd();
        
        while (self.match(.or_or)) {
            const left = try self.allocator.create(ast.Expr);
            left.* = expr;
            const right = try self.allocator.create(ast.Expr);
            right.* = try self.parseLogicalAnd();
            
            expr = ast.Expr{
                .binary = .{
                    .left = left,
                    .op = .or_op,
                    .right = right,
                },
            };
        }
        
        return expr;
    }

    fn parseLogicalAnd(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        var expr = try self.parseEquality();
        
        while (self.match(.and_and)) {
            const left = try self.allocator.create(ast.Expr);
            left.* = expr;
            const right = try self.allocator.create(ast.Expr);
            right.* = try self.parseEquality();
            
            expr = ast.Expr{
                .binary = .{
                    .left = left,
                    .op = .and_op,
                    .right = right,
                },
            };
        }
        
        return expr;
    }

    fn parseEquality(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        var expr = try self.parseComparison();
        
        while (true) {
            const op: ?ast.BinaryOp = if (self.match(.eq))
                .eq
            else if (self.match(.ne))
                .ne
            else
                null;
            
            if (op == null) break;
            
            const left = try self.allocator.create(ast.Expr);
            left.* = expr;
            const right = try self.allocator.create(ast.Expr);
            right.* = try self.parseComparison();
            
            expr = ast.Expr{
                .binary = .{
                    .left = left,
                    .op = op.?,
                    .right = right,
                },
            };
        }
        
        return expr;
    }

    fn parseComparison(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        var expr = try self.parseRange();
        
        while (true) {
            const op: ?ast.BinaryOp = if (self.match(.lt))
                .lt
            else if (self.match(.le))
                .le
            else if (self.match(.gt))
                .gt
            else if (self.match(.ge))
                .ge
            else
                null;
            
            if (op == null) break;
            
            const left = try self.allocator.create(ast.Expr);
            left.* = expr;
            const right = try self.allocator.create(ast.Expr);
            right.* = try self.parseRange();
            
            expr = ast.Expr{
                .binary = .{
                    .left = left,
                    .op = op.?,
                    .right = right,
                },
            };
        }
        
        return expr;
    }
    
    // 🆕 解析范围表达式 (1..10, 1..=10)
    fn parseRange(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        const expr = try self.parseTerm();
        
        // 检查 .. 或 ..=
        if (self.check(.dot_dot) or self.check(.dot_dot_eq)) {
            const inclusive = self.check(.dot_dot_eq);
            _ = self.advance();  // 消费 .. 或 ..=
            
            const start_ptr = try self.allocator.create(ast.Expr);
            start_ptr.* = expr;
            
            const end_ptr = try self.allocator.create(ast.Expr);
            end_ptr.* = try self.parseTerm();
            
            return ast.Expr{
                .range = .{
                    .start = start_ptr,
                    .end = end_ptr,
                    .inclusive = inclusive,
                },
            };
        }
        
        return expr;
    }

    fn parseTerm(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        var expr = try self.parseFactor();
        
        while (true) {
            const op: ?ast.BinaryOp = if (self.match(.plus))
                .add
            else if (self.match(.minus))
                .sub
            else
                null;
            
            if (op == null) break;
            
            const left = try self.allocator.create(ast.Expr);
            left.* = expr;
            const right = try self.allocator.create(ast.Expr);
            right.* = try self.parseFactor();
            
            expr = ast.Expr{
                .binary = .{
                    .left = left,
                    .op = op.?,
                    .right = right,
                },
            };
        }
        
        return expr;
    }

    fn parseFactor(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        var expr = try self.parseUnary();
        
        while (true) {
            const op: ?ast.BinaryOp = if (self.match(.star))
                .mul
            else if (self.match(.slash))
                .div
            else if (self.match(.percent))
                .mod
            else
                null;
            
            if (op == null) break;
            
            const left = try self.allocator.create(ast.Expr);
            left.* = expr;
            const right = try self.allocator.create(ast.Expr);
            right.* = try self.parseUnary();
            
            expr = ast.Expr{
                .binary = .{
                    .left = left,
                    .op = op.?,
                    .right = right,
                },
            };
        }
        
        return expr;
    }

    fn parseUnary(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        if (self.match(.minus)) {
            const operand = try self.allocator.create(ast.Expr);
            operand.* = try self.parseUnary();
            return ast.Expr{
                .unary = .{
                    .op = .neg,
                    .operand = operand,
                },
            };
        }
        
        if (self.match(.bang)) {
            const operand = try self.allocator.create(ast.Expr);
            operand.* = try self.parseUnary();
            return ast.Expr{
                .unary = .{
                    .op = .not,
                    .operand = operand,
                },
            };
        }
        
        return try self.parsePostfix();
    }

    fn parsePostfix(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        var expr = try self.parsePrimary();
        
        while (true) {
            if (self.match(.lparen)) {
                // 函数调用
                var args = std.ArrayList(ast.Expr).init(self.allocator);
                
                while (!self.check(.rparen) and !self.isAtEnd()) {
                    const arg = try self.parseExpr();
                    try args.append(arg);
                    if (!self.match(.comma)) break;
                }
                
                _ = try self.consume(.rparen);
                
                const callee = try self.allocator.create(ast.Expr);
                callee.* = expr;
                
                expr = ast.Expr{
                    .call = .{
                        .callee = callee,
                        .args = try args.toOwnedSlice(),
                        .type_args = &[_]ast.Type{},
                    },
                };
            } else if (self.match(.dot)) {
                // 检查是否是 .await
                if (self.match(.keyword_await)) {
                    const value_ptr = try self.allocator.create(ast.Expr);
                    value_ptr.* = expr;
                    
                    expr = ast.Expr{ .await_expr = value_ptr };
                } else {
                    // 普通字段访问
                    const field = try self.consume(.identifier);
                    const object = try self.allocator.create(ast.Expr);
                    object.* = expr;
                    
                    expr = ast.Expr{
                        .field_access = .{
                            .object = object,
                            .field = field.lexeme,
                        },
                    };
                }
            } else if (self.match(.lbracket)) {
                // 🆕 数组索引 arr[index]
                const index_expr = try self.parseExpr();
                _ = try self.consume(.rbracket);
                
                const array_ptr = try self.allocator.create(ast.Expr);
                array_ptr.* = expr;
                
                const index_ptr = try self.allocator.create(ast.Expr);
                index_ptr.* = index_expr;
                
                expr = ast.Expr{
                    .array_index = .{
                        .array = array_ptr,
                        .index = index_ptr,
                    },
                };
            } else if (self.match(.question)) {
                // 🆕 错误传播 expr?
                const inner_ptr = try self.allocator.create(ast.Expr);
                inner_ptr.* = expr;
                
                expr = ast.Expr{ .try_expr = inner_ptr };
            } else {
                break;
            }
        }
        
        return expr;
    }

    fn parsePrimary(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        if (self.match(.keyword_true)) {
            return ast.Expr{ .bool_literal = true };
        }
        
        if (self.match(.keyword_false)) {
            return ast.Expr{ .bool_literal = false };
        }
        
        if (self.check(.int_literal)) {
            const token = self.advance();
            const value = try std.fmt.parseInt(i64, token.lexeme, 10);
            return ast.Expr{ .int_literal = value };
        }
        
        if (self.check(.float_literal)) {
            const token = self.advance();
            const value = try std.fmt.parseFloat(f64, token.lexeme);
            return ast.Expr{ .float_literal = value };
        }
        
        if (self.check(.string_literal)) {
            const token = self.advance();
            const str_content = token.lexeme[1 .. token.lexeme.len - 1];
            
            // 🆕 检查是否包含插值 $ 或 ${}
            if (self.hasInterpolation(str_content)) {
                return try self.parseStringInterpolation(str_content);
            }
            
            return ast.Expr{ .string_literal = str_content };
        }
        
        if (self.check(.char_literal)) {
            const token = self.advance();
            // 简化处理，只取第一个字�?
            const char_str = token.lexeme[1 .. token.lexeme.len - 1];
            const value: u32 = if (char_str.len > 0) char_str[0] else 0;
            return ast.Expr{ .char_literal = value };
        }
        
        if (self.match(.keyword_if)) {
            return try self.parseIfExpr();
        }
        
        // 🆕 支持 self 关键字作为标识符
        if (self.match(.keyword_self)) {
            return ast.Expr{ .identifier = "self" };
        }
        
        // is 表达式会在 parsePostfix 中作为中缀运算符处理
        // 这里不需要处理
        
        if (self.match(.lbrace)) {
            const stmts = try self.parseStmtList();
            _ = try self.consume(.rbrace);
            return ast.Expr{ .block = stmts };
        }
        
        if (self.match(.lparen)) {
            const expr = try self.parseExpr();
            _ = try self.consume(.rparen);
            return expr;
        }
        
        // 🆕 数组字面量 [1, 2, 3]
        if (self.match(.lbracket)) {
            var elements = std.ArrayList(ast.Expr).init(self.allocator);
            
            while (!self.check(.rbracket) and !self.isAtEnd()) {
                const elem = try self.parseExpr();
                try elements.append(elem);
                if (!self.match(.comma)) break;
            }
            
            _ = try self.consume(.rbracket);
            
            return ast.Expr{
                .array_literal = try elements.toOwnedSlice(),
            };
        }
        
        if (self.check(.identifier)) {
            const name = self.advance();
            
            // 🆕 智能判断：检查是否是泛型调用
            // 只有当 < 后面跟着类型名（identifier 或 type_xxx）时，才认为是泛型
            const is_generic = self.check(.lt) and self.isGenericStart();
            
            if (is_generic and self.match(.lt)) {
                var type_args = std.ArrayList(ast.Type).init(self.allocator);
                while (!self.check(.gt)) {
                    const type_arg = try self.parseType();
                    try type_args.append(type_arg);
                    if (!self.match(.comma)) break;
                }
                _ = try self.consume(.gt);
                
                // 结构体初始化
                if (self.match(.lbrace)) {
                    var fields = std.ArrayList(ast.StructFieldInit).init(self.allocator);
                    
                    while (!self.check(.rbrace) and !self.isAtEnd()) {
                        const field_name = try self.consume(.identifier);
                        _ = try self.consume(.colon);
                        const value = try self.parseExpr();
                        
                        try fields.append(ast.StructFieldInit{
                            .name = field_name.lexeme,
                            .value = value,
                        });
                        
                        if (!self.match(.comma)) break;
                    }
                    
                    _ = try self.consume(.rbrace);
                    
                    return ast.Expr{
                        .struct_init = .{
                            .type_name = name.lexeme,
                            .type_args = try type_args.toOwnedSlice(),
                            .fields = try fields.toOwnedSlice(),
                        },
                    };
                }
            } else if (self.check(.lbrace) and self.isTypeName(name.lexeme)) {
                // 🆕 只有当 identifier 是类型名（首字母大写）时，才解析为 struct 初始化
                // 这样可以避免 `variable { }` 被误判为 struct 初始化
                _ = self.advance();  // 消费 {
                
                // 非泛型结构体初始�?
                var fields = std.ArrayList(ast.StructFieldInit).init(self.allocator);
                
                while (!self.check(.rbrace) and !self.isAtEnd()) {
                    const field_name = try self.consume(.identifier);
                    _ = try self.consume(.colon);
                    const value = try self.parseExpr();
                    
                    try fields.append(ast.StructFieldInit{
                        .name = field_name.lexeme,
                        .value = value,
                    });
                    
                    if (!self.match(.comma)) break;
                }
                
                _ = try self.consume(.rbrace);
                
                return ast.Expr{
                    .struct_init = .{
                        .type_name = name.lexeme,
                        .type_args = &[_]ast.Type{},
                        .fields = try fields.toOwnedSlice(),
                    },
                };
            }
            
            return ast.Expr{ .identifier = name.lexeme };
        }
        
        return error.UnexpectedToken;
    }

    fn parseIfExpr(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        _ = try self.consume(.lparen);
        const condition = try self.parseExpr();
        _ = try self.consume(.rparen);
        
        const then_branch = try self.allocator.create(ast.Expr);
        then_branch.* = try self.parseExpr();
        
        var else_branch: ?*ast.Expr = null;
        if (self.match(.keyword_else)) {
            const eb = try self.allocator.create(ast.Expr);
            eb.* = try self.parseExpr();
            else_branch = eb;
        }
        
        const cond_ptr = try self.allocator.create(ast.Expr);
        cond_ptr.* = condition;
        
        return ast.Expr{
            .if_expr = .{
                .condition = cond_ptr,
                .then_branch = then_branch,
                .else_branch = else_branch,
            },
        };
    }

    fn parseMatchExpr(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        _ = try self.consume(.lparen);
        const value = try self.parseExpr();
        _ = try self.consume(.rparen);
        
        _ = try self.consume(.lbrace);
        
        var arms = std.ArrayList(ast.MatchArm).init(self.allocator);
        
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const pattern = try self.parsePattern();
            _ = try self.consume(.fat_arrow);
            const body = try self.parseExpr();
            
            try arms.append(ast.MatchArm{
                .pattern = pattern,
                .body = body,
            });
            
            _ = self.match(.comma);
        }
        
        _ = try self.consume(.rbrace);
        
        const value_ptr = try self.allocator.create(ast.Expr);
        value_ptr.* = value;
        
        return ast.Expr{
            .match_expr = .{
                .value = value_ptr,
                .arms = try arms.toOwnedSlice(),
            },
        };
    }

    fn parsePattern(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedPattern,ExpectedType,InvalidCharacter,Overflow})!ast.Pattern {
        // 字面量模式
        if (self.check(.int_literal) or self.check(.float_literal) or 
            self.check(.string_literal) or self.check(.char_literal) or
            self.check(.keyword_true) or self.check(.keyword_false)) {
            const literal_expr = try self.parsePrimary();
            return ast.Pattern{ .literal = literal_expr };
        }
        
        // 标识符 或 变体模式 或 通配符
        if (self.check(.identifier)) {
            const name = self.advance();
            
            // 🆕 检查是否是通配符 _
            if (std.mem.eql(u8, name.lexeme, "_")) {
                return ast.Pattern.wildcard;
            }
            
            // 检查是否是变体模式
            if (self.match(.lparen)) {
                var bindings = std.ArrayList([]const u8).init(self.allocator);
                
                while (!self.check(.rparen) and !self.isAtEnd()) {
                    const binding = try self.consume(.identifier);
                    try bindings.append(binding.lexeme);
                    if (!self.match(.comma)) break;
                }
                
                _ = try self.consume(.rparen);
                
                return ast.Pattern{
                    .variant = .{
                        .name = name.lexeme,
                        .bindings = try bindings.toOwnedSlice(),
                    },
                };
            }
            
            return ast.Pattern{ .identifier = name.lexeme };
        }
        
        return error.ExpectedPattern;
    }

    fn match(self: *Parser, token_type: TokenType) bool {
        if (self.check(token_type)) {
            _ = self.advance();
            return true;
        }
        return false;
    }

    // ============================================================================
    // Helper Functions
    // ============================================================================
    
    fn check(self: *Parser, token_type: TokenType) bool {
        if (self.isAtEnd()) return false;
        return self.tokens[self.current].type == token_type;
    }

    fn advance(self: *Parser) Token {
        if (!self.isAtEnd()) {
            self.current += 1;
        }
        return self.tokens[self.current - 1];
    }

    fn isAtEnd(self: *Parser) bool {
        return self.tokens[self.current].type == .eof;
    }
    
    // 🆕 判断 < 后面是否是泛型参数（而不是比较运算符）
    // 策略：使用类型表进行上下文感知判断（100% 准确！）
    fn isGenericStart(self: *Parser) bool {
        if (self.current >= self.tokens.len) return false;
        
        // 跳过当前的 < token，看下一个
        const next_idx = self.current + 1;
        if (next_idx >= self.tokens.len) return false;
        
        const next_token = self.tokens[next_idx];
        
        return switch (next_token.type) {
            // 策略 1: 内置类型关键字 → 100% 确定是泛型
            .type_i8, .type_i16, .type_i32, .type_i64, .type_i128,
            .type_u8, .type_u16, .type_u32, .type_u64, .type_u128,
            .type_f32, .type_f64,
            .type_bool, .type_char, .type_string, .type_void => true,
            
            // 策略 2: 数组类型 [T] → 是泛型
            .lbracket => true,
            
            // 策略 3: identifier → 查询类型表（上下文感知！）
            .identifier => blk: {
                // 🆕 查询第一遍收集的类型表
                if (self.known_types.contains(next_token.lexeme)) {
                    break :blk true;  // 确定是类型 → 是泛型
                }
                
                // 🆕 不在类型表中 → 一定是变量 → 不是泛型
                break :blk false;
            },
            
            // 其他情况不是泛型
            else => false,
        };
    }
    
    // 🆕 判断标识符是否是类型名（首字母大写）
    fn isTypeName(self: *Parser, name: []const u8) bool {
        _ = self;
        if (name.len == 0) return false;
        const first_char = name[0];
        return first_char >= 'A' and first_char <= 'Z';
    }
    
    // 🆕 检查字符串是否包含插值
    fn hasInterpolation(self: *Parser, str: []const u8) bool {
        _ = self;
        for (str, 0..) |c, i| {
            if (c == '$') {
                // 确保不是转义的 \$
                if (i == 0 or str[i - 1] != '\\') {
                    return true;
                }
            }
        }
        return false;
    }
    
    // 🆕 检查字符是否是标识符字符
    fn isIdentifierChar(self: *Parser, c: u8) bool {
        _ = self;
        return (c >= 'a' and c <= 'z') or 
               (c >= 'A' and c <= 'Z') or 
               (c >= '0' and c <= '9') or 
               c == '_';
    }
    
    // 🆕 解析字符串插值
    fn parseStringInterpolation(self: *Parser, str: []const u8) !ast.Expr {
        var parts = std.ArrayList(ast.StringInterpPart).init(self.allocator);
        
        var i: usize = 0;
        var literal_start: usize = 0;
        
        while (i < str.len) {
            if (str[i] == '$') {
                // 添加之前的字面量部分
                if (i > literal_start) {
                    try parts.append(ast.StringInterpPart{
                        .literal = str[literal_start..i],
                    });
                }
                
                i += 1; // 跳过 $
                
                if (i < str.len and str[i] == '{') {
                    // ${expr} 形式
                    i += 1; // 跳过 {
                    const expr_start = i;
                    
                    // 找到匹配的 }
                    var brace_count: i32 = 1;
                    while (i < str.len and brace_count > 0) {
                        if (str[i] == '{') brace_count += 1;
                        if (str[i] == '}') brace_count -= 1;
                        if (brace_count > 0) i += 1;
                    }
                    
                    const expr_str = str[expr_start..i];
                    i += 1; // 跳过 }
                    
                    // 解析表达式（简化：暂时只支持标识符）
                    const expr = ast.Expr{ .identifier = expr_str };
                    try parts.append(ast.StringInterpPart{ .expr = expr });
                    
                    literal_start = i;
                } else {
                    // $var 形式
                    const var_start = i;
                    while (i < str.len and (self.isIdentifierChar(str[i]))) {
                        i += 1;
                    }
                    
                    const var_name = str[var_start..i];
                    const expr = ast.Expr{ .identifier = var_name };
                    try parts.append(ast.StringInterpPart{ .expr = expr });
                    
                    literal_start = i;
                }
            } else {
                i += 1;
            }
        }
        
        // 添加最后的字面量部分
        if (literal_start < str.len) {
            try parts.append(ast.StringInterpPart{
                .literal = str[literal_start..],
            });
        }
        
        return ast.Expr{
            .string_interp = .{
                .parts = try parts.toOwnedSlice(),
            },
        };
    }

    fn consume(self: *Parser, token_type: TokenType) !Token {
        if (self.check(token_type)) {
            return self.advance();
        }
        
        const current_token = self.tokens[self.current];
        std.debug.print("语法错误: 期望 {any}, 但得�?{any} 在第 {d} 行第 {d} 列\n", .{
            token_type,
            current_token.type,
            current_token.line,
            current_token.column,
        });
        
        return error.UnexpectedToken;
    }
};





