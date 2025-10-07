const std = @import("std");

pub const Type = union(enum) {
    void,
    bool,
    byte,
    char,
    int,
    long,
    float,
    double,
    string,
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
            .void, .bool, .byte, .char, .int, .long, .float, .double, .string => true,
            .generic => |name| std.mem.eql(u8, name, other.generic),
            .named => |name| std.mem.eql(u8, name, other.named),
            .pointer => |ptr| ptr.eql(other.pointer.*),
            .array => |arr| {
                if (arr.size != other.array.size) return false;
                return arr.element.eql(other.array.element.*);
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
    match_expr: struct {
        value: *Expr,
        arms: []MatchArm,
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

pub const StructFieldInit = struct {
    name: []const u8,
    value: Expr,
};

pub const MatchArm = struct {
    pattern: Pattern,
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
        type: ?Type,
        init: ?Expr,
    },
    return_stmt: ?Expr,
    break_stmt,
    continue_stmt,
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
};

pub const StructDecl = struct {
    name: []const u8,
    type_params: [][]const u8,
    fields: []StructField,
    is_public: bool,
};

pub const StructField = struct {
    name: []const u8,
    type: Type,
};

pub const EnumVariant = struct {
    name: []const u8,
    fields: []Type, // 数据变体的字段类型
};

pub const EnumDecl = struct {
    name: []const u8,
    type_params: [][]const u8,
    variants: []EnumVariant,
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

pub const TopLevelDecl = union(enum) {
    function: FunctionDecl,
    struct_decl: StructDecl,
    enum_decl: EnumDecl,
    trait_decl: TraitDecl,
    impl_decl: ImplDecl,
    import_decl: ImportDecl,
};

pub const Program = struct {
    declarations: []TopLevelDecl,
};

