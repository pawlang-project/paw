//===--- type_registry.cpp - Type Registry Implementation -------*- C++ -*-===//

#include "type_registry.h"
#include "composite_types.h"
#include "generic_types.h"

namespace pawc {

void TypeRegistry::registerStruct(const std::string& name, StructType* type) {
    struct_types_[name] = type;
    named_types_[name] = type;
}

void TypeRegistry::registerEnum(const std::string& name, EnumType* type) {
    enum_types_[name] = type;
    named_types_[name] = type;
}

void TypeRegistry::registerInterface(const std::string& name, InterfaceType* type) {
    interface_types_[name] = type;
    named_types_[name] = type;
}

void TypeRegistry::registerType(const std::string& name, Type* type) {
    named_types_[name] = type;
}

Type* TypeRegistry::lookup(const std::string& name) const {
    auto it = named_types_.find(name);
    return it != named_types_.end() ? it->second : nullptr;
}

StructType* TypeRegistry::lookupStruct(const std::string& name) const {
    auto it = struct_types_.find(name);
    return it != struct_types_.end() ? it->second : nullptr;
}

EnumType* TypeRegistry::lookupEnum(const std::string& name) const {
    auto it = enum_types_.find(name);
    return it != enum_types_.end() ? it->second : nullptr;
}

InterfaceType* TypeRegistry::lookupInterface(const std::string& name) const {
    auto it = interface_types_.find(name);
    return it != interface_types_.end() ? it->second : nullptr;
}

bool TypeRegistry::exists(const std::string& name) const {
    return named_types_.find(name) != named_types_.end();
}

} // namespace pawc

