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
struct InterfaceStmt;
struct SupportStmt;

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
        Interface,        // Interface定义
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
    
    // 注册接口
    void registerInterface(const std::string& module, const std::string& name,
                          bool is_public, const InterfaceStmt* ast);
    
    // 接口实现信息结构
    struct InterfaceImpl {
        std::string type_name;          // "i32", "Vec<i32>"
        std::string interface_name;     // "Display"
        std::string module;             // 实现所在的模块
        const SupportStmt* impl_stmt;   // 实现语句
        bool is_generic;                // 是否是泛型实现
        std::vector<std::string> generic_params;  // ["T"]
        std::vector<std::string> constraints;     // ["Display"]
        
        InterfaceImpl() : impl_stmt(nullptr), is_generic(false) {}
    };
    
    // 注册接口实现: type_name 实现了 interface_name（旧方法，保持兼容）
    void registerInterfaceImpl(const std::string& module, const std::string& type_name,
                              const std::string& interface_name);
    
    // 注册接口实现（扩展版本，支持泛型）
    void registerInterfaceImplExtended(
        const std::string& module,
        const std::string& type_name,
        const std::string& interface_name,
        const SupportStmt* impl_stmt,
        bool is_generic = false,
        const std::vector<std::string>& generic_params = {},
        const std::vector<std::string>& constraints = {}
    );
    
    // 查询接口实现（考虑泛型）
    const InterfaceImpl* getInterfaceImpl(
        const std::string& type_name,
        const std::string& interface_name
    ) const;
    
    // 查询类型是否实现了接口
    bool typeImplementsInterface(const std::string& type_name, 
                                const std::string& interface_name) const;
    
    // 查询类型是否实现了接口（扩展版本，考虑泛型）
    bool typeImplementsInterfaceExtended(const std::string& type_name,
                                        const std::string& interface_name) const;
    
    // 获取类型实现的所有接口
    std::vector<std::string> getImplementedInterfaces(const std::string& type_name) const;
    
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
    
    // 性能优化：符号索引 (全局唯一名 -> Symbol*)
    std::map<std::string, Symbol*> symbol_index_;
    
    // 接口实现关系: type_name -> [interface_names]（旧系统，保持兼容）
    std::map<std::string, std::vector<std::string>> type_interfaces_;
    
    // 扩展接口实现: type_name -> interface_name -> InterfaceImpl
    std::map<std::string, std::map<std::string, InterfaceImpl>> interface_impls_;
    
    // 泛型接口实现: interface_name -> [InterfaceImpl]
    std::map<std::string, std::vector<InterfaceImpl>> generic_impls_;
};

} // namespace pawc

#endif // PAWC_SYMBOL_TABLE_H

