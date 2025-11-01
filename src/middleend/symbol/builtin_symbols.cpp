//===--- builtin_symbols.cpp - Builtin Functions Implementation --*- C++ -*-===//

#include "builtin_symbols.h"
#include "symbol_table.h"
#include "middleend/types/type_system.h"

namespace pawc {

void BuiltinSymbols::registerAll(SymbolTable* st, TypeSystem* ts) {
    registerPrint(st, ts);
    registerPrintln(st, ts);
    registerToString(st, ts);
    registerLen(st, ts);
    registerPanic(st, ts);
    registerAssert(st, ts);
    registerDebugAssert(st, ts);
    registerUnreachable(st, ts);
}

// print(value) - 18种类型重载
void BuiltinSymbols::registerPrint(SymbolTable* st, TypeSystem* ts) {
    Type* void_type = ts->getVoidType();
    
    // 有符号整数 (5)
    st->defineFunction("print", ts->getFunctionType({ts->getI8Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI16Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI32Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI64Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI128Type()}, void_type), true);
    
    // 无符号整数 (5)
    st->defineFunction("print", ts->getFunctionType({ts->getU8Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU16Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU32Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU64Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU128Type()}, void_type), true);
    
    // 浮点数 (5) - 完整精度
    st->defineFunction("print", ts->getFunctionType({ts->getF8Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF16Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF32Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF64Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF128Type()}, void_type), true);
    
    // 其他 (3)
    st->defineFunction("print", ts->getFunctionType({ts->getBoolType()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getCharType()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getStringType()}, void_type), true);
}

// println(value) - 18种类型重载
void BuiltinSymbols::registerPrintln(SymbolTable* st, TypeSystem* ts) {
    Type* void_type = ts->getVoidType();
    
    // 有符号整数 (5)
    st->defineFunction("println", ts->getFunctionType({ts->getI8Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI16Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI32Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI64Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI128Type()}, void_type), true);
    
    // 无符号整数 (5)
    st->defineFunction("println", ts->getFunctionType({ts->getU8Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU16Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU32Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU64Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU128Type()}, void_type), true);
    
    // 浮点数 (5) - 完整精度
    st->defineFunction("println", ts->getFunctionType({ts->getF8Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF16Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF32Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF64Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF128Type()}, void_type), true);
    
    // 其他 (3)
    st->defineFunction("println", ts->getFunctionType({ts->getBoolType()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getCharType()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getStringType()}, void_type), true);
}

// to_string(value) -> string - 18种类型重载
void BuiltinSymbols::registerToString(SymbolTable* st, TypeSystem* ts) {
    Type* string_type = ts->getStringType();
    
    // 有符号整数 (5)
    st->defineFunction("to_string", ts->getFunctionType({ts->getI8Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI16Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI32Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI64Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI128Type()}, string_type), true);
    
    // 无符号整数 (5)
    st->defineFunction("to_string", ts->getFunctionType({ts->getU8Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU16Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU32Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU64Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU128Type()}, string_type), true);
    
    // 浮点数 (5) - 完整精度
    st->defineFunction("to_string", ts->getFunctionType({ts->getF8Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF16Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF32Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF64Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF128Type()}, string_type), true);
    
    // 其他 (3)
    st->defineFunction("to_string", ts->getFunctionType({ts->getBoolType()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getCharType()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getStringType()}, string_type), true);
}

// len(value) -> u64
// 支持: string, Array[T; N], Slice[T]
void BuiltinSymbols::registerLen(SymbolTable* st, TypeSystem* ts) {
    Type* u64_type = ts->getU64Type();
    
    // len(str: string) -> u64
    st->defineFunction("len",
        ts->getFunctionType({ts->getStringType()}, u64_type),
        true);
    
    // 注意: Array和Slice的len需要泛型支持
    // 这里注册通用签名，具体类型在CodeGen时解析
    // len(array: [T; N]) -> u64  (编译时常量)
    // len(slice: [T]) -> u64     (运行时获取)
    
    // TODO: 当泛型系统完善后，添加Array和Slice的泛型重载
    // 目前在TypeChecker中特殊处理Array和Slice类型的len调用
}

// panic(msg: string) -> void
void BuiltinSymbols::registerPanic(SymbolTable* st, TypeSystem* ts) {
    st->defineFunction("panic",
        ts->getFunctionType({ts->getStringType()}, ts->getVoidType()),
        true);
}

// assert(condition: bool, message: string) -> void
void BuiltinSymbols::registerAssert(SymbolTable* st, TypeSystem* ts) {
    st->defineFunction("assert",
        ts->getFunctionType({ts->getBoolType(), ts->getStringType()}, ts->getVoidType()),
        true);
}

// debug_assert(condition: bool, message: string) -> void (只在debug模式生效)
void BuiltinSymbols::registerDebugAssert(SymbolTable* st, TypeSystem* ts) {
    st->defineFunction("debug_assert",
        ts->getFunctionType({ts->getBoolType(), ts->getStringType()}, ts->getVoidType()),
        true);
}

// unreachable() -> void
void BuiltinSymbols::registerUnreachable(SymbolTable* st, TypeSystem* ts) {
    st->defineFunction("unreachable",
        ts->getFunctionType({}, ts->getVoidType()),
        true);
}

} // namespace pawc
