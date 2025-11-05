//===--- mangling.cpp - Generic Name Mangling Implementation -----*- C++ -*-===//
/// @file mangling.cpp
/// @brief Implementation file

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
    
    // gettypesof/thecharacterstringtableindicates/show
    std::string type_str = type->toString();
    
    // Escape special characters
    return escapeTypeName(type_str);
}

std::string Mangling::escapeTypeName(const std::string& name) {
    std::string results;
    results.reserve(name.size());
    
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
                // Skip these characters or substitute as underscore
                break;
            default:
                results.push_back(c);
        }
    }
    
    return results;
}

} // namespace pawc

