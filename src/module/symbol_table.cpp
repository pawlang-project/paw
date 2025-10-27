#include "symbol_table.h"
#include <iostream>
#include <algorithm>

namespace pawc {

void SymbolTable::registerFunction(const std::string& module, const std::string& name,
                                   bool is_public, llvm::Function* func) {
    Symbol symbol;
    symbol.name = name;
    symbol.module = module;
    symbol.kind = SymbolKind::Function;
    symbol.is_public = is_public;
    symbol.value = func;  // llvm::Function*自动向上转型为llvm::Value*
    symbol.type = nullptr;
    symbol.ast_node = nullptr;
    
    module_symbols_[module][name] = symbol;
}

void SymbolTable::registerGenericFunction(const std::string& module, const std::string& name,
                                          bool is_public, const FunctionStmt* ast) {
    Symbol symbol;
    symbol.name = name;
    symbol.module = module;
    symbol.kind = SymbolKind::GenericFunction;
    symbol.is_public = is_public;
    symbol.value = nullptr;  // 泛型函数没有具体的llvm::Function*
    symbol.type = nullptr;
    symbol.ast_node = static_cast<const void*>(ast);  // 保存AST定义
    
    module_symbols_[module][name] = symbol;
}

void SymbolTable::registerType(const std::string& module, const std::string& name,
                               bool is_public, llvm::Type* type, const void* ast_node) {
    Symbol symbol;
    symbol.name = name;
    symbol.module = module;
    symbol.kind = SymbolKind::Type;
    symbol.is_public = is_public;
    symbol.value = nullptr;
    symbol.type = type;
    symbol.ast_node = ast_node;  // 保存AST节点，用于跨模块重建
    
    module_symbols_[module][name] = symbol;
}

void SymbolTable::registerGenericStructInstance(const std::string& module, const std::string& mangled_name,
                                                const std::string& base_name, bool is_public,
                                                llvm::Type* type, const void* ast_node) {
    // 注册泛型struct实例（如Pair_i32_string）
    // 这样其他模块可以通过mangled_name查找到这个实例化的类型
    Symbol symbol;
    symbol.name = mangled_name;  // 使用mangled name作为查找键
    symbol.module = module;
    symbol.kind = SymbolKind::Type;
    symbol.is_public = is_public;  // 继承基础struct的可见性
    symbol.value = nullptr;
    symbol.type = type;
    symbol.ast_node = ast_node;  // 保存原始泛型struct的AST
    
    module_symbols_[module][mangled_name] = symbol;
    
    // 同时记录base_name信息（用于后续查找）
    // 可以通过注释或调试信息记录
}

void SymbolTable::registerVariable(const std::string& module, const std::string& name,
                                   bool is_public, llvm::Value* value) {
    Symbol symbol;
    symbol.name = name;
    symbol.module = module;
    symbol.kind = SymbolKind::Variable;
    symbol.is_public = is_public;
    symbol.value = value;
    symbol.type = nullptr;
    
    module_symbols_[module][name] = symbol;
}

void SymbolTable::registerInterface(const std::string& module, const std::string& name,
                                    bool is_public, const InterfaceStmt* ast) {
    Symbol symbol;
    symbol.name = name;
    symbol.module = module;
    symbol.kind = SymbolKind::Interface;
    symbol.is_public = is_public;
    symbol.value = nullptr;
    symbol.type = nullptr;
    symbol.ast_node = static_cast<const void*>(ast);  // 保存AST定义
    
    module_symbols_[module][name] = symbol;
}

void SymbolTable::registerInterfaceImpl(const std::string& module, const std::string& type_name,
                                        const std::string& interface_name) {
    // 记录类型实现了某个接口（旧方法，保持兼容）
    type_interfaces_[type_name].push_back(interface_name);
}

void SymbolTable::registerInterfaceImplExtended(
    const std::string& module,
    const std::string& type_name,
    const std::string& interface_name,
    const SupportStmt* impl_stmt,
    bool is_generic,
    const std::vector<std::string>& generic_params,
    const std::vector<std::string>& constraints
) {
    // 创建接口实现信息
    InterfaceImpl impl;
    impl.type_name = type_name;
    impl.interface_name = interface_name;
    impl.module = module;
    impl.impl_stmt = impl_stmt;
    impl.is_generic = is_generic;
    impl.generic_params = generic_params;
    impl.constraints = constraints;
    
    // 存储实现信息
    interface_impls_[type_name][interface_name] = impl;
    
    // 同时记录到旧系统（保持兼容）
    type_interfaces_[type_name].push_back(interface_name);
    
    // 如果是泛型实现，也存储到泛型实现列表
    if (is_generic) {
        generic_impls_[interface_name].push_back(impl);
    }
    
    // 【调试】注册成功（可选）
    // std::cerr << "[SymbolTable] 注册接口实现: " << type_name << " → " << interface_name << std::endl;
}

const SymbolTable::InterfaceImpl* SymbolTable::getInterfaceImpl(
    const std::string& type_name,
    const std::string& interface_name
) const {
    // 1. 先查找精确匹配
    auto type_it = interface_impls_.find(type_name);
    if (type_it != interface_impls_.end()) {
        auto interface_it = type_it->second.find(interface_name);
        if (interface_it != type_it->second.end()) {
            return &interface_it->second;
        }
    }
    
    // 2. 查找泛型匹配（Phase 2.5）
    auto generic_it = generic_impls_.find(interface_name);
    if (generic_it != generic_impls_.end()) {
        for (const auto& impl : generic_it->second) {
            if (matchesGenericPatternImpl(type_name, impl.type_name, impl.constraints)) {
                return &impl;
            }
        }
    }
    
    return nullptr;
}

bool SymbolTable::typeImplementsInterface(const std::string& type_name,
                                          const std::string& interface_name) const {
    auto it = type_interfaces_.find(type_name);
    if (it == type_interfaces_.end()) {
        return false;
    }
    
    const auto& interfaces = it->second;
    return std::find(interfaces.begin(), interfaces.end(), interface_name) != interfaces.end();
}

bool SymbolTable::typeImplementsInterfaceExtended(const std::string& type_name,
                                                  const std::string& interface_name) const {
    // 1. 先查找精确匹配
    if (getInterfaceImpl(type_name, interface_name) != nullptr) {
        return true;
    }
    
    // 2. 检查旧系统（兼容）
    return typeImplementsInterface(type_name, interface_name);
}

std::vector<std::string> SymbolTable::getImplementedInterfaces(const std::string& type_name) const {
    auto it = type_interfaces_.find(type_name);
    if (it == type_interfaces_.end()) {
        return {};
    }
    return it->second;
}

SymbolTable::Symbol* SymbolTable::lookup(const std::string& name, const std::string& current_module) {
    // 1. 先在当前模块查找
    auto module_it = module_symbols_.find(current_module);
    if (module_it != module_symbols_.end()) {
        auto symbol_it = module_it->second.find(name);
        if (symbol_it != module_it->second.end()) {
            return &symbol_it->second;
        }
    }
    
    // 2. 在其他模块中查找公开符号
    for (auto& [module, symbols] : module_symbols_) {
        if (module == current_module) continue;
        
        auto symbol_it = symbols.find(name);
        if (symbol_it != symbols.end() && symbol_it->second.is_public) {
            return &symbol_it->second;
        }
    }
    
    return nullptr;
}

SymbolTable::Symbol* SymbolTable::lookupInModule(const std::string& module, const std::string& name) {
    auto module_it = module_symbols_.find(module);
    if (module_it == module_symbols_.end()) {
        return nullptr;
    }
    
    auto symbol_it = module_it->second.find(name);
    if (symbol_it == module_it->second.end()) {
        return nullptr;
    }
    
    return &symbol_it->second;
}

bool SymbolTable::isAccessible(const Symbol& symbol, const std::string& from_module) const {
    // 同模块内总是可访问
    if (symbol.module == from_module) {
        return true;
    }
    
    // 跨模块需要public
    return symbol.is_public;
}

std::vector<SymbolTable::Symbol*> SymbolTable::getPublicSymbols(const std::string& module) {
    std::vector<Symbol*> result;
    
    auto module_it = module_symbols_.find(module);
    if (module_it == module_symbols_.end()) {
        return result;
    }
    
    for (auto& [name, symbol] : module_it->second) {
        if (symbol.is_public) {
            result.push_back(&symbol);
        }
    }
    
    return result;
}

SymbolTable::TypePattern SymbolTable::parseTypePattern(const std::string& type_name) const {
    TypePattern pattern;
    
    size_t lt_pos = type_name.find('<');
    
    if (lt_pos == std::string::npos) {
        // 没有泛型参数
        pattern.base = type_name;
        pattern.has_generics = false;
        return pattern;
    }
    
    // 有泛型参数
    pattern.base = type_name.substr(0, lt_pos);
    pattern.has_generics = true;
    
    // 提取泛型参数部分
    std::string args_str = type_name.substr(lt_pos + 1);
    if (!args_str.empty() && args_str.back() == '>') {
        args_str = args_str.substr(0, args_str.size() - 1);
    }
    
    // 解析参数（处理嵌套泛型）
    int depth = 0;
    std::string current_arg;
    
    for (char c : args_str) {
        if (c == '<') {
            depth++;
            if (!current_arg.empty() || depth > 1) {
                current_arg += c;
            }
        } else if (c == '>') {
            depth--;
            if (depth >= 0) {
                current_arg += c;
            }
        } else if (c == ',' && depth == 0) {
            if (!current_arg.empty()) {
                pattern.args.push_back(current_arg);
            }
            current_arg = "";
        } else {
            current_arg += c;
        }
    }
    if (!current_arg.empty()) {
        pattern.args.push_back(current_arg);
    }
    
    return pattern;
}

bool SymbolTable::matchesGenericPatternImpl(
    const std::string& concrete_type,
    const std::string& pattern,
    const std::vector<std::string>& constraints
) const {
    // 解析两个类型
    TypePattern concrete_pattern = parseTypePattern(concrete_type);
    TypePattern pattern_parsed = parseTypePattern(pattern);
    
    // 检查 base 是否相同
    if (concrete_pattern.base != pattern_parsed.base) {
        return false;
    }
    
    // 检查参数数量
    if (concrete_pattern.args.size() != pattern_parsed.args.size()) {
        return false;
    }
    
    // 检查约束（如果有）
    if (!constraints.empty()) {
        for (const auto& arg : concrete_pattern.args) {
            if (!checkConstraintsImpl(arg, constraints)) {
                return false;
            }
        }
    }
    
    return true;
}

bool SymbolTable::checkConstraintsImpl(
    const std::string& type_name,
    const std::vector<std::string>& constraints
) const {
    for (const auto& constraint : constraints) {
        if (!typeImplementsInterface(type_name, constraint)) {
            return false;
        }
    }
    return true;
}

void SymbolTable::dump() const {
    std::cout << "\n=== Symbol Table ===\n";
    for (const auto& [module, symbols] : module_symbols_) {
        std::cout << "Module: " << module << "\n";
        for (const auto& [name, symbol] : symbols) {
            std::cout << "  " << (symbol.is_public ? "pub " : "    ")
                     << name << " (";
            switch (symbol.kind) {
                case SymbolKind::Function: std::cout << "fn"; break;
                case SymbolKind::GenericFunction: std::cout << "fn<T>"; break;
                case SymbolKind::Type: std::cout << "type"; break;
                case SymbolKind::Interface: std::cout << "interface"; break;
                case SymbolKind::Variable: std::cout << "var"; break;
            }
            std::cout << ")\n";
        }
    }
    std::cout << "===================\n\n";
}

} // namespace pawc

