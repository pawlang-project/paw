//===--- generic_instantiator.h - Generic Instantiation ---------*- C++ -*-===//
//
// genericinstantiationlinker/er - coordinateASTmonomorphizationandCodeGen
// Bridge between MonomorphizationPass and GenericMonomorphization
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_GENERIC_INSTANTIATOR_H
#define PAW_CODEGEN_GENERIC_INSTANTIATOR_H

#include <string>
#include <vector>
#include <memory>

namespace pawc {

class Type;
class CodeGenContext;

/// GenericInstantiator - genericinstantiationlinker/er
///
/// responsibilities：
/// 1. coordinateASTlevelof/themonomorphization（MonomorphizationPass）
/// 2. coordinateLLVMlevelof/thetypesmap（GenericMonomorphization）
/// 3. Provide unified generic instantiation interface
class GenericInstantiator {
    CodeGenContext* context_;
    
public:
    explicit GenericInstantiator(CodeGenContext* context);
    
    /// instantiationgenericstructbody/struct
    /// @param base_name basename
    /// @param type_args typesparameter
    /// @return instantiationback/afterof/thetypesname
    std::string instantiateStruct(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
    
    /// instantiationgenericfunction
    /// @param base_name basename
    /// @param type_args typesparameter
    /// @return Instantiated function name
    std::string instantiateFunction(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
    
    /// instantiationgenericenum
    /// @param base_name basename
    /// @param type_args typesparameter
    /// @return instantiationback/afterof/theenumname
    std::string instantiateEnum(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
};

} // namespace pawc

#endif // PAW_CODEGEN_GENERIC_INSTANTIATOR_H

