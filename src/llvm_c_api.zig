//! Direct LLVM C API bindings for PawLang
//! 
//! This module provides minimal bindings to LLVM's C API,
//! allowing us to use our locally compiled LLVM without
//! relying on llvm-zig or system LLVM installations.
//!
//! We only bind the functions we actually need for PawLang's code generation.

const std = @import("std");

// LLVM C API types (opaque pointers)
pub const ContextRef = ?*opaque {};
pub const ModuleRef = ?*opaque {};
pub const BuilderRef = ?*opaque {};
pub const TypeRef = ?*opaque {};
pub const ValueRef = ?*opaque {};
pub const BasicBlockRef = ?*opaque {};

// LLVM Linkage Types
pub const Linkage = enum(c_uint) {
    External = 0,
    AvailableExternally = 1,
    LinkOnceAny = 2,
    LinkOnceODR = 3,
    LinkOnceODRAutoHide = 4,
    WeakAny = 5,
    WeakODR = 6,
    Appending = 7,
    Internal = 8,
    Private = 9,
    DLLImport = 10,
    DLLExport = 11,
    ExternalWeak = 12,
    Ghost = 13,
    Common = 14,
    LinkerPrivate = 15,
    LinkerPrivateWeak = 16,
};

// LLVM Integer Comparison Predicates
pub const IntPredicate = enum(c_uint) {
    EQ = 32,  // equal
    NE = 33,  // not equal
    UGT = 34, // unsigned greater than
    UGE = 35, // unsigned greater or equal
    ULT = 36, // unsigned less than
    ULE = 37, // unsigned less or equal
    SGT = 38, // signed greater than
    SGE = 39, // signed greater or equal
    SLT = 40, // signed less than
    SLE = 41, // signed less or equal
};

// ============================================================================
// Context Functions
// ============================================================================

/// Create a new LLVM context
pub extern "c" fn LLVMContextCreate() ContextRef;

/// Dispose of an LLVM context
pub extern "c" fn LLVMContextDispose(C: ContextRef) void;

// ============================================================================
// Module Functions
// ============================================================================

/// Create a new module in a context
pub extern "c" fn LLVMModuleCreateWithNameInContext(
    ModuleID: [*:0]const u8,
    C: ContextRef,
) ModuleRef;

/// Dispose of a module
pub extern "c" fn LLVMDisposeModule(M: ModuleRef) void;

/// Print module to a string
pub extern "c" fn LLVMPrintModuleToString(M: ModuleRef) [*:0]u8;

/// Dispose of a string returned by LLVM
pub extern "c" fn LLVMDisposeMessage(Message: [*:0]u8) void;

/// Verify a module
pub extern "c" fn LLVMVerifyModule(
    M: ModuleRef,
    Action: c_uint,
    OutMessage: *[*:0]u8,
) c_int;

// ============================================================================
// Type Functions
// ============================================================================

/// Get void type
pub extern "c" fn LLVMVoidTypeInContext(C: ContextRef) TypeRef;

/// Get i1 type (boolean)
pub extern "c" fn LLVMInt1TypeInContext(C: ContextRef) TypeRef;

/// Get i8 type
pub extern "c" fn LLVMInt8TypeInContext(C: ContextRef) TypeRef;

/// Get i32 type
pub extern "c" fn LLVMInt32TypeInContext(C: ContextRef) TypeRef;

/// Get i64 type
pub extern "c" fn LLVMInt64TypeInContext(C: ContextRef) TypeRef;

/// Get double type (f64)
pub extern "c" fn LLVMDoubleTypeInContext(C: ContextRef) TypeRef;

/// Get type of a value
pub extern "c" fn LLVMTypeOf(Val: ValueRef) TypeRef;

/// Get pointer type
pub extern "c" fn LLVMPointerType(ElementType: TypeRef, AddressSpace: c_uint) TypeRef;

/// Get function type
pub extern "c" fn LLVMFunctionType(
    ReturnType: TypeRef,
    ParamTypes: [*]TypeRef,
    ParamCount: c_uint,
    IsVarArg: c_int,
) TypeRef;

// ============================================================================
// Value/Function Functions
// ============================================================================

/// Add a function to a module
pub extern "c" fn LLVMAddFunction(
    M: ModuleRef,
    Name: [*:0]const u8,
    FunctionTy: TypeRef,
) ValueRef;

/// Get a function parameter
pub extern "c" fn LLVMGetParam(Fn: ValueRef, Index: c_uint) ValueRef;

/// Set function linkage
pub extern "c" fn LLVMSetLinkage(Global: ValueRef, Linkage: Linkage) void;

/// Create a constant integer
pub extern "c" fn LLVMConstInt(
    IntTy: TypeRef,
    N: c_ulonglong,
    SignExtend: c_int,
) ValueRef;

/// Create a constant real (floating point)
pub extern "c" fn LLVMConstReal(RealTy: TypeRef, N: f64) ValueRef;

// ============================================================================
// Basic Block Functions
// ============================================================================

/// Append a basic block to a function
pub extern "c" fn LLVMAppendBasicBlockInContext(
    C: ContextRef,
    Fn: ValueRef,
    Name: [*:0]const u8,
) BasicBlockRef;

// ============================================================================
// Builder Functions
// ============================================================================

/// Create a builder in a context
pub extern "c" fn LLVMCreateBuilderInContext(C: ContextRef) BuilderRef;

/// Dispose of a builder
pub extern "c" fn LLVMDisposeBuilder(Builder: BuilderRef) void;

/// Position builder at end of basic block
pub extern "c" fn LLVMPositionBuilderAtEnd(
    Builder: BuilderRef,
    Block: BasicBlockRef,
) void;

// ============================================================================
// Instruction Building Functions
// ============================================================================

/// Build return instruction
pub extern "c" fn LLVMBuildRetVoid(Builder: BuilderRef) ValueRef;

/// Build return instruction with value
pub extern "c" fn LLVMBuildRet(Builder: BuilderRef, V: ValueRef) ValueRef;

/// Build add instruction
pub extern "c" fn LLVMBuildAdd(
    Builder: BuilderRef,
    LHS: ValueRef,
    RHS: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build subtract instruction
pub extern "c" fn LLVMBuildSub(
    Builder: BuilderRef,
    LHS: ValueRef,
    RHS: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build multiply instruction
pub extern "c" fn LLVMBuildMul(
    Builder: BuilderRef,
    LHS: ValueRef,
    RHS: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build signed division instruction
pub extern "c" fn LLVMBuildSDiv(
    Builder: BuilderRef,
    LHS: ValueRef,
    RHS: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build function call
pub extern "c" fn LLVMBuildCall2(
    Builder: BuilderRef,
    Ty: TypeRef,
    Fn: ValueRef,
    Args: [*]ValueRef,
    NumArgs: c_uint,
    Name: [*:0]const u8,
) ValueRef;

/// Build alloca instruction (stack allocation)
pub extern "c" fn LLVMBuildAlloca(
    Builder: BuilderRef,
    Ty: TypeRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build load instruction
pub extern "c" fn LLVMBuildLoad2(
    Builder: BuilderRef,
    Ty: TypeRef,
    PointerVal: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build store instruction
pub extern "c" fn LLVMBuildStore(
    Builder: BuilderRef,
    Val: ValueRef,
    Ptr: ValueRef,
) ValueRef;

/// Build integer comparison
pub extern "c" fn LLVMBuildICmp(
    Builder: BuilderRef,
    Op: IntPredicate,
    LHS: ValueRef,
    RHS: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build conditional branch
pub extern "c" fn LLVMBuildCondBr(
    Builder: BuilderRef,
    If: ValueRef,
    Then: BasicBlockRef,
    Else: BasicBlockRef,
) ValueRef;

/// Build unconditional branch
pub extern "c" fn LLVMBuildBr(Builder: BuilderRef, Dest: BasicBlockRef) ValueRef;

/// Build logical AND instruction
pub extern "c" fn LLVMBuildAnd(
    Builder: BuilderRef,
    LHS: ValueRef,
    RHS: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build logical OR instruction
pub extern "c" fn LLVMBuildOr(
    Builder: BuilderRef,
    LHS: ValueRef,
    RHS: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build logical NOT instruction
pub extern "c" fn LLVMBuildNot(
    Builder: BuilderRef,
    V: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build negation instruction
pub extern "c" fn LLVMBuildNeg(
    Builder: BuilderRef,
    V: ValueRef,
    Name: [*:0]const u8,
) ValueRef;

/// Build PHI node
pub extern "c" fn LLVMBuildPhi(
    Builder: BuilderRef,
    Ty: TypeRef,
    Name: [*:0]const u8,
) ValueRef;

/// Add incoming value to PHI node
pub extern "c" fn LLVMAddIncoming(
    PhiNode: ValueRef,
    IncomingValues: [*]ValueRef,
    IncomingBlocks: [*]BasicBlockRef,
    Count: c_uint,
) void;

/// Get basic block of a value (for PHI nodes)
pub extern "c" fn LLVMGetInsertBlock(Builder: BuilderRef) BasicBlockRef;

// ============================================================================
// Wrapper Types for Better Zig Experience
// ============================================================================

pub const Context = struct {
    ref: ContextRef,

    pub fn create() Context {
        return Context{ .ref = LLVMContextCreate() };
    }

    pub fn dispose(self: *Context) void {
        LLVMContextDispose(self.ref);
    }

    pub fn createModule(self: Context, name: [:0]const u8) Module {
        return Module{
            .ref = LLVMModuleCreateWithNameInContext(name.ptr, self.ref),
            .context = self,
        };
    }

    pub fn createBuilder(self: Context) Builder {
        return Builder{
            .ref = LLVMCreateBuilderInContext(self.ref),
        };
    }

    pub fn voidType(self: Context) TypeRef {
        return LLVMVoidTypeInContext(self.ref);
    }

    pub fn i1Type(self: Context) TypeRef {
        return LLVMInt1TypeInContext(self.ref);
    }

    pub fn i32Type(self: Context) TypeRef {
        return LLVMInt32TypeInContext(self.ref);
    }

    pub fn i64Type(self: Context) TypeRef {
        return LLVMInt64TypeInContext(self.ref);
    }

    pub fn doubleType(self: Context) TypeRef {
        return LLVMDoubleTypeInContext(self.ref);
    }
};

pub const Module = struct {
    ref: ModuleRef,
    context: Context,

    pub fn dispose(self: *Module) void {
        LLVMDisposeModule(self.ref);
    }

    pub fn toString(self: Module) [:0]const u8 {
        const str = LLVMPrintModuleToString(self.ref);
        // Note: caller must free with LLVMDisposeMessage
        return std.mem.span(str);
    }

    pub fn addFunction(self: Module, name: [:0]const u8, func_type: TypeRef) ValueRef {
        return LLVMAddFunction(self.ref, name.ptr, func_type);
    }

    pub fn verify(self: Module) !void {
        var error_msg: [*:0]u8 = undefined;
        const result = LLVMVerifyModule(self.ref, 2, &error_msg); // 2 = ReturnStatusAction
        if (result != 0) {
            defer LLVMDisposeMessage(error_msg);
            std.debug.print("LLVM module verification failed: {s}\n", .{error_msg});
            return error.VerificationFailed;
        }
    }
};

pub const Builder = struct {
    ref: BuilderRef,

    pub fn dispose(self: *Builder) void {
        LLVMDisposeBuilder(self.ref);
    }

    pub fn positionAtEnd(self: Builder, block: BasicBlockRef) void {
        LLVMPositionBuilderAtEnd(self.ref, block);
    }

    pub fn buildRet(self: Builder, value: ValueRef) ValueRef {
        return LLVMBuildRet(self.ref, value);
    }

    pub fn buildRetVoid(self: Builder) ValueRef {
        return LLVMBuildRetVoid(self.ref);
    }

    pub fn buildAdd(self: Builder, lhs: ValueRef, rhs: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildAdd(self.ref, lhs, rhs, name.ptr);
    }

    pub fn buildSub(self: Builder, lhs: ValueRef, rhs: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildSub(self.ref, lhs, rhs, name.ptr);
    }

    pub fn buildMul(self: Builder, lhs: ValueRef, rhs: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildMul(self.ref, lhs, rhs, name.ptr);
    }

    pub fn buildSDiv(self: Builder, lhs: ValueRef, rhs: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildSDiv(self.ref, lhs, rhs, name.ptr);
    }

    pub fn buildCall(
        self: Builder,
        func_type: TypeRef,
        func: ValueRef,
        args: []ValueRef,
        name: [:0]const u8,
    ) ValueRef {
        return LLVMBuildCall2(
            self.ref,
            func_type,
            func,
            args.ptr,
            @intCast(args.len),
            name.ptr,
        );
    }
    
    pub fn buildBr(self: Builder, dest: BasicBlockRef) ValueRef {
        return LLVMBuildBr(self.ref, dest);
    }
    
    pub fn buildICmp(self: Builder, op: IntPredicate, lhs: ValueRef, rhs: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildICmp(self.ref, op, lhs, rhs, name.ptr);
    }
    
    pub fn buildAnd(self: Builder, lhs: ValueRef, rhs: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildAnd(self.ref, lhs, rhs, name.ptr);
    }
    
    pub fn buildOr(self: Builder, lhs: ValueRef, rhs: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildOr(self.ref, lhs, rhs, name.ptr);
    }
    
    pub fn buildNot(self: Builder, value: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildNot(self.ref, value, name.ptr);
    }
    
    pub fn buildNeg(self: Builder, value: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildNeg(self.ref, value, name.ptr);
    }
    
    pub fn buildAlloca(self: Builder, ty: TypeRef, name: [:0]const u8) ValueRef {
        return LLVMBuildAlloca(self.ref, ty, name.ptr);
    }
    
    pub fn buildLoad(self: Builder, ty: TypeRef, ptr: ValueRef, name: [:0]const u8) ValueRef {
        return LLVMBuildLoad2(self.ref, ty, ptr, name.ptr);
    }
    
    pub fn buildStore(self: Builder, value: ValueRef, ptr: ValueRef) ValueRef {
        return LLVMBuildStore(self.ref, value, ptr);
    }
    
    pub fn buildPhi(self: Builder, ty: TypeRef, name: [:0]const u8) ValueRef {
        return LLVMBuildPhi(self.ref, ty, name.ptr);
    }
    
    pub fn getInsertBlock(self: Builder) BasicBlockRef {
        return LLVMGetInsertBlock(self.ref);
    }
};

// Helper function to create constant int
pub fn constI32(context: Context, value: i32) ValueRef {
    return LLVMConstInt(context.i32Type(), @intCast(value), 1);
}

pub fn constI64(context: Context, value: i64) ValueRef {
    return LLVMConstInt(context.i64Type(), @intCast(value), 1);
}

pub fn constDouble(context: Context, value: f64) ValueRef {
    return LLVMConstReal(context.doubleType(), value);
}

pub fn functionType(
    return_type: TypeRef,
    param_types: []TypeRef,
    is_var_arg: bool,
) TypeRef {
    return LLVMFunctionType(
        return_type,
        param_types.ptr,
        @intCast(param_types.len),
        if (is_var_arg) 1 else 0,
    );
}

pub fn appendBasicBlock(context: Context, func: ValueRef, name: [:0]const u8) BasicBlockRef {
    return LLVMAppendBasicBlockInContext(context.ref, func, name.ptr);
}

