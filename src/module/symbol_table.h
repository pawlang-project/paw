#ifndef PAWC_SYMBOL_TABLE_H
#define PAWC_SYMBOL_TABLE_H

#include <string>
#include <map>
#include <memory>
#include "llvm/IR/Value.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"

namespace pawc {

// 前向声明
struct StructStmt;
struct EnumStmt;
struct FunctionStmt;

/**
 * 符号表系统
 * 
 * 管理跨模块的符号可见性和访问
 */
class SymbolTable {
public:
    enum class SymbolKind {
        Function,
        GenericFunction,  // 泛型函数
        Type,             // Struct或Enum
        Variable
    };
    
    struct Symbol {
        std::string name;           // 符号名
        std::string module;         // 所属模块
        SymbolKind kind;            // 符号类型
        bool is_public;             // 是否公开
        llvm::Value* value;         // LLVM值（函数、变量）
        llvm::Type* type;           // LLVM类型（类型定义）
        const void* ast_node;       // AST节点（StructStmt* / EnumStmt* / FunctionStmt*），用于跨模块重建
        
        Symbol() : kind(SymbolKind::Function), is_public(false), value(nullptr), type(nullptr), ast_node(nullptr) {}
    };
    
    SymbolTable() = default;
    
    // 注册符号
    void registerFunction(const std::string& module, const std::string& name, 
                         bool is_public, llvm::Function* func);
    
    void registerGenericFunction(const std::string& module, const std::string& name,
                                bool is_public, const FunctionStmt* ast);
    
    void registerType(const std::string& module, const std::string& name,
                     bool is_public, llvm::Type* type, const void* ast_node = nullptr);
    
    // 注册泛型struct实例（如Pair_i32_string）
    void registerGenericStructInstance(const std::string& module, const std::string& mangled_name,
                                       const std::string& base_name, bool is_public, 
                                       llvm::Type* type, const void* ast_node = nullptr);
    
    void registerVariable(const std::string& module, const std::string& name,
                         bool is_public, llvm::Value* value);
    
    // 查找符号
    Symbol* lookup(const std::string& name, const std::string& current_module);
    Symbol* lookupInModule(const std::string& module, const std::string& name);
    
    // 可见性检查
    bool isAccessible(const Symbol& symbol, const std::string& from_module) const;
    
    // 获取模块的所有公开符号
    std::vector<Symbol*> getPublicSymbols(const std::string& module);
    
    // 调试：打印所有符号
    void dump() const;
    
private:
    // module_name -> symbol_name -> Symbol
    std::map<std::string, std::map<std::string, Symbol>> module_symbols_;
};

} // namespace pawc

#endif // PAWC_SYMBOL_TABLE_H

