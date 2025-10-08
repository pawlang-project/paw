const std = @import("std");

pub const Type = union(enum) {
    // 有符号整数类型（支持到 128 位）
    i8,
    i16,
    i32,
    i64,
    i128,
    
    // 无符号整数类型（支持到 128 位）
    u8,
    u16,
    u32,
    u64,
    u128,
    
    // 浮点类型
    f32,
    f64,
    
    // 其他基础类型
    bool,
    char,
    string,
    void,
    
    // 复杂类型
    generic: []const u8, // T
    named: []const u8, // 用户定义类型
    pointer: *Type,
    array: struct {
        element: *Type,
        size: ?usize,
    },
    function: struct {
        params: []Type,
        return_type: *Type,
    },
    generic_instance: struct {
        name: []const u8,
        type_args: []Type,
    },

    pub fn eql(self: Type, other: Type) bool {
        if (@intFromEnum(self) != @intFromEnum(other)) return false;
        return switch (self) {
            // 基础类型直接比较
            .i8, .i16, .i32, .i64, .i128,
            .u8, .u16, .u32, .u64, .u128,
            .f32, .f64,
            .bool, .char, .string, .void => true,
            .generic => |name| std.mem.eql(u8, name, other.generic),
            .named => |name| std.mem.eql(u8, name, other.named),
            .pointer => |ptr| ptr.eql(other.pointer.*),
            .array => |arr| {
                // 🆕 改进数组类型比较
                // [T] (无大小) 可以匹配任何 [T; N]
                // [T; N] 只能匹配相同大小的 [T; M] 其中 N == M
                
                // 元素类型必须相同
                if (!arr.element.eql(other.array.element.*)) return false;
                
                // 如果两边都指定了大小，必须相等
                if (arr.size != null and other.array.size != null) {
                    return arr.size.? == other.array.size.?;
                }
                
                // 如果任意一边是动态大小，则兼容
                return true;
            },
            .function => |func| {
                if (!func.return_type.eql(other.function.return_type.*)) return false;
                if (func.params.len != other.function.params.len) return false;
                for (func.params, other.function.params) |p1, p2| {
                    if (!p1.eql(p2)) return false;
                }
                return true;
            },
            .generic_instance => |gi| {
                if (!std.mem.eql(u8, gi.name, other.generic_instance.name)) return false;
                if (gi.type_args.len != other.generic_instance.type_args.len) return false;
                for (gi.type_args, other.generic_instance.type_args) |t1, t2| {
                    if (!t1.eql(t2)) return false;
                }
                return true;
            },
        };
    }
};

pub const Expr = union(enum) {
    int_literal: i64,
    float_literal: f64,
    string_literal: []const u8,
    char_literal: u32,
    bool_literal: bool,
    identifier: []const u8,
    binary: struct {
        left: *Expr,
        op: BinaryOp,
        right: *Expr,
    },
    unary: struct {
        op: UnaryOp,
        operand: *Expr,
    },
    call: struct {
        callee: *Expr,
        args: []Expr,
        type_args: []Type,
    },
    field_access: struct {
        object: *Expr,
        field: []const u8,
    },
    struct_init: struct {
        type_name: []const u8,
        type_args: []Type,
        fields: []StructFieldInit,
    },
    enum_variant: struct {
        enum_name: []const u8,
        variant: []const u8,
        args: []Expr,
    },
    block: []Stmt,
    if_expr: struct {
        condition: *Expr,
        then_branch: *Expr,
        else_branch: ?*Expr,
    },
    // 新增：is 表达式（模式匹配）
    is_expr: struct {
        value: *Expr,
        arms: []IsArm,
    },
    // 向后兼容：match 表达式
    match_expr: struct {
        value: *Expr,
        arms: []MatchArm,
    },
    // 新增：as 表达式（类型转换）
    as_expr: struct {
        value: *Expr,
        target_type: Type,
    },
    // 新增：await 表达式
    await_expr: *Expr,
    // 🆕 数组字面量
    array_literal: []Expr,
    // 🆕 数组索引
    array_index: struct {
        array: *Expr,
        index: *Expr,
    },
    // 🆕 范围表达式
    range: struct {
        start: *Expr,
        end: *Expr,
        inclusive: bool,  // true = ..=, false = ..
    },
};

pub const BinaryOp = enum {
    add,
    sub,
    mul,
    div,
    mod,
    eq,
    ne,
    lt,
    le,
    gt,
    ge,
    and_op,
    or_op,
};

pub const UnaryOp = enum {
    neg,
    not,
};

// 🆕 复合赋值操作符
pub const CompoundAssignOp = enum {
    add_assign,  // +=
    sub_assign,  // -=
    mul_assign,  // *=
    div_assign,  // /=
    mod_assign,  // %=
};

pub const StructFieldInit = struct {
    name: []const u8,
    value: Expr,
};

pub const MatchArm = struct {
    pattern: Pattern,
    body: Expr,
};

// 新增：is 表达式的分支（使用 -> 而不是 =>）
pub const IsArm = struct {
    pattern: Pattern,
    guard: ?Expr,  // 可选的 if 条件
    body: Expr,
};

pub const Pattern = union(enum) {
    identifier: []const u8,
    variant: struct {
        name: []const u8,
        bindings: [][]const u8,
    },
    literal: Expr,
    wildcard,
};

pub const Stmt = union(enum) {
    expr: Expr,
    let_decl: struct {
        name: []const u8,
        is_mut: bool,  // 新增：是否可变
        type: ?Type,
        init: ?Expr,
    },
    // 🆕 赋值语句
    assign: struct {
        target: Expr,  // 可以是变量、字段访问、数组索引等
        value: Expr,
    },
    // 🆕 复合赋值语句
    compound_assign: struct {
        target: Expr,
        op: CompoundAssignOp,
        value: Expr,
    },
    return_stmt: ?Expr,
    break_stmt: ?Expr,  // loop 可以返回值
    continue_stmt,
    // 新增：统一的 loop 语句
    loop_stmt: struct {
        condition: ?Expr,      // loop if condition
        iterator: ?LoopIterator,  // loop for item in iter
        body: []Stmt,
    },
    // 向后兼容
    while_loop: struct {
        condition: Expr,
        body: []Stmt,
    },
    for_loop: struct {
        init: ?*Stmt,
        condition: ?Expr,
        step: ?Expr,
        body: []Stmt,
    },
};

// 新增：loop for 的迭代器
pub const LoopIterator = struct {
    binding: []const u8,  // item
    iterable: Expr,       // collection
};

pub const Param = struct {
    name: []const u8,
    type: Type,
};

pub const FunctionDecl = struct {
    name: []const u8,
    type_params: [][]const u8,
    params: []Param,
    return_type: Type,
    body: []Stmt,
    is_public: bool,
    is_async: bool,  // 新增：是否异步
};

pub const StructDecl = struct {
    name: []const u8,
    type_params: [][]const u8,
    fields: []StructField,
    methods: []FunctionDecl,  // 新增：方法在类型内定义
    is_public: bool,
};

pub const StructField = struct {
    name: []const u8,
    type: Type,
    is_public: bool,  // 新增：字段可见性
    is_mut: bool,     // 新增：字段可变性
};

pub const EnumVariant = struct {
    name: []const u8,
    fields: []Type, // 数据变体的字段类型
};

pub const EnumDecl = struct {
    name: []const u8,
    type_params: [][]const u8,
    variants: []EnumVariant,
    methods: []FunctionDecl,  // 新增：枚举也可以有方法
    is_public: bool,
};

pub const TraitDecl = struct {
    name: []const u8,
    type_params: [][]const u8,
    methods: []FunctionSignature,
    is_public: bool,
};

pub const FunctionSignature = struct {
    name: []const u8,
    params: []Param,
    return_type: Type,
};

pub const ImplDecl = struct {
    trait_name: []const u8,
    type_args: []Type,
    target_type: Type,
    methods: []FunctionDecl,
};

pub const ImportDecl = struct {
    path: []const u8,
};

// 新增：统一的类型定义类型
pub const TypeDeclKind = union(enum) {
    struct_type: struct {
        fields: []StructField,
        methods: []FunctionDecl,
    },
    enum_type: struct {
        variants: []EnumVariant,
        methods: []FunctionDecl,
    },
    trait_type: struct {
        methods: []FunctionSignature,
    },
};

// 新增：统一的 type 声明
pub const TypeDecl = struct {
    name: []const u8,
    type_params: [][]const u8,
    kind: TypeDeclKind,
    is_public: bool,
};

pub const TopLevelDecl = union(enum) {
    function: FunctionDecl,
    // 新增：统一的 type 声明
    type_decl: TypeDecl,
    // 向后兼容
    struct_decl: StructDecl,
    enum_decl: EnumDecl,
    trait_decl: TraitDecl,
    impl_decl: ImplDecl,
    import_decl: ImportDecl,
};

pub const Program = struct {
    declarations: []TopLevelDecl,
};

