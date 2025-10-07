const std = @import("std");
const Token = @import("token.zig").Token;
const TokenType = @import("token.zig").TokenType;

pub const Lexer = struct {
    allocator: std.mem.Allocator,
    source: []const u8,
    tokens: std.ArrayList(Token),
    start: usize,
    current: usize,
    line: usize,
    column: usize,

    pub fn init(allocator: std.mem.Allocator, source: []const u8) Lexer {
        return Lexer{
            .allocator = allocator,
            .source = source,
            .tokens = .{},
            .start = 0,
            .current = 0,
            .line = 1,
            .column = 1,
        };
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

        try self.tokens.append(self.allocator, Token.init(.eof, "", self.line, self.column));
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
            '.' => try self.addToken(.dot),
            '+' => try self.addToken(.plus),
            '-' => {
                if (self.match('>')) {
                    try self.addToken(.arrow);
                } else {
                    try self.addToken(.minus);
                }
            },
            '*' => try self.addToken(.star),
            '%' => try self.addToken(.percent),
            '!' => {
                if (self.match('=')) {
                    try self.addToken(.ne);
                } else {
                    try self.addToken(.bang);
                }
            },
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
                    std.debug.print("错误: 未知字符 '{c}' 在第 {d} 行第 {d} 列\n", .{ c, self.line, self.column });
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
            std.debug.print("错误: 未终止的字符串\n", .{});
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
            std.debug.print("错误: 未终止的字符字面量\n", .{});
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
        if (std.mem.eql(u8, text, "fn")) return .keyword_fn;
        if (std.mem.eql(u8, text, "let")) return .keyword_let;
        if (std.mem.eql(u8, text, "if")) return .keyword_if;
        if (std.mem.eql(u8, text, "else")) return .keyword_else;
        if (std.mem.eql(u8, text, "while")) return .keyword_while;
        if (std.mem.eql(u8, text, "for")) return .keyword_for;
        if (std.mem.eql(u8, text, "match")) return .keyword_match;
        if (std.mem.eql(u8, text, "return")) return .keyword_return;
        if (std.mem.eql(u8, text, "break")) return .keyword_break;
        if (std.mem.eql(u8, text, "continue")) return .keyword_continue;
        if (std.mem.eql(u8, text, "struct")) return .keyword_struct;
        if (std.mem.eql(u8, text, "enum")) return .keyword_enum;
        if (std.mem.eql(u8, text, "trait")) return .keyword_trait;
        if (std.mem.eql(u8, text, "impl")) return .keyword_impl;
        if (std.mem.eql(u8, text, "import")) return .keyword_import;
        if (std.mem.eql(u8, text, "pub")) return .keyword_pub;
        if (std.mem.eql(u8, text, "true")) return .keyword_true;
        if (std.mem.eql(u8, text, "false")) return .keyword_false;
        if (std.mem.eql(u8, text, "Void")) return .type_void;
        if (std.mem.eql(u8, text, "Bool")) return .type_bool;
        if (std.mem.eql(u8, text, "Byte")) return .type_byte;
        if (std.mem.eql(u8, text, "Char")) return .type_char;
        if (std.mem.eql(u8, text, "Int")) return .type_int;
        if (std.mem.eql(u8, text, "Long")) return .type_long;
        if (std.mem.eql(u8, text, "Float")) return .type_float;
        if (std.mem.eql(u8, text, "Double")) return .type_double;
        if (std.mem.eql(u8, text, "String")) return .type_string;
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
        const token = Token.init(token_type, text, self.line, self.column);
        try self.tokens.append(self.allocator, token);
    }
};

