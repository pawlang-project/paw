/**
 * @file type_system.h
 * @brief PawLang 类型系统管理器
 * 
 * 提供类型创建、查询、比较等统一接口。
 * 
 * @version 0.3.0-dev (Phase 2)
 * @date 2025-10-27
 */

#ifndef PAWC_TYPE_SYSTEM_H
#define PAWC_TYPE_SYSTEM_H

#include "type.h"
#include <map>
#include <memory>
#include <vector>

namespace pawc {

/**
 * 类型系统管理器
 * 
 * 职责：
 * - 创建和管理所有类型实例
 * - 提供类型比较、转换等操作
 * - 缓存常用类型（如原始类型）
 * - 类型注册表（用户定义类型）
 */
class TypeSystem {
public:
    TypeSystem();
    ~TypeSystem() = default;
    
    // ========================================================================
    // 类型创建
    // ========================================================================
    
    /**
     * 获取原始类型（缓存）
     */
    Type* getPrimitiveType(PrimitiveType::Primitive prim);
    
    /**
     * 创建命名类型
     */
    Type* getNamedType(const std::string& name, 
                       std::vector<std::unique_ptr<Type>> generic_args = {});
    
    /**
     * 创建泛型类型
     */
    Type* getGenericType(const std::string& name);
    
    /**
     * 获取 Self 类型（单例）
     */
    Type* getSelfType();
    
    /**
     * 创建数组类型
     */
    Type* getArrayType(Type* element, int size);
    
    /**
     * 创建切片类型
     */
    Type* getSliceType(Type* element);
    
    /**
     * 创建元组类型
     */
    Type* getTupleType(std::vector<std::unique_ptr<Type>> elements);
    
    /**
     * 创建引用类型
     */
    Type* getReferenceType(Type* pointee, bool is_mutable);
    
    /**
     * 创建可选类型
     */
    Type* getOptionalType(Type* inner);
    
    /**
     * 创建函数类型
     */
    Type* getFunctionType(std::vector<std::unique_ptr<Type>> params, 
                          std::unique_ptr<Type> return_type);
    
    // ========================================================================
    // 类型查询与比较
    // ========================================================================
    
    /**
     * 类型相等性比较
     */
    bool equals(const Type* a, const Type* b);
    
    /**
     * 类型是否可赋值（from 可以赋值给 to）
     */
    bool isAssignable(const Type* from, const Type* to);
    
    /**
     * 获取两个类型的公共类型（如果存在）
     */
    Type* commonType(const Type* a, const Type* b);
    
    /**
     * 是否是子类型关系
     */
    bool isSubtype(const Type* sub, const Type* super);
    
    /**
     * 获取类型的字符串表示
     */
    std::string toString(const Type* type);
    
    // ========================================================================
    // 类型注册
    // ========================================================================
    
    /**
     * 注册用户定义类型
     */
    void registerType(const std::string& name, Type* type);
    
    /**
     * 查找类型
     */
    Type* lookupType(const std::string& name);
    
    /**
     * 是否已注册类型
     */
    bool hasType(const std::string& name) const;
    
    // ========================================================================
    // 工具方法
    // ========================================================================
    
    /**
     * 清空类型池（通常在编译完成后）
     */
    void clear();
    
    /**
     * 获取类型池大小（调试用）
     */
    size_t getTypePoolSize() const { return type_pool_.size(); }
    
private:
    // 原始类型缓存（16 个原始类型）
    std::map<PrimitiveType::Primitive, Type*> primitive_cache_;
    
    // Self 类型单例
    Type* self_type_;
    
    // 类型池（管理所有创建的类型）
    std::vector<std::unique_ptr<Type>> type_pool_;
    
    // 命名类型注册表
    std::map<std::string, Type*> named_types_;
    
    /**
     * 将类型添加到类型池并返回原始指针
     */
    Type* internType(std::unique_ptr<Type> type);
    
    /**
     * 辅助方法：克隆类型向量
     */
    std::vector<std::unique_ptr<Type>> cloneTypeVector(const std::vector<std::unique_ptr<Type>>& types);
};

} // namespace pawc

#endif // PAWC_TYPE_SYSTEM_H

