//===--- interface_validator.h - Interface Validation -----------*- C++ -*-===//
//
// interfacevalidate - validateinterfaceimplementationof/thecorrectperformance
//
//===----------------------------------------------------------------------===//

#ifndef PAW_INTERFACE_VALIDATOR_H
#define PAW_INTERFACE_VALIDATOR_H

#include "semantic_context.h"
#include <string>

namespace pawc {

class SupportDecl;
class InterfaceDecl;
class Type;

/// InterfaceValidator - interfacevalidatelinker/er
///
/// negativeresponsiblevalidate：
/// - interfacedefinitionof/thecorrectperformance
/// - supportimplementationyesnosatisfyinterfacerequirement
/// - methodsignatureyesnomatch
class InterfaceValidator {
    SemanticContext* context_;
    
public:
    explicit InterfaceValidator(SemanticContext* context);
    
    /// validateinterfacedefinition
    bool validateInterfaceDecl(InterfaceDecl* decl);
    
    /// validatesupportimplementation
    bool validateSupportDecl(SupportDecl* decl);
    
    /// validatetypesyesnoimplementationimplementedinterface
    bool checkImplementsInterface(Type* type, const std::string& interface_name);
    
    /// validatemethodsignatureyesnomatch
    bool validateMethodSignature(
        const std::string& impl_method_name,
        const std::vector<Type*>& impl_param_types,
        Type* impl_return_type,
        const std::string& interface_method_name,
        const std::vector<Type*>& interface_param_types,
        Type* interface_return_type);
};

} // namespace pawc

#endif // PAW_INTERFACE_VALIDATOR_H

