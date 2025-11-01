//===--- symbol_table.cpp - Symbol Table Implementation ----------*- C++ -*-===//

#include "symbol_table.h"
#include "builtin_symbols.h"
#include "middleend/types/type_system.h"

namespace pawc {

SymbolTable::SymbolTable(TypeSystem* type_system)
    : type_system_(type_system) {
    global_scope_ = new Scope(nullptr);
    current_scope_ = global_scope_;
    scopes_.emplace_back(global_scope_);
}

void SymbolTable::enterScope() {
    auto* new_scope = new Scope(current_scope_);
    scopes_.emplace_back(new_scope);
    current_scope_ = new_scope;
}

void SymbolTable::exitScope() {
    if (current_scope_->getParent()) {
        current_scope_ = current_scope_->getParent();
    }
}

void SymbolTable::defineVariable(const std::string& name, Type* type, bool is_mutable) {
    auto* symbol = new VariableSymbol(name, type, is_mutable);
    symbols_.emplace_back(symbol);
    current_scope_->define(name, symbol);
}

void SymbolTable::defineFunction(const std::string& name, FunctionType* type, bool is_builtin) {
    auto* symbol = new FunctionSymbol(name, type, is_builtin);
    symbols_.emplace_back(symbol);
    current_scope_->define(name, symbol);
    
    // 添加到重载函数表
    overloaded_functions_[name].push_back(symbol);
}

void SymbolTable::defineType(const std::string& name, Type* type) {
    auto* symbol = new TypeSymbol(name, type);
    symbols_.emplace_back(symbol);
    current_scope_->define(name, symbol);
}

Symbol* SymbolTable::lookup(const std::string& name) {
    return current_scope_->lookup(name);
}

VariableSymbol* SymbolTable::lookupVariable(const std::string& name) {
    Symbol* sym = lookup(name);
    if (sym && sym->getKind() == Symbol::Kind::Variable) {
        return static_cast<VariableSymbol*>(sym);
    }
    return nullptr;
}

FunctionSymbol* SymbolTable::lookupFunction(const std::string& name) {
    Symbol* sym = lookup(name);
    if (sym && sym->getKind() == Symbol::Kind::Function) {
        return static_cast<FunctionSymbol*>(sym);
    }
    return nullptr;
}

FunctionSymbol* SymbolTable::lookupFunction(const std::string& name, const std::vector<Type*>& arg_types) {
    auto it = overloaded_functions_.find(name);
    if (it == overloaded_functions_.end()) {
        return nullptr;
    }
    
    // 查找匹配的重载
    for (auto* func : it->second) {
        const auto& param_types = func->getParamTypes();
        if (param_types.size() != arg_types.size()) {
            continue;
        }
        
        bool matches = true;
        for (size_t i = 0; i < param_types.size(); ++i) {
            if (!type_system_->equals(param_types[i], arg_types[i])) {
                matches = false;
                break;
            }
        }
        
        if (matches) {
            return func;
        }
    }
    
    return nullptr;
}

void SymbolTable::initializeBuiltins() {
    BuiltinSymbols::registerAll(this, type_system_);
}

} // namespace pawc
