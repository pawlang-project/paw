//===--- generic_template.h - Generic Template Definition -------*- C++ -*-===//
//
// 泛型模板定义：用于泛型类型的实例化
//
//===----------------------------------------------------------------------===//

#ifndef PAW_GENERIC_TEMPLATE_H
#define PAW_GENERIC_TEMPLATE_H

#include <string>
#include <vector>
#include <memory>

namespace pawc {

// 前向声明 - 需要完整路径
class EnumDecl;
class StructDecl;

// 声明完整类型（避免不完整类型错误）
// #include "frontend/parser/ast/stmt.h"  // 包含EnumDecl定义

/// GenericTemplate - 泛型模板
///
/// 保存泛型定义信息，用于实例化具体类型
struct GenericTemplate {
    enum Kind {
        ENUM,
        STRUCT,
        // 未来可以添加：FUNCTION, INTERFACE等
    };
    
    std::string name;                      // "Option", "Result"
    std::vector<std::string> type_params;  // ["T"], ["T", "E"]
    Kind kind;
    
    // 保存原始AST定义（用于实例化）
    union {
        EnumDecl* enum_def;
        StructDecl* struct_def;
    };
    
    // 构造函数
    GenericTemplate(std::string n, std::vector<std::string> params, EnumDecl* def)
        : name(std::move(n)), type_params(std::move(params)), 
          kind(ENUM), enum_def(def) {}
    
    GenericTemplate(std::string n, std::vector<std::string> params, StructDecl* def)
        : name(std::move(n)), type_params(std::move(params)), 
          kind(STRUCT), struct_def(def) {}
    
    // 获取类型参数数量
    size_t getParamCount() const { return type_params.size(); }
    
    // 检查类型参数名称是否有效
    bool hasTypeParam(const std::string& param_name) const {
        for (const auto& p : type_params) {
            if (p == param_name) return true;
        }
        return false;
    }
};

} // namespace pawc

#endif // PAW_GENERIC_TEMPLATE_H

