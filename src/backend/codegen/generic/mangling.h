//===--- mangling.h - Generic Name Mangling ----------------------*- C++ -*-===//
//
// 泛型名称修饰（Name Mangling）
// 为泛型实例生成唯一的、可读的名称
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_GENERIC_MANGLING_H
#define PAW_CODEGEN_GENERIC_MANGLING_H

#include <string>
#include <vector>

namespace pawc {

class Type;

/// Mangling - 泛型名称修饰工具
///
/// 将泛型名称和类型参数转换为单态化后的唯一名称
/// 例如: Box<i32> -> Box_i32
///       Pair<i32, string> -> Pair_i32_string
class Mangling {
public:
    /// 生成单态化名称
    /// @param base_name 基础名称 (例如 "Box", "identity")
    /// @param type_args 类型参数列表
    /// @return 单态化后的名称 (例如 "Box_i32")
    static std::string generateMonomorphizedName(
        const std::string& base_name,
        const std::vector<Type*>& type_args);
    
    /// 将类型转换为字符串表示（用于名称修饰）
    /// @param type 类型
    /// @return 类型的字符串表示
    static std::string typeToString(Type* type);
    
private:
    /// 转义特殊字符（用于名称中的特殊类型）
    static std::string escapeTypeName(const std::string& name);
};

} // namespace pawc

#endif // PAW_CODEGEN_GENERIC_MANGLING_H

