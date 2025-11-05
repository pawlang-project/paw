//===--- generic_template.h - Generic Template Definition -------*- C++ -*-===//
//
// generictemplatedefinition：used forgenerictypesof/theinstantiation
//
//===----------------------------------------------------------------------===//

#ifndef PAW_GENERIC_TEMPLATE_H
#define PAW_GENERIC_TEMPLATE_H

#include <string>
#include <vector>
#include <memory>

namespace pawc {

// Forward declaration - needcompletepath
class EnumDecl;
class StructDecl;

// declarecompletetypes（avoidnotcompletetypeserror）
// #include "frontend/parser/ast/stmt.h"  // containsEnumDecldefinition

/// GenericTemplate - generictemplate
///
/// savegenericdefinitioninfo，used forinstantiationconcretetypes
struct GenericTemplate {
    enum Kind {
        ENUM,
        STRUCT,
        FUNCTION,     // 🔧 G3: genericfunctionsupport
        INTERFACE     // 🔧 G2: genericinterfacesupport
    };
    
    std::string name;                      // "Option", "Result", "identity"
    std::vector<std::string> type_params;  // ["T"], ["T", "E"]
    Kind kind;
    
    // saveoriginalASTdefinition（used forinstantiation）
    union {
        EnumDecl* enum_def;
        StructDecl* struct_def;
        class FunctionDecl* func_def;      // G3
        class InterfaceDecl* interface_def; // G2
    };
    
    // constructfunction
    GenericTemplate(std::string n, std::vector<std::string> params, EnumDecl* def)
        : name(std::move(n)), type_params(std::move(params)), 
          kind(ENUM), enum_def(def) {}
    
    GenericTemplate(std::string n, std::vector<std::string> params, StructDecl* def)
        : name(std::move(n)), type_params(std::move(params)), 
          kind(STRUCT), struct_def(def) {}
    
    GenericTemplate(std::string n, std::vector<std::string> params, class FunctionDecl* def)
        : name(std::move(n)), type_params(std::move(params)), 
          kind(FUNCTION), func_def(def) {}
    
    GenericTemplate(std::string n, std::vector<std::string> params, class InterfaceDecl* def)
        : name(std::move(n)), type_params(std::move(params)), 
          kind(INTERFACE), interface_def(def) {}
    
    // gettypesparametercount
    size_t getParamCount() const { return type_params.size(); }
    
    // Checktypesparameternameyesnohassignificant/effective
    bool hasTypeParam(const std::string& param_name) const {
        for (const auto& p : type_params) {
            if (p == param_name) return true;
        }
        return false;
    }
};

} // namespace pawc

#endif // PAW_GENERIC_TEMPLATE_H

