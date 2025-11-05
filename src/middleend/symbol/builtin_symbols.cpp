//===--- builtin_symbols.cpp - Builtin Functions Implementation --*- C++ -*-===//
/// @file builtin_symbols.cpp
/// @brief Implementation file
///

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

// print(value) - 18types/kindstypesoverload
void BuiltinSymbols::registerPrint(SymbolTable* st, TypeSystem* ts) {
    Type* void_type = ts->getVoidType();
    
    // Signed integers (5)
    st->defineFunction("print", ts->getFunctionType({ts->getI8Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI16Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI32Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI64Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getI128Type()}, void_type), true);
    
    // Unsigned integers (5)
    st->defineFunction("print", ts->getFunctionType({ts->getU8Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU16Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU32Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU64Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getU128Type()}, void_type), true);
    
    // Floating-point numbers (5) - completeprecision
    st->defineFunction("print", ts->getFunctionType({ts->getF8Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF16Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF32Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF64Type()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getF128Type()}, void_type), true);
    
    // Other types (3)
    st->defineFunction("print", ts->getFunctionType({ts->getBoolType()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getCharType()}, void_type), true);
    st->defineFunction("print", ts->getFunctionType({ts->getStringType()}, void_type), true);
}

// println(value) - 18types/kindstypesoverload
void BuiltinSymbols::registerPrintln(SymbolTable* st, TypeSystem* ts) {
    Type* void_type = ts->getVoidType();
    
    // Signed integers (5)
    st->defineFunction("println", ts->getFunctionType({ts->getI8Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI16Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI32Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI64Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getI128Type()}, void_type), true);
    
    // Unsigned integers (5)
    st->defineFunction("println", ts->getFunctionType({ts->getU8Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU16Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU32Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU64Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getU128Type()}, void_type), true);
    
    // Floating-point numbers (5) - completeprecision
    st->defineFunction("println", ts->getFunctionType({ts->getF8Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF16Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF32Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF64Type()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getF128Type()}, void_type), true);
    
    // Other types (3)
    st->defineFunction("println", ts->getFunctionType({ts->getBoolType()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getCharType()}, void_type), true);
    st->defineFunction("println", ts->getFunctionType({ts->getStringType()}, void_type), true);
}

// to_string(value) -> string - 18types/kindstypesoverload
void BuiltinSymbols::registerToString(SymbolTable* st, TypeSystem* ts) {
    Type* string_type = ts->getStringType();
    
    // Signed integers (5)
    st->defineFunction("to_string", ts->getFunctionType({ts->getI8Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI16Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI32Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI64Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getI128Type()}, string_type), true);
    
    // Unsigned integers (5)
    st->defineFunction("to_string", ts->getFunctionType({ts->getU8Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU16Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU32Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU64Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getU128Type()}, string_type), true);
    
    // Floating-point numbers (5) - completeprecision
    st->defineFunction("to_string", ts->getFunctionType({ts->getF8Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF16Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF32Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF64Type()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getF128Type()}, string_type), true);
    
    // Other types (3)
    st->defineFunction("to_string", ts->getFunctionType({ts->getBoolType()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getCharType()}, string_type), true);
    st->defineFunction("to_string", ts->getFunctionType({ts->getStringType()}, string_type), true);
}

// len(value) -> u64
// support: string, Array[T; N], Slice[T]
void BuiltinSymbols::registerLen(SymbolTable* st, TypeSystem* ts) {
    Type* u64_type = ts->getU64Type();
    
    // len(str: string) -> u64
    st->defineFunction("len",
        ts->getFunctionType({ts->getStringType()}, u64_type),
        true);
    
    // Note: ArrayandSliceof/thelenneedgenericsupport
    // hereregistergeneric/commonsignature，concretetypesin/atCodeGentime/whenparse
    // len(array: [T; N]) -> u64  (compile timeconstant)
    // len(slice: [T]) -> u64     (runtime/whenget)
    
    // TODO: When generic system is perfected, add Array and Slice generic overloads
    // currentlyfront/beforein/atTypeCheckermiddle/centerspecialhandleArrayandSlicetypesof/thelencall
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

// debug_assert(condition: bool, message: string) -> void (only effective in debug modeve)
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
