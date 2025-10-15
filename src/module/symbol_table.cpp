#include "symbol_table.h"
#include <iostream>

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

void SymbolTable::dump() const {
    std::cout << "\n=== Symbol Table ===\n";
    for (const auto& [module, symbols] : module_symbols_) {
        std::cout << "Module: " << module << "\n";
        for (const auto& [name, symbol] : symbols) {
            std::cout << "  " << (symbol.is_public ? "pub " : "    ")
                     << name << " (";
            switch (symbol.kind) {
                case SymbolKind::Function: std::cout << "fn"; break;
                case SymbolKind::Type: std::cout << "type"; break;
                case SymbolKind::Variable: std::cout << "var"; break;
            }
            std::cout << ")\n";
        }
    }
    std::cout << "===================\n\n";
}

} // namespace pawc

