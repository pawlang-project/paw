const std = @import("std");
const Token = @import("token.zig").Token;
const TokenType = @import("token.zig").TokenType;

pub const Lexer = struct {
    allocator: std.mem.Allocator,
    source: []const u8,
    filename: []const u8,  // 🆕 v0.1.8: 文件名
    tokens: std.ArrayList(Token),
    start: usize,
    current: usize,
    line: usize,
    column: usize,
    line_offset: usize,  // 🆕 v0.1.8: 行号偏移（用于处理 prelude）

    pub fn init(allocator: std.mem.Allocator, source: []const u8, filename: []const u8) Lexer {
        var tokens: std.ArrayList(Token) = .{};
        // 🚀 Performance: Pre-allocate token array (estimate ~1 token per 10 chars)
        const estimated_tokens = source.len / 10 + 100;
        tokens.ensureTotalCapacity(allocator, estimated_tokens) catch {};
        
        return Lexer{
            .allocator = allocator,
            .source = source,
            .filename = filename,  // 🆕 v0.1.8
            .tokens = tokens,
            .start = 0,
            .current = 0,
            .line = 1,
            .column = 1,
            .line_offset = 0,  // 🆕 v0.1.8: 默认无偏移
        };
    }
    
    /// 🆕 v0.1.8: 设置行号偏移（用于处理 prelude）
    pub fn setLineOffset(self: *Lexer, offset: usize) void {
        self.line_offset = offset;
    }

    pub fn deinit(self: *Lexer) void {
        self.tokens.deinit(self.allocator);
    }

    pub fn tokenize(self: *Lexer) ![]Token {
        // 清空现有tokens
        self.tokens.clearRetainingCapacity();
        
        while (!self.isAtEnd()) {
            self.start = self.current;
            try self.scanToken();
        }

        try self.tokens.append(self.allocator, Token.init(.eof, "", self.line, self.column, self.filename));
        return self.tokens.items;
    }

    fn scanToken(self: *Lexer) !void {
        const c = self.advance();
        switch (c) {
            ' ', '\r', '\t' => {}, // 忽略空白
            '\n' => {
                self.line += 1;
                self.column = 1;
            },
            '(' => try self.addToken(.lparen),
            ')' => try self.addToken(.rparen),
            '{' => try self.addToken(.lbrace),
            '}' => try self.addToken(.rbrace),
            '[' => try self.addToken(.lbracket),
            ']' => try self.addToken(.rbracket),
            ',' => try self.addToken(.comma),
            ';' => try self.addToken(.semicolon),
            '.' => {
                // 🆕 检查 .. 和 ..=
                if (self.match('.')) {
                    if (self.match('=')) {
                        try self.addToken(.dot_dot_eq);  // ..=
                    } else {
                        try self.addToken(.dot_dot);     // ..
                    }
                } else {
                    try self.addToken(.dot);             // .
                }
            },
            '+' => {
                if (self.match('=')) {
                    try self.addToken(.add_assign);
                } else {
                    try self.addToken(.plus);
                }
            },
            '-' => {
                if (self.match('>')) {
                    try self.addToken(.arrow);
                } else if (self.match('=')) {
                    try self.addToken(.sub_assign);
                } else {
                    try self.addToken(.minus);
                }
            },
            '*' => {
                if (self.match('=')) {
                    try self.addToken(.mul_assign);
                } else {
                    try self.addToken(.star);
                }
            },
            '%' => {
                if (self.match('=')) {
                    try self.addToken(.mod_assign);
                } else {
                    try self.addToken(.percent);
                }
            },
            '!' => {
                if (self.match('=')) {
                    try self.addToken(.ne);
                } else {
                    try self.addToken(.bang);
                }
            },
            '?' => try self.addToken(.question),
            '=' => {
                if (self.match('=')) {
                    try self.addToken(.eq);
                } else if (self.match('>')) {
                    try self.addToken(.fat_arrow);
                } else {
                    try self.addToken(.assign);
                }
            },
            '<' => {
                if (self.match('=')) {
                    try self.addToken(.le);
                } else {
                    try self.addToken(.lt);
                }
            },
            '>' => {
                if (self.match('=')) {
                    try self.addToken(.ge);
                } else {
                    try self.addToken(.gt);
                }
            },
            '&' => {
                if (self.match('&')) {
                    try self.addToken(.and_and);
                }
            },
            '|' => {
                if (self.match('|')) {
                    try self.addToken(.or_or);
                }
            },
            ':' => {
                if (self.match(':')) {
                    try self.addToken(.double_colon);
                } else {
                    try self.addToken(.colon);
                }
            },
            '/' => {
                if (self.match('/')) {
                    // 单行注释
                    while (self.peek() != '\n' and !self.isAtEnd()) {
                        _ = self.advance();
                    }
                } else if (self.match('*')) {
                    // 多行注释
                    try self.blockComment();
                } else if (self.match('=')) {
                    try self.addToken(.div_assign);
                } else {
                    try self.addToken(.slash);
                }
            },
            '"' => try self.string(),
            '\'' => try self.char(),
            else => {
                if (isDigit(c)) {
                    try self.number();
                } else if (isAlpha(c)) {
                    try self.identifier();
                } else {
                    std.debug.print("Error: Unknown character '{c}' at line {d} column {d}\n", .{ c, self.line, self.column });
                }
            },
        }
    }

    fn blockComment(self: *Lexer) !void {
        var depth: usize = 1;
        while (depth > 0 and !self.isAtEnd()) {
            if (self.peek() == '/' and self.peekNext() == '*') {
                _ = self.advance();
                _ = self.advance();
                depth += 1;
            } else if (self.peek() == '*' and self.peekNext() == '/') {
                _ = self.advance();
                _ = self.advance();
                depth -= 1;
            } else {
                if (self.peek() == '\n') {
                    self.line += 1;
                    self.column = 1;
                }
                _ = self.advance();
            }
        }
    }

    fn string(self: *Lexer) !void {
        while (self.peek() != '"' and !self.isAtEnd()) {
            if (self.peek() == '\n') {
                self.line += 1;
                self.column = 1;
            }
            _ = self.advance();
        }

        if (self.isAtEnd()) {
            std.debug.print("Error: Unterminated string\n", .{});
            return;
        }

        _ = self.advance(); // 消耗结束的 "
        try self.addToken(.string_literal);
    }

    fn char(self: *Lexer) !void {
        while (self.peek() != '\'' and !self.isAtEnd()) {
            if (self.peek() == '\n') {
                self.line += 1;
                self.column = 1;
            }
            _ = self.advance();
        }

        if (self.isAtEnd()) {
            std.debug.print("Error: Unterminated character literal\n", .{});
            return;
        }

        _ = self.advance(); // 消耗结束的 '
        try self.addToken(.char_literal);
    }

    fn number(self: *Lexer) !void {
        while (isDigit(self.peek())) {
            _ = self.advance();
        }

        // 查找小数部分
        if (self.peek() == '.' and isDigit(self.peekNext())) {
            _ = self.advance(); // 消耗 '.'

            while (isDigit(self.peek())) {
                _ = self.advance();
            }

            try self.addToken(.float_literal);
        } else {
            try self.addToken(.int_literal);
        }
    }

    fn identifier(self: *Lexer) !void {
        while (isAlphaNumeric(self.peek())) {
            _ = self.advance();
        }

        const text = self.source[self.start..self.current];
        const token_type = getKeywordType(text) orelse .identifier;
        try self.addToken(token_type);
    }
    
    fn getKeywordType(text: []const u8) ?TokenType {
        // Paw 核心关键字 (19个) - 极简设计
        if (std.mem.eql(u8, text, "fn")) return .keyword_fn;
        if (std.mem.eql(u8, text, "let")) return .keyword_let;
        if (std.mem.eql(u8, text, "type")) return .keyword_type;
        if (std.mem.eql(u8, text, "import")) return .keyword_import;
        if (std.mem.eql(u8, text, "pub")) return .keyword_pub;
        if (std.mem.eql(u8, text, "if")) return .keyword_if;
        if (std.mem.eql(u8, text, "else")) return .keyword_else;
        if (std.mem.eql(u8, text, "loop")) return .keyword_loop;
        if (std.mem.eql(u8, text, "break")) return .keyword_break;
        if (std.mem.eql(u8, text, "return")) return .keyword_return;
        if (std.mem.eql(u8, text, "is")) return .keyword_is;
        if (std.mem.eql(u8, text, "as")) return .keyword_as;
        if (std.mem.eql(u8, text, "async")) return .keyword_async;
        if (std.mem.eql(u8, text, "await")) return .keyword_await;
        if (std.mem.eql(u8, text, "self")) return .keyword_self;
        if (std.mem.eql(u8, text, "Self")) return .keyword_Self;
        if (std.mem.eql(u8, text, "mut")) return .keyword_mut;
        if (std.mem.eql(u8, text, "true")) return .keyword_true;
        if (std.mem.eql(u8, text, "false")) return .keyword_false;
        if (std.mem.eql(u8, text, "in")) return .keyword_in;
        
        // 内置类型（Rust 风格，纯粹无别名）
        // 有符号整数
        if (std.mem.eql(u8, text, "i8")) return .type_i8;
        if (std.mem.eql(u8, text, "i16")) return .type_i16;
        if (std.mem.eql(u8, text, "i32")) return .type_i32;
        if (std.mem.eql(u8, text, "i64")) return .type_i64;
        if (std.mem.eql(u8, text, "i128")) return .type_i128;
        
        // 无符号整数
        if (std.mem.eql(u8, text, "u8")) return .type_u8;
        if (std.mem.eql(u8, text, "u16")) return .type_u16;
        if (std.mem.eql(u8, text, "u32")) return .type_u32;
        if (std.mem.eql(u8, text, "u64")) return .type_u64;
        if (std.mem.eql(u8, text, "u128")) return .type_u128;
        
        // 浮点类型
        if (std.mem.eql(u8, text, "f32")) return .type_f32;
        if (std.mem.eql(u8, text, "f64")) return .type_f64;
        
        // 其他类型
        if (std.mem.eql(u8, text, "bool")) return .type_bool;
        if (std.mem.eql(u8, text, "char")) return .type_char;
        if (std.mem.eql(u8, text, "string")) return .type_string;
        if (std.mem.eql(u8, text, "void")) return .type_void;
        
        return null;
    }

    fn isDigit(c: u8) bool {
        return c >= '0' and c <= '9';
    }

    fn isAlpha(c: u8) bool {
        return (c >= 'a' and c <= 'z') or
            (c >= 'A' and c <= 'Z') or
            c == '_';
    }

    fn isAlphaNumeric(c: u8) bool {
        return isAlpha(c) or isDigit(c);
    }

    fn isAtEnd(self: *Lexer) bool {
        return self.current >= self.source.len;
    }

    fn advance(self: *Lexer) u8 {
        const c = self.source[self.current];
        self.current += 1;
        self.column += 1;
        return c;
    }

    fn match(self: *Lexer, expected: u8) bool {
        if (self.isAtEnd()) return false;
        if (self.source[self.current] != expected) return false;

        self.current += 1;
        self.column += 1;
        return true;
    }

    fn peek(self: *Lexer) u8 {
        if (self.isAtEnd()) return 0;
        return self.source[self.current];
    }

    fn peekNext(self: *Lexer) u8 {
        if (self.current + 1 >= self.source.len) return 0;
        return self.source[self.current + 1];
    }

    fn addToken(self: *Lexer, token_type: TokenType) !void {
        const text = self.source[self.start..self.current];
        // 🆕 v0.1.8: 调整行号（减去 prelude 偏移）
        const adjusted_line = if (self.line > self.line_offset) 
            self.line - self.line_offset 
        else 
            self.line;
        const token = Token.init(token_type, text, adjusted_line, self.column, self.filename);
        try self.tokens.append(self.allocator, token);
    }
};

