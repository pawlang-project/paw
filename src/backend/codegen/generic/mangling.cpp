//===--- mangling.cpp - Generic Name Mangling Implementation -----*- C++ -*-===//

#include "mangling.h"
#include "middleend/types/type.h"
#include <sstream>

namespace pawc {

std::string Mangling::generateMonomorphizedName(
    const std::string& base_name,
    const std::vector<Type*>& type_args) {
    
    if (type_args.empty()) {
        return base_name;
    }
    
    std::ostringstream oss;
    oss << base_name;
    
    for (auto* type : type_args) {
        oss << "_" << typeToString(type);
    }
    
    return oss.str();
}

std::string Mangling::typeToString(Type* type) {
    if (!type) {
        return "unknown";
    }
    
    // 获取类型的字符串表示
    std::string type_str = type->toString();
    
    // 转义特殊字符
    return escapeTypeName(type_str);
}

std::string Mangling::escapeTypeName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    
    for (char c : name) {
        switch (c) {
            case '<':
            case '>':
            case ',':
            case ' ':
            case '[':
            case ']':
            case '(':
            case ')':
            case '&':
            case '*':
            case '?':
            case '!':
                // 跳过这些字符或替换为下划线
                break;
            default:
                result.push_back(c);
        }
    }
    
    return result;
}

} // namespace pawc

