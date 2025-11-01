//===--- builtin_symbols.h - Builtin Functions -------------------*- C++ -*-===//
//
// PawLang Compiler - Builtin Functions Registration
//
// 18种类型重载：i8-i128, u8-u128, f8-f128, bool, char, string
//
//===----------------------------------------------------------------------===//

#ifndef PAW_BUILTIN_SYMBOLS_H
#define PAW_BUILTIN_SYMBOLS_H

namespace pawc {

class SymbolTable;
class TypeSystem;

/// BuiltinSymbols - 注册所有内置函数
class BuiltinSymbols {
public:
    static void registerAll(SymbolTable* symbol_table, TypeSystem* type_system);
    
private:
    static void registerPrint(SymbolTable* st, TypeSystem* ts);
    static void registerPrintln(SymbolTable* st, TypeSystem* ts);
    static void registerToString(SymbolTable* st, TypeSystem* ts);
    static void registerLen(SymbolTable* st, TypeSystem* ts);
    static void registerPanic(SymbolTable* st, TypeSystem* ts);
    static void registerAssert(SymbolTable* st, TypeSystem* ts);
    static void registerDebugAssert(SymbolTable* st, TypeSystem* ts);
    static void registerUnreachable(SymbolTable* st, TypeSystem* ts);
};

} // namespace pawc

#endif // PAW_BUILTIN_SYMBOLS_H
