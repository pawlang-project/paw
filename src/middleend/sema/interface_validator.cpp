//===--- interface_validator.cpp - Interface Validation Impl -----*- C++ -*-===//

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
// 1. 验证接口定义
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool InterfaceValidator::validateInterfaceDecl(InterfaceDecl* decl) {
    auto* diagnostics = context_->getDiagnostics();
    bool valid = true;
    
    // 1. 检查接口名称是否已存在（避免重复定义）
    // 注：这里简化处理，实际应该从符号表或类型系统中查找
    
    // 2. 验证方法声明
    std::unordered_set<std::string> method_names;
    
    for (const auto& method : decl->getMethods()) {
        // 2.1 检查方法名重复
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
        
        // 2.2 验证方法参数类型存在
        for (const auto& param : method.params) {
            if (!param.type) {
                if (diagnostics) {
                    std::cerr << "[InterfaceValidator] Error: Method '" << method.name 
                              << "' in interface '" << decl->getName() 
                              << "' has parameter without type" << std::endl;
                }
                valid = false;
            }
            
            // 检查 Self 类型只能出现在引用类型中
            if (param.type && param.type->getKind() == Type::Kind::SelfType) {
                if (diagnostics) {
                    std::cerr << "[InterfaceValidator] Error: Self type can only appear "
                              << "as reference (&Self, &~Self) in interface method '" 
                              << method.name << "'" << std::endl;
                }
                valid = false;
            }
        }
        
        // 2.3 验证返回类型存在
        if (!method.return_type) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Method '" << method.name 
                          << "' in interface '" << decl->getName() 
                          << "' has no return type" << std::endl;
            }
            valid = false;
        }
    }
    
    // 3. 验证泛型参数（如果是泛型接口）
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
// 2. 验证 Support 实现
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool InterfaceValidator::validateSupportDecl(SupportDecl* decl) {
    auto* diagnostics = context_->getDiagnostics();
    auto* type_system = context_->getTypeSystem();
    bool valid = true;
    
    // 1. 检查实现的类型是否存在
    Type* impl_type = type_system->lookupType(decl->getTypeName());
    if (!impl_type) {
        if (diagnostics) {
            std::cerr << "[InterfaceValidator] Error: Type '" << decl->getTypeName() 
                      << "' not found in support declaration" << std::endl;
        }
        return false;
    }
    
    // 2. 检查接口是否存在
    // 注：实际应该从类型系统或符号表中查找接口定义
    // 这里简化处理，假设接口存在
    
    // 3. 获取接口定义（简化版 - 实际需要从上下文中获取）
    // TODO: 从类型系统或符号表中查找接口定义
    // 目前先假设接口定义存在，只验证方法签名的基本正确性
    
    // 4. 验证实现的方法
    std::unordered_set<std::string> impl_method_names;
    
    for (const auto& method : decl->getMethods()) {
        const std::string& method_name = method->getName();
        
        // 4.1 检查方法名重复
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
        
        // 4.2 验证方法有实现体
        if (!method->getBody()) {
            if (diagnostics) {
                std::cerr << "[InterfaceValidator] Error: Method '" << method_name 
                          << "' in support for '" << decl->getTypeName() 
                          << "' must have a body" << std::endl;
            }
            valid = false;
        }
        
        // 4.3 验证方法参数和返回类型
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
    
    // 5. 验证 where 约束
    for (const auto& clause : decl->getWhereClauses()) {
        // 检查约束的类型参数是否在泛型参数中
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
// 3. 检查类型是否实现了接口
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool InterfaceValidator::checkImplementsInterface(Type* type, const std::string& interface_name) {
    if (!type) return false;
    
    // TODO: 实现完整的接口实现检查
    // 当前简化版本：假设如果类型存在且接口存在，就认为可能实现了
    // 实际需要：
    // 1. 查找该类型的所有 Support 声明
    // 2. 检查是否有 support Type with Interface 的声明
    // 3. 验证所有必需的方法都已实现
    
    auto* type_system = context_->getTypeSystem();
    
    // 检查接口类型是否存在
    Type* interface_type = type_system->lookupType(interface_name);
    if (!interface_type || interface_type->getKind() != Type::Kind::Interface) {
        return false;
    }
    
    // 简化实现：总是返回 true
    // 实际实现需要检查 Support 声明
    return true;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 4. 验证方法签名是否匹配
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
    
    // 1. 检查方法名是否匹配
    if (impl_method_name != interface_method_name) {
        if (diagnostics) {
            std::cerr << "[InterfaceValidator] Error: Method name mismatch: expected '" 
                      << interface_method_name << "', got '" << impl_method_name << "'" 
                      << std::endl;
        }
        return false;
    }
    
    // 2. 检查参数数量是否匹配
    if (impl_param_types.size() != interface_param_types.size()) {
        if (diagnostics) {
            std::cerr << "[InterfaceValidator] Error: Method '" << impl_method_name 
                      << "' has " << impl_param_types.size() << " parameters, "
                      << "but interface requires " << interface_param_types.size() 
                      << " parameters" << std::endl;
        }
        return false;
    }
    
    // 3. 检查每个参数类型是否匹配
    for (size_t i = 0; i < impl_param_types.size(); i++) {
        Type* impl_type = impl_param_types[i];
        Type* interface_type = interface_param_types[i];
        
        if (!impl_type || !interface_type) {
            valid = false;
            continue;
        }
        
        // 处理 Self 类型：在实现中 Self 会被替换为实际类型
        // 这里需要特殊处理
        if (interface_type->getKind() == Type::Kind::SelfType) {
            // 接口中的 Self 在实现中应该被替换为实际类型
            continue;  // 简化处理，跳过 Self 类型检查
        }
        
        // 处理引用类型中的 Self
        if (interface_type->getKind() == Type::Kind::Reference) {
            auto* ref_type = static_cast<ReferenceType*>(interface_type);
            if (ref_type->getPointeeType()->getKind() == Type::Kind::SelfType) {
                // &Self 或 &~Self
                continue;  // 简化处理
            }
        }
        
        // 检查类型是否相等
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
    
    // 4. 检查返回类型是否匹配
    if (!impl_return_type || !interface_return_type) {
        return false;
    }
    
    // 处理 Self 类型
    if (interface_return_type->getKind() == Type::Kind::SelfType) {
        // 返回 Self 在实现中应该被替换为实际类型
        // 简化处理，跳过检查
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

