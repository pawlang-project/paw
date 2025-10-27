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
#include <unordered_map>
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
    types::Type* getPrimitiveType(types::PrimitiveType::Primitive prim);
    
    /**
     * 创建命名类型
     */
    types::Type* getNamedType(const std::string& name, 
                       std::vector<std::unique_ptr<types::Type>> generic_args = {});
    
    /**
     * 创建泛型类型
     */
    types::Type* getGenericType(const std::string& name);
    
    /**
     * 获取 Self 类型（单例）
     */
    types::Type* getSelfType();
    
    /**
     * 创建数组类型
     */
    types::Type* getArrayType(types::Type* element, int size);
    
    /**
     * 创建切片类型
     */
    types::Type* getSliceType(types::Type* element);
    
    /**
     * 创建元组类型
     */
    types::Type* getTupleType(std::vector<std::unique_ptr<types::Type>> elements);
    
    /**
     * 创建引用类型
     */
    types::Type* getReferenceType(types::Type* pointee, bool is_mutable);
    
    /**
     * 创建可选类型
     */
    types::Type* getOptionalType(types::Type* inner);
    
    /**
     * 创建函数类型
     */
    types::Type* getFunctionType(std::vector<std::unique_ptr<types::Type>> params, 
                          std::unique_ptr<types::Type> return_type);
    
    // ========================================================================
    // 类型查询与比较
    // ========================================================================
    
    /**
     * 类型相等性比较
     */
    bool equals(const types::Type* a, const types::Type* b);
    
    /**
     * 类型是否可赋值（from 可以赋值给 to）
     */
    bool isAssignable(const types::Type* from, const types::Type* to);
    
    /**
     * 获取两个类型的公共类型（如果存在）
     */
    types::Type* commonType(const types::Type* a, const types::Type* b);
    
    /**
     * 是否是子类型关系
     */
    bool isSubtype(const types::Type* sub, const types::Type* super);
    
    /**
     * 获取类型的字符串表示
     */
    std::string toString(const types::Type* type);
    
    // ========================================================================
    // 类型注册
    // ========================================================================
    
    /**
     * 注册用户定义类型
     */
    void registerType(const std::string& name, types::Type* type);
    
    /**
     * 查找类型
     */
    types::Type* lookupType(const std::string& name);
    
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
    std::map<types::PrimitiveType::Primitive, types::Type*> primitive_cache_;
    
    // Self 类型单例
    types::Type* self_type_;
    
    // 类型池（管理所有创建的类型）
    std::vector<std::unique_ptr<types::Type>> type_pool_;
    
    // 命名类型注册表
    std::map<std::string, types::Type*> named_types_;
    
    /**
     * 将类型添加到类型池并返回原始指针
     */
    types::Type* internType(std::unique_ptr<types::Type> type);
    
    /**
     * 辅助方法：克隆类型向量
     */
    std::vector<std::unique_ptr<types::Type>> cloneTypeVector(const std::vector<std::unique_ptr<types::Type>>& types);
    
    // 性能优化：缓存类型字符串表示
    mutable std::unordered_map<const types::Type*, std::string> string_cache_;
    
    // 性能优化：缓存类型比较结果
    struct TypePair {
        const types::Type* a;
        const types::Type* b;
        
        bool operator==(const TypePair& other) const {
            return (a == other.a && b == other.b) || 
                   (a == other.b && b == other.a);  // 交换律
        }
    };
    
    struct TypePairHash {
        size_t operator()(const TypePair& p) const {
            // 确保哈希对称（a,b 和 b,a 相同）
            size_t h1 = std::hash<const void*>()(p.a);
            size_t h2 = std::hash<const void*>()(p.b);
            return h1 < h2 ? (h1 ^ (h2 << 1)) : (h2 ^ (h1 << 1));
        }
    };
    
    mutable std::unordered_map<TypePair, bool, TypePairHash> equals_cache_;
};

} // namespace pawc

#endif // PAWC_TYPE_SYSTEM_H

