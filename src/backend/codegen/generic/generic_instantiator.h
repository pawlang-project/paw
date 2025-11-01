//===--- generic_instantiator.h - Generic Instantiation ---------*- C++ -*-===//
//
// 泛型实例化器 - 协调AST单态化和CodeGen
// 桥接MonomorphizationPass和GenericMonomorphization
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

/// GenericInstantiator - 泛型实例化器
///
/// 职责：
/// 1. 协调AST级别的单态化（MonomorphizationPass）
/// 2. 协调LLVM级别的类型映射（GenericMonomorphization）
/// 3. 提供统一的泛型实例化接口
class GenericInstantiator {
    CodeGenContext* context_;
    
public:
    explicit GenericInstantiator(CodeGenContext* context);
    
    /// 实例化泛型结构体
    /// @param base_name 基础名称
    /// @param type_args 类型参数
    /// @return 实例化后的类型名称
    std::string instantiateStruct(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
    
    /// 实例化泛型函数
    /// @param base_name 基础名称
    /// @param type_args 类型参数
    /// @return 实例化后的函数名称
    std::string instantiateFunction(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
    
    /// 实例化泛型枚举
    /// @param base_name 基础名称
    /// @param type_args 类型参数
    /// @return 实例化后的枚举名称
    std::string instantiateEnum(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
};

} // namespace pawc

#endif // PAW_CODEGEN_GENERIC_INSTANTIATOR_H

