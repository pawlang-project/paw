pub const TokenType = enum {
    // 关键字
    keyword_fn,
    keyword_let,
    keyword_if,
    keyword_else,
    keyword_while,
    keyword_for,
    keyword_match,
    keyword_return,
    keyword_break,
    keyword_continue,
    keyword_struct,
    keyword_enum,
    keyword_trait,
    keyword_impl,
    keyword_import,
    keyword_pub,
    keyword_true,
    keyword_false,

    // 类型关键字
    type_void,
    type_bool,
    type_byte,
    type_char,
    type_int,
    type_long,
    type_float,
    type_double,
    type_string,

    // 标识符和字面量
    identifier,
    int_literal,
    float_literal,
    string_literal,
    char_literal,

    // 运算符
    plus,        // +
    minus,       // -
    star,        // *
    slash,       // /
    percent,     // %
    
    eq,          // ==
    ne,          // !=
    lt,          // <
    le,          // <=
    gt,          // >
    ge,          // >=
    
    and_and,     // &&
    or_or,       // ||
    bang,        // !
    
    assign,      // =
    arrow,       // ->
    fat_arrow,   // =>
    
    // 分隔符
    lparen,      // (
    rparen,      // )
    lbrace,      // {
    rbrace,      // }
    lbracket,    // [
    rbracket,    // ]
    
    comma,       // ,
    semicolon,   // ;
    colon,       // :
    double_colon, // ::
    dot,         // .
    
    // 特殊
    eof,
    invalid,
};

pub const Token = struct {
    type: TokenType,
    lexeme: []const u8,
    line: usize,
    column: usize,

    pub fn init(token_type: TokenType, lexeme: []const u8, line: usize, column: usize) Token {
        return Token{
            .type = token_type,
            .lexeme = lexeme,
            .line = line,
            .column = column,
        };
    }
};

