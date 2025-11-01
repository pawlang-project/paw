//===--- generic_instantiator.cpp - Generic Instantiation Impl ---*- C++ -*-===//

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
    
    // 使用Mangling生成单态化名称
    return Mangling::generateMonomorphizedName(base_name, type_args);
}

std::string GenericInstantiator::instantiateFunction(
    const std::string& base_name,
    const std::vector<Type*>& type_args) {
    
    // 使用Mangling生成单态化名称
    return Mangling::generateMonomorphizedName(base_name, type_args);
}

std::string GenericInstantiator::instantiateEnum(
    const std::string& base_name,
    const std::vector<Type*>& type_args) {
    
    // 使用Mangling生成单态化名称
    return Mangling::generateMonomorphizedName(base_name, type_args);
}

} // namespace pawc

