//===--- interface_validator.h - Interface Validation -----------*- C++ -*-===//
//
// 接口验证 - 验证接口实现的正确性
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

/// InterfaceValidator - 接口验证器
///
/// 负责验证：
/// - 接口定义的正确性
/// - support实现是否满足接口要求
/// - 方法签名是否匹配
class InterfaceValidator {
    SemanticContext* context_;
    
public:
    explicit InterfaceValidator(SemanticContext* context);
    
    /// 验证接口定义
    bool validateInterfaceDecl(InterfaceDecl* decl);
    
    /// 验证support实现
    bool validateSupportDecl(SupportDecl* decl);
    
    /// 验证类型是否实现了接口
    bool checkImplementsInterface(Type* type, const std::string& interface_name);
    
    /// 验证方法签名是否匹配
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

