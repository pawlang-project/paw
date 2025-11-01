//===--- interface_validator.cpp - Interface Validation Impl -----*- C++ -*-===//

#include "interface_validator.h"
#include "frontend/parser/ast/stmt.h"

namespace pawc {

InterfaceValidator::InterfaceValidator(SemanticContext* context)
    : context_(context) {}

bool InterfaceValidator::validateInterfaceDecl(InterfaceDecl* decl) {
    // 具体实现从TypeChecker中提取
    // TODO: 验证接口定义
    return true;
}

bool InterfaceValidator::validateSupportDecl(SupportDecl* decl) {
    // 具体实现从TypeChecker中提取
    // TODO: 验证support实现
    return true;
}

bool InterfaceValidator::checkImplementsInterface(Type* type, const std::string& interface_name) {
    // 具体实现从TypeChecker中提取
    // TODO: 检查类型是否实现接口
    return true;
}

bool InterfaceValidator::validateMethodSignature(
    const std::string& impl_method_name,
    const std::vector<Type*>& impl_param_types,
    Type* impl_return_type,
    const std::string& interface_method_name,
    const std::vector<Type*>& interface_param_types,
    Type* interface_return_type) {
    
    // 具体实现从TypeChecker中提取
    // TODO: 验证方法签名
    return true;
}

} // namespace pawc

