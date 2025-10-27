/**
 * @file interface_validator.h
 * @brief 接口实现验证器
 * 
 * 负责验证类型是否正确实现了接口。
 * 
 * @version 0.3.0-dev (Phase 3)
 * @date 2025-10-27
 */

#ifndef PAWC_INTERFACE_VALIDATOR_H
#define PAWC_INTERFACE_VALIDATOR_H

#include "../parser/ast.h"
#include "../types/type_system.h"
#include "../diagnostics/diagnostic_engine.h"
#include "../module/symbol_table.h"
#include <map>
#include <string>

namespace pawc {

/**
 * 接口验证器
 * 
 * 验证类型（struct）是否正确实现了声明的接口。
 * 包括：
 * - 方法完整性检查
 * - 参数数量验证
 * - 参数类型验证
 * - 返回类型验证
 * - 默认方法支持
 */
class InterfaceValidator {
public:
    InterfaceValidator(TypeSystem* type_system,
                      DiagnosticEngine* diagnostics,
                      const std::map<std::string, const InterfaceStmt*>& interface_defs);
    
    /**
     * 验证类型的接口实现
     * 
     * @param type_name 类型名称
     * @param interface_name 接口名称
     * @param methods 实现的方法列表
     * @param location 实现位置（用于错误报告）
     * @return 是否验证通过
     */
    bool validateImpl(const std::string& type_name,
                     const std::string& interface_name,
                     const std::vector<std::unique_ptr<FunctionStmt>>& methods,
                     const SourceLocation& location);
    
private:
    TypeSystem* type_system_;
    DiagnosticEngine* diagnostics_;
    const std::map<std::string, const InterfaceStmt*>& interface_defs_;
    std::string current_impl_type_;  // 当前正在验证的类型名（用于 Self 解析）
    
    /**
     * 比较两个 AST 类型是否相等
     */
    bool compareTypes(const Type* a, const Type* b);
    
    /**
     * 获取类型的字符串表示
     */
    std::string typeToString(const Type* type);
};

} // namespace pawc

#endif // PAWC_INTERFACE_VALIDATOR_H

