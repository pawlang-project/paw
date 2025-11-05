//===--- type_registry.h - Type Registration and Lookup ---------*- C++ -*-===//
//
// typeregistertable：Manage all user-defined type registration and lookup
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_REGISTRY_H
#define PAW_TYPE_REGISTRY_H

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace pawc {

class Type;
class StructType;
class EnumType;
class InterfaceType;

/// TypeRegistry - typesregistertable
///
/// Manage user-defined type registration, lookup and lifetime
class TypeRegistry {
    // typestorage（Allownershipmanage）
    std::vector<std::unique_ptr<Type>> type_pool_;
    
    // nametotypesof/themap
    std::unordered_map<std::string, Type*> named_types_;
    
    // Fast lookup of specific types
    std::unordered_map<std::string, StructType*> struct_types_;
    std::unordered_map<std::string, EnumType*> enum_types_;
    std::unordered_map<std::string, InterfaceType*> interface_types_;
    
public:
    TypeRegistry() = default;
    ~TypeRegistry() = default;
    
    /// registerstructbody/structtypes
    void registerStruct(const std::string& name, StructType* type);
    
    /// registerenumtypes
    void registerEnum(const std::string& name, EnumType* type);
    
    /// registerinterfacetypes
    void registerInterface(const std::string& name, InterfaceType* type);
    
    /// generic/commontypesregister
    void registerType(const std::string& name, Type* type);
    
    /// lookuptypes
    Type* lookup(const std::string& name) const;
    
    /// lookupstructbody/struct
    StructType* lookupStruct(const std::string& name) const;
    
    /// lookupenum
    EnumType* lookupEnum(const std::string& name) const;
    
    /// lookupinterface
    InterfaceType* lookupInterface(const std::string& name) const;
    
    /// Checktypesyesnoexists
    bool exists(const std::string& name) const;
    
    /// Add types to pool (with ownership transfer)
    template<typename T>
    T* addToPool(std::unique_ptr<T> type) {
        T* ptr = type.get();
        type_pool_.push_back(std::move(type));
        return ptr;
    }
};

} // namespace pawc

#endif // PAW_TYPE_REGISTRY_H

