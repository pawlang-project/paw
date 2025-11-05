//===--- generic_instantiator.cpp - Generic Instantiation Impl ---*- C++ -*-===//
/// @file generic_instantiator.cpp
/// @brief Implementation file

#include "generic_instantiator.h"
#include "mangling.h"
#include "backend/codegen/codegen_context.h"
#include "middleend/types/type.h"

namespace pawc {

GenericInstantiator::GenericInstantiator(CodeGenContext* context)
    : context_(context) {}

std::string GenericInstantiator::instantiateStruct(
    const std::string& base_name,
    const std::vector<Type*>& type_args) {
    
    // useManglinggeneratemonomorphizationname
    return Mangling::generateMonomorphizedName(base_name, type_args);
}

std::string GenericInstantiator::instantiateFunction(
    const std::string& base_name,
    const std::vector<Type*>& type_args) {
    
    // useManglinggeneratemonomorphizationname
    return Mangling::generateMonomorphizedName(base_name, type_args);
}

std::string GenericInstantiator::instantiateEnum(
    const std::string& base_name,
    const std::vector<Type*>& type_args) {
    
    // useManglinggeneratemonomorphizationname
    return Mangling::generateMonomorphizedName(base_name, type_args);
}

} // namespace pawc

