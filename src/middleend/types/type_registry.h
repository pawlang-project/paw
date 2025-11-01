//===--- type_registry.h - Type Registration and Lookup ---------*- C++ -*-===//
//
// 类型注册表：管理所有用户定义类型的注册和查找
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_REGISTRY_H
#define PAW_TYPE_REGISTRY_H

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace pawc {

class Type;
class StructType;
class EnumType;
class InterfaceType;

/// TypeRegistry - 类型注册表
///
/// 管理用户定义类型的注册、查找和生命周期
class TypeRegistry {
    // 类型存储（所有权管理）
    std::vector<std::unique_ptr<Type>> type_pool_;
    
    // 名称到类型的映射
    std::unordered_map<std::string, Type*> named_types_;
    
    // 特定类型的快速查找
    std::unordered_map<std::string, StructType*> struct_types_;
    std::unordered_map<std::string, EnumType*> enum_types_;
    std::unordered_map<std::string, InterfaceType*> interface_types_;
    
public:
    TypeRegistry() = default;
    ~TypeRegistry() = default;
    
    /// 注册结构体类型
    void registerStruct(const std::string& name, StructType* type);
    
    /// 注册枚举类型
    void registerEnum(const std::string& name, EnumType* type);
    
    /// 注册接口类型
    void registerInterface(const std::string& name, InterfaceType* type);
    
    /// 通用类型注册
    void registerType(const std::string& name, Type* type);
    
    /// 查找类型
    Type* lookup(const std::string& name) const;
    
    /// 查找结构体
    StructType* lookupStruct(const std::string& name) const;
    
    /// 查找枚举
    EnumType* lookupEnum(const std::string& name) const;
    
    /// 查找接口
    InterfaceType* lookupInterface(const std::string& name) const;
    
    /// 检查类型是否存在
    bool exists(const std::string& name) const;
    
    /// 添加类型到pool（所有权转移）
    template<typename T>
    T* addToPool(std::unique_ptr<T> type) {
        T* ptr = type.get();
        type_pool_.push_back(std::move(type));
        return ptr;
    }
};

} // namespace pawc

#endif // PAW_TYPE_REGISTRY_H

