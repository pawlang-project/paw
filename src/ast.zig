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
    
    /// 释放Type的动态分配部分（如type_args）
    /// 注意：只释放一层，不做深度递归，避免double-free
    pub fn deinit(self: Type, allocator: std.mem.Allocator) void {
        switch (self) {
            .generic_instance => |gi| {
                // 释放type_args slice（这是最主要的泄漏源）
                allocator.free(gi.type_args);
            },
            .function => |func| {
                // 释放参数slice
                allocator.free(func.params);
            },
            else => {}, // 其他类型暂不处理，避免复杂的所有权问题
        }
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
    // 🆕 静态方法调用：Type<T>::method()
    static_method_call: struct {
        type_name: []const u8,
        type_args: []Type,
        method_name: []const u8,
        args: []Expr,
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
    // 🆕 字符串插值
    string_interp: struct {
        parts: []StringInterpPart,
    },
    // 🆕 错误传播 (expr?)
    try_expr: *Expr,
    
    /// 递归释放表达式及其子表达式
    pub fn deinit(self: Expr, allocator: std.mem.Allocator) void {
        switch (self) {
            .binary => |bin| {
                bin.left.deinit(allocator);
                allocator.destroy(bin.left);
                bin.right.deinit(allocator);
                allocator.destroy(bin.right);
            },
            .unary => |un| {
                un.operand.deinit(allocator);
                allocator.destroy(un.operand);
            },
            .call => |call| {
                call.callee.deinit(allocator);
                allocator.destroy(call.callee);
                for (call.args) |arg| {
                    arg.deinit(allocator);
                }
                allocator.free(call.args);
                allocator.free(call.type_args);
            },
            .static_method_call => |smc| {
                for (smc.args) |arg| {
                    arg.deinit(allocator);
                }
                allocator.free(smc.args);
                allocator.free(smc.type_args);
            },
            .field_access => |fa| {
                fa.object.deinit(allocator);
                allocator.destroy(fa.object);
            },
            .struct_init => |si| {
                allocator.free(si.type_args);
                allocator.free(si.fields);
            },
            .enum_variant => |ev| {
                for (ev.args) |arg| {
                    arg.deinit(allocator);
                }
                allocator.free(ev.args);
            },
            .block => |stmts| {
                for (stmts) |stmt| {
                    stmt.deinit(allocator);
                }
                allocator.free(stmts);
            },
            .if_expr => |ie| {
                ie.condition.deinit(allocator);
                allocator.destroy(ie.condition);
                ie.then_branch.deinit(allocator);
                allocator.destroy(ie.then_branch);
                if (ie.else_branch) |eb| {
                    eb.deinit(allocator);
                    allocator.destroy(eb);
                }
            },
            .is_expr => |is_e| {
                is_e.value.deinit(allocator);
                allocator.destroy(is_e.value);
                allocator.free(is_e.arms);
            },
            .match_expr => |me| {
                me.value.deinit(allocator);
                allocator.destroy(me.value);
                allocator.free(me.arms);
            },
            .as_expr => |ae| {
                ae.value.deinit(allocator);
                allocator.destroy(ae.value);
            },
            .await_expr => |ae| {
                ae.deinit(allocator);
                allocator.destroy(ae);
            },
            .array_literal => |arr| {
                for (arr) |elem| {
                    elem.deinit(allocator);
                }
                allocator.free(arr);
            },
            .array_index => |ai| {
                ai.array.deinit(allocator);
                allocator.destroy(ai.array);
                ai.index.deinit(allocator);
                allocator.destroy(ai.index);
            },
            .range => |rng| {
                rng.start.deinit(allocator);
                allocator.destroy(rng.start);
                rng.end.deinit(allocator);
                allocator.destroy(rng.end);
            },
            .string_interp => |si| {
                allocator.free(si.parts);
            },
            .try_expr => |te| {
                te.deinit(allocator);
                allocator.destroy(te);
            },
            else => {}, // 字面量和标识符不需要释放
        }
    }
};

// 🆕 字符串插值的部分
pub const StringInterpPart = union(enum) {
    literal: []const u8,  // 字面量部分
    expr: Expr,           // 表达式部分（$var 或 ${expr}）
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
    
    /// 释放语句及其子表达式
    pub fn deinit(self: Stmt, allocator: std.mem.Allocator) void {
        switch (self) {
            .expr => |expr| expr.deinit(allocator),
            .let_decl => |ld| {
                if (ld.init) |init| {
                    init.deinit(allocator);
                }
            },
            .assign => |assign| {
                assign.target.deinit(allocator);
                assign.value.deinit(allocator);
            },
            .compound_assign => |ca| {
                ca.target.deinit(allocator);
                ca.value.deinit(allocator);
            },
            .return_stmt => |ret| {
                if (ret) |expr| {
                    expr.deinit(allocator);
                }
            },
            .break_stmt => |brk| {
                if (brk) |expr| {
                    expr.deinit(allocator);
                }
            },
            .continue_stmt => {},
            .loop_stmt => |loop| {
                if (loop.condition) |cond| {
                    cond.deinit(allocator);
                }
                if (loop.iterator) |iter| {
                    iter.iterable.deinit(allocator);
                }
                for (loop.body) |stmt| {
                    stmt.deinit(allocator);
                }
                allocator.free(loop.body);
            },
            .while_loop => |loop| {
                loop.condition.deinit(allocator);
                for (loop.body) |stmt| {
                    stmt.deinit(allocator);
                }
                allocator.free(loop.body);
            },
            .for_loop => |loop| {
                if (loop.init) |init| {
                    init.deinit(allocator);
                    allocator.destroy(init);
                }
                if (loop.condition) |cond| {
                    cond.deinit(allocator);
                }
                if (loop.step) |step| {
                    step.deinit(allocator);
                }
                for (loop.body) |stmt| {
                    stmt.deinit(allocator);
                }
                allocator.free(loop.body);
            },
        }
    }
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
    
    pub fn deinit(self: FunctionDecl, allocator: std.mem.Allocator) void {
        allocator.free(self.type_params);
        allocator.free(self.params);
        // 注意：不释放参数类型，避免复杂的所有权问题
        for (self.body) |stmt| {
            stmt.deinit(allocator);
        }
        allocator.free(self.body);
    }
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
    module_path: []const u8,      // math.add -> "math" (需要释放)
    items: ImportItems,           // 🆕 支持多项导入
    
    pub const ImportItems = union(enum) {
        single: []const u8,       // import math.add
        multiple: [][]const u8,   // import math.{add, sub} (需要释放slice)
    };
    
    pub fn deinit(self: ImportDecl, allocator: std.mem.Allocator) void {
        allocator.free(self.module_path);
        if (self.items == .multiple) {
            allocator.free(self.items.multiple);
        }
    }
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
    
    pub fn deinit(self: TypeDecl, allocator: std.mem.Allocator) void {
        allocator.free(self.type_params);
        switch (self.kind) {
            .struct_type => |st| {
                allocator.free(st.fields);
                for (st.methods) |method| {
                    method.deinit(allocator);
                }
                allocator.free(st.methods);
            },
            .enum_type => |et| {
                // 释放每个变体的字段
                for (et.variants) |variant| {
                    allocator.free(variant.fields);
                }
                allocator.free(et.variants);
                for (et.methods) |method| {
                    method.deinit(allocator);
                }
                allocator.free(et.methods);
            },
            .trait_type => |tt| {
                allocator.free(tt.methods);
            },
        }
    }
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
    
    pub fn deinit(self: Program, allocator: std.mem.Allocator) void {
        // 递归释放所有声明
        for (self.declarations) |decl| {
            switch (decl) {
                .function => |func| func.deinit(allocator),
                .type_decl => |td| td.deinit(allocator),
                .import_decl => |id| id.deinit(allocator),
                else => {}, // 其他类型暂不处理
            }
        }
        allocator.free(self.declarations);
    }
};

