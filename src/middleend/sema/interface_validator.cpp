//===--- interface_validator.cpp - Interface Validation Impl -----*- C++ -*-===//
/// @file interface_validator.cpp
/// @brief Type system and semantic analysis implementation

#include "interface_validator.h"
#include "frontend/parser/ast/stmt.h"
#include "middleend/types/generic_types.h"
#include "middleend/types/composite_types.h"
#include <unordered_set>
#include <iostream>

namespace pawc {

InterfaceValidator::InterfaceValidator(SemanticContext* context)
    : context_(context) {}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 1. validateinterfacedefinition
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool InterfaceValidator::validateInterfaceDecl(InterfaceDecl* decl) {
    auto* diagnostics = context_->getDiagnostics();
    bool valid = true;
    
    // 1. checkinterfacenameyesnoalreadyexists（avoidduplicatedefinition）
    // note：heresimplifyprocess，actualshouldfromsymboltableortypessystemmiddle/centerlookup
    
    // 2. validatemethoddeclaration
    std::unordered_set<std::string> method_names;
    
    for (const auto& method : decl->getMethods()) {
        // 2.1 checkmethodnameduplicate
        if (method_names.find(method.name) != method_names.end()) {
            if (diagnostics) {
                // diagnostics->error("Interface method '" + method.name + 
                //                   "' is defined multiple times in interface '" + 
                //                   decl->getName() + "'");
                std::cerr << "[InterfaceValidator] Error: Method '" << method.name 
                          << "' is defined multiple times in interface '" 
                          << decl->getName() << "'" << std::endl;
            }
            valid = false;
            continue;
        }
        method_names.insert(method.name);
        
        // 2.2 validatemethodparametertypesexists
        for (const auto& param : method.params) {
            if (!param.type) {
                if (diagnostics) {
                    std::cerr << "[InterfaceValidator] Error: Method '" << method.name 
                              << "' in interface '" << decl->getName() 
                              << "' has parameter without type" << std::endl;
                }
                valid = false;
            }
            
            // Check Self typesonlycanappearnowreferencetypesmiddle/center
            if (param.type && param.type->getKind() == Type::Kind::SelfType) {
                if (diagnostics) {
                    std::cerr << "[InterfaceValidator] Error: Self type can only appear "
                              << "as reference (&Self, &~Self) in interface method '" 
                              << method.name << "'" << std::endl;
                }
                valid = false;
            }
        }
        
        // 2.3 validatereturntypesexists
        if (!method.return_type) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Method '" << method.name 
                          << "' in interface '" << decl->getName() 
                          << "' has no return type" << std::endl;
            }
            valid = false;
        }
    }
    
    // 3. validategenericparameter（ifyesgenericinterface）
    if (decl->isGeneric()) {
        std::unordered_set<std::string> generic_param_names;
        for (const auto& param : decl->getGenericParams()) {
            if (generic_param_names.find(param.name) != generic_param_names.end()) {
                if (diagnostics) {
                    std::cerr << "[InterfaceValidator] Error: Generic parameter '" 
                              << param.name << "' is defined multiple times in interface '" 
                              << decl->getName() << "'" << std::endl;
                }
                valid = false;
            }
            generic_param_names.insert(param.name);
        }
    }
    
    return valid;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 2. validate Support implementation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool InterfaceValidator::validateSupportDecl(SupportDecl* decl) {
    auto* diagnostics = context_->getDiagnostics();
    auto* type_system = context_->getTypeSystem();
    bool valid = true;
    
    // 1. checkimplementationof/thetypesyesnoexists
    Type* impl_type = type_system->lookupType(decl->getTypeName());
    if (!impl_type) {
        if (diagnostics) {
            std::cerr << "[InterfaceValidator] Error: Type '" << decl->getTypeName() 
                      << "' not found in support declaration" << std::endl;
        }
        return false;
    }
    
    // 2. checkinterfaceyesnoexists
    // note：actualshouldfromtypessystemorsymboltablemiddle/centerlookupinterfacedefinition
    // heresimplifyprocess，assumptioninterfaceexists
    
    // 3. getinterfacedefinition（simplifyversion - actualneedfromcontextmiddle/centerget）
    // TODO: fromtypessystemorsymboltablemiddle/centerlookupinterfacedefinition
    // Currently first assume interface definition exists, only validate basic method signature correctness
    
    // 4. validateimplementationof/themethod
    std::unordered_set<std::string> impl_method_names;
    
    for (const auto& method : decl->getMethods()) {
        const std::string& method_name = method->getName();
        
        // 4.1 checkmethodnameduplicate
        if (impl_method_names.find(method_name) != impl_method_names.end()) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Method '" << method_name 
                          << "' is implemented multiple times in support for '" 
                          << decl->getTypeName() << "'" << std::endl;
            }
            valid = false;
            continue;
        }
        impl_method_names.insert(method_name);
        
        // 4.2 validatemethodhasimplementationbody/struct
        if (!method->getBody()) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Method '" << method_name 
                          << "' in support for '" << decl->getTypeName() 
                          << "' must have a body" << std::endl;
            }
            valid = false;
        }
        
        // 4.3 validatemethodparameterandreturntypes
        for (const auto& param : method->getParams()) {
            if (!param.type) {
                if (diagnostics) {
                    std::cerr << "[InterfaceValidator] Error: Parameter in method '" 
                              << method_name << "' has no type" << std::endl;
                }
                valid = false;
            }
        }
        
        if (!method->getReturnType()) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Method '" << method_name 
                          << "' has no return type" << std::endl;
            }
            valid = false;
        }
    }
    
    // 5. validate where constraint
    for (const auto& clause : decl->getWhereClauses()) {
        // Checkconstraintof/thetypesparameteryesnoin/atgenericparametermiddle/center
        bool found = false;
        for (const auto& param : decl->getTypeGenericParams()) {
            if (param.name == clause.type_param) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Type parameter '" 
                          << clause.type_param << "' in where clause is not declared "
                          << "in support for '" << decl->getTypeName() << "'" << std::endl;
            }
            valid = false;
        }
    }
    
    return valid;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 3. checktypesyesnoimplementationimplementedinterface
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool InterfaceValidator::checkImplementsInterface(Type* type, const std::string& interface_name) {
    if (!type) return false;
    
    // TODO: implementationcompleteof/theinterfaceimplementationcheck
    // currentsimplifyversion：assumptioniftypesexistsandinterfaceexists，assumeis/aspossiblyimplementationimplemented
    // actualneed：
    // 1. lookupshouldtypesof/theAll Support declaration
    // 2. checkyesnohas support Type with Interface of/thedeclaration
    // 3. validateAllrequiredneedof/themethodallalreadyimplementation
    
    auto* type_system = context_->getTypeSystem();
    
    // Checkinterfacetypesyesnoexists
    Type* interface_type = type_system->lookupType(interface_name);
    if (!interface_type || interface_type->getKind() != Type::Kind::Interface) {
        return false;
    }
    
    // simplifyimplementation：alwaysyesreturn true
    // actualimplementationneedcheck Support declaration
    return true;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 4. validatemethodsignatureyesnomatch
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool InterfaceValidator::validateMethodSignature(
    const std::string& impl_method_name,
    const std::vector<Type*>& impl_param_types,
    Type* impl_return_type,
    const std::string& interface_method_name,
    const std::vector<Type*>& interface_param_types,
    Type* interface_return_type) {
    
    auto* diagnostics = context_->getDiagnostics();
    bool valid = true;
    
    // 1. checkmethodnameyesnomatch
    if (impl_method_name != interface_method_name) {
        if (diagnostics) {
            std::cerr << "[InterfaceValidator] Error: Method name mismatch: expected '" 
                      << interface_method_name << "', got '" << impl_method_name << "'" 
                      << std::endl;
        }
        return false;
    }
    
    // 2. checkparametercountyesnomatch
    if (impl_param_types.size() != interface_param_types.size()) {
        if (diagnostics) {
            std::cerr << "[InterfaceValidator] Error: Method '" << impl_method_name 
                      << "' has " << impl_param_types.size() << " parameters, "
                      << "but interface requires " << interface_param_types.size() 
                      << " parameters" << std::endl;
        }
        return false;
    }
    
    // 3. checkeach/everyparametertypesyesnomatch
    for (size_t i = 0; i < impl_param_types.size(); i++) {
        Type* impl_type = impl_param_types[i];
        Type* interface_type = interface_param_types[i];
        
        if (!impl_type || !interface_type) {
            valid = false;
            continue;
        }
        
        // process Self types：in/atimplementationmiddle/center Self willby/passive markersubstitutionis/asactualtypes
        // hereneedspecialhandle
        if (interface_type->getKind() == Type::Kind::SelfType) {
            // interfacein Self in/atimplementationmiddle/centershouldby/passive markersubstitutionis/asactualtypes
            continue;  // simplifyprocess，skip Self typescheck
        }
        
        // processreferencetypesin Self
        if (interface_type->getKind() == Type::Kind::Reference) {
            auto* ref_type = static_cast<ReferenceType*>(interface_type);
            if (ref_type->getPointeeType()->getKind() == Type::Kind::SelfType) {
                // &Self or &~Self
                continue;  // simplifyprocess
            }
        }
        
        // Checktypesyesnoequal
        if (!impl_type->equals(interface_type)) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Method '" << impl_method_name 
                          << "' parameter " << i << " type mismatch: expected '" 
                          << interface_type->toString() << "', got '" 
                          << impl_type->toString() << "'" << std::endl;
            }
            valid = false;
        }
    }
    
    // 4. checkreturntypesyesnomatch
    if (!impl_return_type || !interface_return_type) {
        return false;
    }
    
    // process Self types
    if (interface_return_type->getKind() == Type::Kind::SelfType) {
        // return Self in/atimplementationmiddle/centershouldby/passive markersubstitutionis/asactualtypes
        // simplifyprocess，skipcheck
        return valid;
    }
    
    if (!impl_return_type->equals(interface_return_type)) {
        if (diagnostics) {
            std::cerr << "[InterfaceValidator] Error: Method '" << impl_method_name 
                      << "' return type mismatch: expected '" 
                      << interface_return_type->toString() << "', got '" 
                      << impl_return_type->toString() << "'" << std::endl;
        }
        valid = false;
    }
    
    return valid;
}

} // namespace pawc

