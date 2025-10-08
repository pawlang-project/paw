pub const TokenType = enum {
    // Paw 核心关键字 (19个) - 极简设计
    keyword_fn,       // 函数定义
    keyword_let,      // 变量声明
    keyword_type,     // 类型定义（统一 struct/enum/trait）
    keyword_import,   // 导入
    keyword_pub,      // 可见性
    keyword_if,       // 条件
    keyword_else,     // 条件分支
    keyword_loop,     // 循环（统一 while/for）
    keyword_break,    // 中断
    keyword_return,   // 返回
    keyword_is,       // 模式匹配（统一 match）
    keyword_as,       // 类型转换
    keyword_async,    // 异步
    keyword_await,    // 等待
    keyword_self,     // 实例引用
    keyword_Self,     // 类型引用
    keyword_mut,      // 可变性
    keyword_true,     // 布尔真
    keyword_false,    // 布尔假
    keyword_in,       // 上下文关键字（用于 loop for）

    // 内置类型（Rust 风格，无别名）
    // 整数类型（有符号）
    type_i8,      // 8位有符号整数
    type_i16,     // 16位有符号整数
    type_i32,     // 32位有符号整数（默认）
    type_i64,     // 64位有符号整数
    type_i128,    // 128位有符号整数
    
    // 整数类型（无符号）
    type_u8,      // 8位无符号整数
    type_u16,     // 16位无符号整数
    type_u32,     // 32位无符号整数
    type_u64,     // 64位无符号整数
    type_u128,    // 128位无符号整数
    
    // 浮点类型
    type_f32,     // 32位浮点数
    type_f64,     // 64位浮点数（默认）
    
    // 其他类型
    type_bool,    // 布尔类型
    type_char,    // 字符类型
    type_string,  // 字符串类型
    type_void,    // 空类型

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
    question,    // ? (错误传播)
    
    assign,      // =
    // 🆕 复合赋值操作符
    add_assign,  // +=
    sub_assign,  // -=
    mul_assign,  // *=
    div_assign,  // /=
    mod_assign,  // %=
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
    dot_dot,     // .. (范围，不包含结束)
    dot_dot_eq,  // ..= (范围，包含结束)
    
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

