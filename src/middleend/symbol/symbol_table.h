//===--- symbol_table.h - Symbol Table ---------------------------*- C++ -*-===//
//
// PawLang Compiler - Symbol Table
//
//===----------------------------------------------------------------------===//

#ifndef PAW_SYMBOL_TABLE_H
#define PAW_SYMBOL_TABLE_H

#include "middleend/types/type.h"
#include "middleend/types/generic_types.h"  // for FunctionType
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace pawc {

class TypeSystem;

/// Symbol - symbolbaseclass
class Symbol {
public:
    enum class Kind {
        Variable,
        Function,
        Type,
        Module,
    };
    
    Symbol(Kind kind, const std::string& name)
        : kind_(kind), name_(name) {}
    
    virtual ~Symbol() = default;
    
    Kind getKind() const { return kind_; }
    const std::string& getName() const { return name_; }
    
private:
    Kind kind_;
    std::string name_;
};

/// VariableSymbol - variablesymbol
class VariableSymbol : public Symbol {
public:
    VariableSymbol(const std::string& name, Type* type, bool is_mutable)
        : Symbol(Kind::Variable, name), type_(type), is_mutable_(is_mutable) {}
    
    Type* getType() const { return type_; }
    bool isMutable() const { return is_mutable_; }
    
private:
    Type* type_;
    bool is_mutable_;
};

/// FunctionSymbol - functionsymbol
class FunctionSymbol : public Symbol {
public:
    FunctionSymbol(const std::string& name, FunctionType* type, bool is_builtin = false)
        : Symbol(Kind::Function, name), type_(type), is_builtin_(is_builtin) {}
    
    FunctionType* getType() const { return type_; }
    bool isBuiltin() const { return is_builtin_; }
    
    const std::vector<Type*>& getParamTypes() const {
        return type_->getParamTypes();
    }
    
    Type* getReturnType() const {
        return type_->getReturnType();
    }
    
private:
    FunctionType* type_;
    bool is_builtin_;
};

/// TypeSymbol - typessymbol
class TypeSymbol : public Symbol {
public:
    TypeSymbol(const std::string& name, Type* type)
        : Symbol(Kind::Type, name), type_(type) {}
    
    Type* getType() const { return type_; }
    
private:
    Type* type_;
};

/// Scope - scope
class Scope {
public:
    explicit Scope(Scope* parent = nullptr) : parent_(parent) {}
    
    void define(const std::string& name, Symbol* symbol) {
        symbols_[name] = symbol;
    }
    
    Symbol* lookup(const std::string& name, bool search_parent = true) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return it->second;
        }
        if (search_parent && parent_) {
            return parent_->lookup(name, true);
        }
        return nullptr;
    }
    
    Scope* getParent() const { return parent_; }
    
private:
    Scope* parent_;
    std::unordered_map<std::string, Symbol*> symbols_;
};

/// SymbolTable - symboltable
class SymbolTable {
public:
    explicit SymbolTable(TypeSystem* type_system);
    ~SymbolTable() = default;
    
    // scopemanage
    void enterScope();
    void exitScope();
    Scope* getCurrentScope() { return current_scope_; }
    
    // symboldefinition
    void defineVariable(const std::string& name, Type* type, bool is_mutable);
    void defineFunction(const std::string& name, FunctionType* type, bool is_builtin = false);
    void defineType(const std::string& name, Type* type);
    
    // symbollookup
    Symbol* lookup(const std::string& name);
    VariableSymbol* lookupVariable(const std::string& name);
    FunctionSymbol* lookupFunction(const std::string& name);
    
    // overloadfunctionlookup
    FunctionSymbol* lookupFunction(const std::string& name, const std::vector<Type*>& arg_types);
    
    // Builtininitialize
    void initializeBuiltins();
    
private:
    TypeSystem* type_system_;
    Scope* global_scope_;
    Scope* current_scope_;
    std::vector<std::unique_ptr<Scope>> scopes_;
    std::vector<std::unique_ptr<Symbol>> symbols_;
    
    // overloadfunctiontable：name -> vector of FunctionSymbol*
    std::unordered_map<std::string, std::vector<FunctionSymbol*>> overloaded_functions_;
};

} // namespace pawc

#endif // PAW_SYMBOL_TABLE_H
