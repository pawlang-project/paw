//===--- type_system.h - Type System Manager ---------------------*- C++ -*-===//
//
// PawLang Compiler - Type System
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_SYSTEM_H
#define PAW_TYPE_SYSTEM_H

#include "type.h"
#include "primitive_types.h"
#include "composite_types.h"
#include "generic_types.h"
#include "type_registry.h"
#include "utils/arena.h"
#include <unordered_map>
#include <string>

namespace pawc {

/// TypeSystem - 类型系统管理器
///
/// 职责：
/// - 创建和管理所有类型
/// - 类型查找和注册
/// - 类型兼容性检查
class TypeSystem {
public:
    TypeSystem();
    ~TypeSystem() = default;
    
    // Non-copyable
    TypeSystem(const TypeSystem&) = delete;
    TypeSystem& operator=(const TypeSystem&) = delete;
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 基础类型获取（18种）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    Type* getI8Type();
    Type* getI16Type();
    Type* getI32Type();
    Type* getI64Type();
    Type* getI128Type();
    
    Type* getU8Type();
    Type* getU16Type();
    Type* getU32Type();
    Type* getU64Type();
    Type* getU128Type();
    
    Type* getF8Type();
    Type* getF16Type();
    Type* getF32Type();
    Type* getF64Type();
    Type* getF128Type();
    
    Type* getBoolType();
    Type* getCharType();
    Type* getStringType();
    Type* getVoidType();
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 复合类型创建
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    ArrayType* getArrayType(Type* element, size_t size);
    SliceType* getSliceType(Type* element);
    TupleType* getTupleType(std::vector<Type*> elements);
    
    OptionalType* getOptionalType(Type* inner);
    ResultType* getResultType(Type* ok_type);
    
    ReferenceType* getReferenceType(Type* pointee, bool is_mutable);
    FunctionType* getFunctionType(std::vector<Type*> params, Type* ret);
    GenericType* getGenericType(const std::string& name);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 用户定义类型
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void registerStruct(StructType* type);
    void registerEnum(EnumType* type);
    void registerInterface(InterfaceType* type);
    
    Type* lookupType(const std::string& name);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 类型查询
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    bool isAssignable(Type* from, Type* to);
    bool equals(Type* t1, Type* t2);
    
private:
    void initializePrimitiveTypes();
    
    Arena arena_;
    std::unordered_map<std::string, Type*> primitive_cache_;
    std::unordered_map<std::string, Type*> named_types_;
    std::vector<std::unique_ptr<Type>> type_pool_;
};

} // namespace pawc

#endif // PAW_TYPE_SYSTEM_H
