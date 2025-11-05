//===--- mangling.h - Generic Name Mangling ----------------------*- C++ -*-===//
//
// genericnamequalify（Name Mangling）
// is/asgenericinstancegenerateuniqueof/the、canreadableof/thename
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_GENERIC_MANGLING_H
#define PAW_CODEGEN_GENERIC_MANGLING_H

#include <string>
#include <vector>

namespace pawc {

class Type;

/// Mangling - genericnamequalifyutility
///
/// willgenericnameandtypesparameterconvertis/asmonomorphizationback/afterof/theuniquename
/// e.g./for example: Box<i32> -> Box_i32
///       Pair<i32, string> -> Pair_i32_string
class Mangling {
public:
    /// generationmonomorphizationname
    /// @param base_name basename (e.g./for example "Box", "identity")
    /// @param type_args typesparameterlist
    /// @return monomorphizationback/afterof/thename (e.g./for example "Box_i32")
    static std::string generateMonomorphizedName(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
    
    /// willtypesconvertis/ascharacterstringtableindicates/show（used fornamequalify）
    /// @param type types
    /// @return typesof/thecharacterstringtableindicates/show
    static std::string typeToString(Type* type);
    
private:
    /// Escape special characters (used for special type names)
    static std::string escapeTypeName(const std::string& name);
};

} // namespace pawc

#endif // PAW_CODEGEN_GENERIC_MANGLING_H

