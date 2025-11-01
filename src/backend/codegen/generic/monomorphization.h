//===--- monomorphization.h - Generic Monomorphization ----------*- C++ -*-===//
//
// 泛型单态化 - LLVM CodeGen级别的泛型实例化
// 将单态化后的AST转换为LLVM IR
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_GENERIC_MONOMORPHIZATION_H
#define PAW_CODEGEN_GENERIC_MONOMORPHIZATION_H

#include <string>
#include <vector>
#include <unordered_map>

namespace llvm {
    class Type;
    class Value;
    class Function;
    class StructType;
}

namespace pawc {

class Type;
class CodeGenContext;

/// GenericMonomorphization - 泛型单态化工具（CodeGen级别）
///
/// 在LLVM IR生成阶段处理泛型类型的实例化
/// 与MonomorphizationPass配合工作：
///   - MonomorphizationPass: AST级别的单态化（编译早期）
///   - GenericMonomorphization: LLVM级别的类型映射（CodeGen阶段）
class GenericMonomorphization {
    CodeGenContext* context_;
    
    // 泛型类型实例缓存
    std::unordered_map<std::string, llvm::Type*> type_cache_;
    std::unordered_map<std::string, llvm::Function*> function_cache_;
    
public:
    explicit GenericMonomorphization(CodeGenContext* context);
    
    /// 将单态化后的类型映射到LLVM类型
    /// @param paw_type 单态化后的PawLang类型
    /// @param instance_name 实例名称 (例如 "Box_i32")
    /// @return LLVM类型
    llvm::Type* mapMonomorphizedType(Type* paw_type, 
                                     const std::string& instance_name);
    
    /// 获取或创建单态化函数
    /// @param instance_name 函数实例名称
    /// @param param_types 参数类型列表
    /// @param return_type 返回类型
    /// @return LLVM函数
    llvm::Function* getOrCreateMonomorphizedFunction(
        const std::string& instance_name,
        const std::vector<llvm::Type*>& param_types,
        llvm::Type* return_type);
    
    /// 清除缓存
    void clearCache();
};

} // namespace pawc

#endif // PAW_CODEGEN_GENERIC_MONOMORPHIZATION_H

