const std = @import("std");
const Token = @import("token.zig").Token;
const TokenType = @import("token.zig").TokenType;
const ast = @import("ast.zig");

pub const Parser = struct {
    allocator: std.mem.Allocator,
    tokens: []Token,
    current: usize,

    pub fn init(allocator: std.mem.Allocator, tokens: []Token) Parser {
        return Parser{
            .allocator = allocator,
            .tokens = tokens,
            .current = 0,
        };
    }

    pub fn deinit(self: *Parser) void {
        _ = self;
    }

    pub fn parse(self: *Parser) !ast.Program {
        var declarations = std.ArrayList(ast.TopLevelDecl){};
        
        while (!self.isAtEnd()) {
            const decl = try self.parseTopLevelDecl();
            try declarations.append(self.allocator, decl);
        }

        return ast.Program{
            .declarations = try declarations.toOwnedSlice(self.allocator),
        };
    }

    fn parseTopLevelDecl(self: *Parser) !ast.TopLevelDecl {
        const is_public = self.match(.keyword_pub);
        
        if (self.match(.keyword_fn)) {
            const func = try self.parseFunctionDecl(is_public);
            return ast.TopLevelDecl{ .function = func };
        } else if (self.match(.keyword_struct)) {
            const struct_decl = try self.parseStructDecl(is_public);
            return ast.TopLevelDecl{ .struct_decl = struct_decl };
        } else if (self.match(.keyword_enum)) {
            const enum_decl = try self.parseEnumDecl(is_public);
            return ast.TopLevelDecl{ .enum_decl = enum_decl };
        } else if (self.match(.keyword_trait)) {
            const trait_decl = try self.parseTraitDecl(is_public);
            return ast.TopLevelDecl{ .trait_decl = trait_decl };
        } else if (self.match(.keyword_impl)) {
            const impl_decl = try self.parseImplDecl();
            return ast.TopLevelDecl{ .impl_decl = impl_decl };
        } else if (self.match(.keyword_import)) {
            const import_decl = try self.parseImportDecl();
            return ast.TopLevelDecl{ .import_decl = import_decl };
        } else {
            return error.UnexpectedToken;
        }
    }

    fn parseFunctionDecl(self: *Parser, is_public: bool) !ast.FunctionDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8){};
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(self.allocator, type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        // 解析参数
        _ = try self.consume(.lparen);
        var params = std.ArrayList(ast.Param){};
        
        while (!self.check(.rparen) and !self.isAtEnd()) {
            const param_name = try self.consume(.identifier);
            _ = try self.consume(.colon);
            const param_type = try self.parseType();
            
            try params.append(self.allocator, ast.Param{
                .name = param_name.lexeme,
                .type = param_type,
            });
            
            if (!self.match(.comma)) break;
        }
        
        _ = try self.consume(.rparen);
        
        // 解析返回类型
        _ = try self.consume(.arrow);
        const return_type = try self.parseType();
        
        // 解析函数�?
        _ = try self.consume(.lbrace);
        const body = try self.parseStmtList();
        _ = try self.consume(.rbrace);

        return ast.FunctionDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(self.allocator),
            .params = try params.toOwnedSlice(self.allocator),
            .return_type = return_type,
            .body = body,
            .is_public = is_public,
        };
    }

    fn parseStructDecl(self: *Parser, is_public: bool) !ast.StructDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8){};
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(self.allocator, type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var fields = std.ArrayList(ast.StructField){};
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const field_name = try self.consume(.identifier);
            _ = try self.consume(.colon);
            const field_type = try self.parseType();
            
            try fields.append(self.allocator, ast.StructField{
                .name = field_name.lexeme,
                .type = field_type,
            });
            
            _ = self.match(.comma);
        }
        
        _ = try self.consume(.rbrace);

        return ast.StructDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(self.allocator),
            .fields = try fields.toOwnedSlice(self.allocator),
            .is_public = is_public,
        };
    }

    fn parseEnumDecl(self: *Parser, is_public: bool) !ast.EnumDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8){};
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(self.allocator, type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var variants = std.ArrayList(ast.EnumVariant){};
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const variant_name = try self.consume(.identifier);
            
            var fields = std.ArrayList(ast.Type){};
            if (self.match(.lparen)) {
                while (!self.check(.rparen) and !self.isAtEnd()) {
                    const field_type = try self.parseType();
                    try fields.append(self.allocator, field_type);
                    if (!self.match(.comma)) break;
                }
                _ = try self.consume(.rparen);
            }
            
            try variants.append(self.allocator, ast.EnumVariant{
                .name = variant_name.lexeme,
                .fields = try fields.toOwnedSlice(self.allocator),
            });
            
            _ = self.match(.comma);
        }
        
        _ = try self.consume(.rbrace);

        return ast.EnumDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(self.allocator),
            .variants = try variants.toOwnedSlice(self.allocator),
            .is_public = is_public,
        };
    }

    fn parseTraitDecl(self: *Parser, is_public: bool) !ast.TraitDecl {
        const name = try self.consume(.identifier);
        
        // 解析泛型参数
        var type_params = std.ArrayList([]const u8){};
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_param = try self.consume(.identifier);
                try type_params.append(self.allocator, type_param.lexeme);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var methods = std.ArrayList(ast.FunctionSignature){};
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            _ = try self.consume(.keyword_fn);
            const method_name = try self.consume(.identifier);
            
            _ = try self.consume(.lparen);
            var params = std.ArrayList(ast.Param){};
            
            while (!self.check(.rparen) and !self.isAtEnd()) {
                const param_name = try self.consume(.identifier);
                _ = try self.consume(.colon);
                const param_type = try self.parseType();
                
                try params.append(self.allocator, ast.Param{
                    .name = param_name.lexeme,
                    .type = param_type,
                });
                
                if (!self.match(.comma)) break;
            }
            
            _ = try self.consume(.rparen);
            _ = try self.consume(.arrow);
            const return_type = try self.parseType();
            _ = try self.consume(.semicolon);
            
            try methods.append(self.allocator, ast.FunctionSignature{
                .name = method_name.lexeme,
                .params = try params.toOwnedSlice(self.allocator),
                .return_type = return_type,
            });
        }
        
        _ = try self.consume(.rbrace);

        return ast.TraitDecl{
            .name = name.lexeme,
            .type_params = try type_params.toOwnedSlice(self.allocator),
            .methods = try methods.toOwnedSlice(self.allocator),
            .is_public = is_public,
        };
    }

    fn parseImplDecl(self: *Parser) !ast.ImplDecl {
        const trait_name_tok = try self.consume(.identifier);
        
        // 解析类型参数
        var type_args = std.ArrayList(ast.Type){};
        if (self.match(.lt)) {
            while (!self.check(.gt)) {
                const type_arg = try self.parseType();
                try type_args.append(self.allocator, type_arg);
                if (!self.match(.comma)) break;
            }
            _ = try self.consume(.gt);
        }

        _ = try self.consume(.lbrace);
        
        var methods = std.ArrayList(ast.FunctionDecl){};
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const func = try self.parseFunctionDecl(false);
            try methods.append(self.allocator, func);
        }
        
        _ = try self.consume(.rbrace);

        return ast.ImplDecl{
            .trait_name = trait_name_tok.lexeme,
            .type_args = try type_args.toOwnedSlice(self.allocator),
            .target_type = ast.Type.void, // 简化处�?
            .methods = try methods.toOwnedSlice(self.allocator),
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
        if (self.match(.type_void)) return ast.Type.void;
        if (self.match(.type_bool)) return ast.Type.bool;
        if (self.match(.type_byte)) return ast.Type.byte;
        if (self.match(.type_char)) return ast.Type.char;
        if (self.match(.type_int)) return ast.Type.int;
        if (self.match(.type_long)) return ast.Type.long;
        if (self.match(.type_float)) return ast.Type.float;
        if (self.match(.type_double)) return ast.Type.double;
        if (self.match(.type_string)) return ast.Type.string;
        
        if (self.check(.identifier)) {
            const name = self.advance();
            
            // 检查是否有泛型参数
            if (self.match(.lt)) {
                var type_args = std.ArrayList(ast.Type){};
                while (!self.check(.gt)) {
                    const type_arg = try self.parseType();
                    try type_args.append(self.allocator, type_arg);
                    if (!self.match(.comma)) break;
                }
                _ = try self.consume(.gt);
                
                const generic_inst = try self.allocator.create(ast.Type);
                generic_inst.* = ast.Type{
                    .generic_instance = .{
                        .name = name.lexeme,
                        .type_args = try type_args.toOwnedSlice(self.allocator),
                    },
                };
                return generic_inst.*;
            }
            
            return ast.Type{ .named = name.lexeme };
        }
        
        return error.ExpectedType;
    }

    fn parseStmtList(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})![]ast.Stmt {
        var stmts = std.ArrayList(ast.Stmt){};
        
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const stmt = try self.parseStmt();
            try stmts.append(self.allocator, stmt);
        }
        
        return try stmts.toOwnedSlice(self.allocator);
    }

    fn parseStmt(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Stmt {
        if (self.match(.keyword_let)) {
            return try self.parseLetStmt();
        } else if (self.match(.keyword_return)) {
            return try self.parseReturnStmt();
        } else if (self.match(.keyword_while)) {
            return try self.parseWhileStmt();
        } else if (self.match(.keyword_for)) {
            return try self.parseForStmt();
        } else if (self.match(.keyword_break)) {
            _ = self.match(.semicolon);
            return ast.Stmt.break_stmt;
        } else if (self.match(.keyword_continue)) {
            _ = self.match(.semicolon);
            return ast.Stmt.continue_stmt;
        } else {
            const expr = try self.parseExpr();
            _ = self.match(.semicolon);
            return ast.Stmt{ .expr = expr };
        }
    }

    fn parseLetStmt(self: *Parser) !ast.Stmt {
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

    fn parseWhileStmt(self: *Parser) !ast.Stmt {
        _ = try self.consume(.lparen);
        const condition = try self.parseExpr();
        _ = try self.consume(.rparen);
        
        _ = try self.consume(.lbrace);
        const body = try self.parseStmtList();
        _ = try self.consume(.rbrace);
        
        return ast.Stmt{
            .while_loop = .{
                .condition = condition,
                .body = body,
            },
        };
    }

    fn parseForStmt(self: *Parser) !ast.Stmt {
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

    fn parseExpr(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedType,ExpectedPattern,InvalidCharacter,Overflow})!ast.Expr {
        return try self.parseLogicalOr();
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
        var expr = try self.parseTerm();
        
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
            right.* = try self.parseTerm();
            
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
                var args = std.ArrayList(ast.Expr){};
                
                while (!self.check(.rparen) and !self.isAtEnd()) {
                    const arg = try self.parseExpr();
                    try args.append(self.allocator, arg);
                    if (!self.match(.comma)) break;
                }
                
                _ = try self.consume(.rparen);
                
                const callee = try self.allocator.create(ast.Expr);
                callee.* = expr;
                
                expr = ast.Expr{
                    .call = .{
                        .callee = callee,
                        .args = try args.toOwnedSlice(self.allocator),
                        .type_args = &[_]ast.Type{},
                    },
                };
            } else if (self.match(.dot)) {
                const field = try self.consume(.identifier);
                const object = try self.allocator.create(ast.Expr);
                object.* = expr;
                
                expr = ast.Expr{
                    .field_access = .{
                        .object = object,
                        .field = field.lexeme,
                    },
                };
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
            return ast.Expr{ .string_literal = token.lexeme[1 .. token.lexeme.len - 1] };
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
        
        if (self.match(.keyword_match)) {
            return try self.parseMatchExpr();
        }
        
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
        
        if (self.check(.identifier)) {
            const name = self.advance();
            
            // 检查是否是泛型调用或结构体初始�?
            if (self.match(.lt)) {
                var type_args = std.ArrayList(ast.Type){};
                while (!self.check(.gt)) {
                    const type_arg = try self.parseType();
                    try type_args.append(self.allocator, type_arg);
                    if (!self.match(.comma)) break;
                }
                _ = try self.consume(.gt);
                
                // 结构体初始化
                if (self.match(.lbrace)) {
                    var fields = std.ArrayList(ast.StructFieldInit){};
                    
                    while (!self.check(.rbrace) and !self.isAtEnd()) {
                        const field_name = try self.consume(.identifier);
                        _ = try self.consume(.colon);
                        const value = try self.parseExpr();
                        
                        try fields.append(self.allocator, ast.StructFieldInit{
                            .name = field_name.lexeme,
                            .value = value,
                        });
                        
                        if (!self.match(.comma)) break;
                    }
                    
                    _ = try self.consume(.rbrace);
                    
                    return ast.Expr{
                        .struct_init = .{
                            .type_name = name.lexeme,
                            .type_args = try type_args.toOwnedSlice(self.allocator),
                            .fields = try fields.toOwnedSlice(self.allocator),
                        },
                    };
                }
            } else if (self.match(.lbrace)) {
                // 非泛型结构体初始�?
                var fields = std.ArrayList(ast.StructFieldInit){};
                
                while (!self.check(.rbrace) and !self.isAtEnd()) {
                    const field_name = try self.consume(.identifier);
                    _ = try self.consume(.colon);
                    const value = try self.parseExpr();
                    
                    try fields.append(self.allocator, ast.StructFieldInit{
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
                        .fields = try fields.toOwnedSlice(self.allocator),
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
        
        var arms = std.ArrayList(ast.MatchArm){};
        
        while (!self.check(.rbrace) and !self.isAtEnd()) {
            const pattern = try self.parsePattern();
            _ = try self.consume(.fat_arrow);
            const body = try self.parseExpr();
            
            try arms.append(self.allocator, ast.MatchArm{
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
                .arms = try arms.toOwnedSlice(self.allocator),
            },
        };
    }

    fn parsePattern(self: *Parser) (std.mem.Allocator.Error || error{UnexpectedToken,ExpectedPattern})!ast.Pattern {
        if (self.check(.identifier)) {
            const name = self.advance();
            
            // 检查是否是变体模式
            if (self.match(.lparen)) {
                var bindings = std.ArrayList([]const u8){};
                
                while (!self.check(.rparen) and !self.isAtEnd()) {
                    const binding = try self.consume(.identifier);
                    try bindings.append(self.allocator, binding.lexeme);
                    if (!self.match(.comma)) break;
                }
                
                _ = try self.consume(.rparen);
                
                return ast.Pattern{
                    .variant = .{
                        .name = name.lexeme,
                        .bindings = try bindings.toOwnedSlice(self.allocator),
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





