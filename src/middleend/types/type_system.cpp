//===--- type_system.cpp - Type System Implementation ------------*- C++ -*-===//

#include "type_system.h"

namespace pawc {

TypeSystem::TypeSystem() {
    initializePrimitiveTypes();
}

void TypeSystem::initializePrimitiveTypes() {
    // 有符号整数 (5)
    primitive_cache_["i8"] = arena_.allocate<PrimitiveType>(Type::Kind::I8);
    primitive_cache_["i16"] = arena_.allocate<PrimitiveType>(Type::Kind::I16);
    primitive_cache_["i32"] = arena_.allocate<PrimitiveType>(Type::Kind::I32);
    primitive_cache_["i64"] = arena_.allocate<PrimitiveType>(Type::Kind::I64);
    primitive_cache_["i128"] = arena_.allocate<PrimitiveType>(Type::Kind::I128);
    
    // 无符号整数 (5)
    primitive_cache_["u8"] = arena_.allocate<PrimitiveType>(Type::Kind::U8);
    primitive_cache_["u16"] = arena_.allocate<PrimitiveType>(Type::Kind::U16);
    primitive_cache_["u32"] = arena_.allocate<PrimitiveType>(Type::Kind::U32);
    primitive_cache_["u64"] = arena_.allocate<PrimitiveType>(Type::Kind::U64);
    primitive_cache_["u128"] = arena_.allocate<PrimitiveType>(Type::Kind::U128);
    
    // 浮点数 (5) - 原生精度，不近似
    primitive_cache_["f8"] = arena_.allocate<PrimitiveType>(Type::Kind::F8);
    primitive_cache_["f16"] = arena_.allocate<PrimitiveType>(Type::Kind::F16);
    primitive_cache_["f32"] = arena_.allocate<PrimitiveType>(Type::Kind::F32);
    primitive_cache_["f64"] = arena_.allocate<PrimitiveType>(Type::Kind::F64);
    primitive_cache_["f128"] = arena_.allocate<PrimitiveType>(Type::Kind::F128);
    
    // 其他 (4)
    primitive_cache_["bool"] = arena_.allocate<PrimitiveType>(Type::Kind::Bool);
    primitive_cache_["char"] = arena_.allocate<PrimitiveType>(Type::Kind::Char);
    primitive_cache_["string"] = arena_.allocate<PrimitiveType>(Type::Kind::String);
    primitive_cache_["void"] = arena_.allocate<PrimitiveType>(Type::Kind::Void);
}

// 基础类型getter（18种）
Type* TypeSystem::getI8Type() { return primitive_cache_["i8"]; }
Type* TypeSystem::getI16Type() { return primitive_cache_["i16"]; }
Type* TypeSystem::getI32Type() { return primitive_cache_["i32"]; }
Type* TypeSystem::getI64Type() { return primitive_cache_["i64"]; }
Type* TypeSystem::getI128Type() { return primitive_cache_["i128"]; }

Type* TypeSystem::getU8Type() { return primitive_cache_["u8"]; }
Type* TypeSystem::getU16Type() { return primitive_cache_["u16"]; }
Type* TypeSystem::getU32Type() { return primitive_cache_["u32"]; }
Type* TypeSystem::getU64Type() { return primitive_cache_["u64"]; }
Type* TypeSystem::getU128Type() { return primitive_cache_["u128"]; }

Type* TypeSystem::getF8Type() { return primitive_cache_["f8"]; }
Type* TypeSystem::getF16Type() { return primitive_cache_["f16"]; }
Type* TypeSystem::getF32Type() { return primitive_cache_["f32"]; }
Type* TypeSystem::getF64Type() { return primitive_cache_["f64"]; }
Type* TypeSystem::getF128Type() { return primitive_cache_["f128"]; }

Type* TypeSystem::getBoolType() { return primitive_cache_["bool"]; }
Type* TypeSystem::getCharType() { return primitive_cache_["char"]; }
Type* TypeSystem::getStringType() { return primitive_cache_["string"]; }
Type* TypeSystem::getVoidType() { return primitive_cache_["void"]; }

// 复合类型创建
ArrayType* TypeSystem::getArrayType(Type* element, size_t size) {
    auto* type = arena_.allocate<ArrayType>(element, size);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

SliceType* TypeSystem::getSliceType(Type* element) {
    auto* type = arena_.allocate<SliceType>(element);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

TupleType* TypeSystem::getTupleType(std::vector<Type*> elements) {
    auto* type = arena_.allocate<TupleType>(std::move(elements));
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

OptionalType* TypeSystem::getOptionalType(Type* inner) {
    auto* type = arena_.allocate<OptionalType>(inner);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

ResultType* TypeSystem::getResultType(Type* ok_type) {
    auto* type = arena_.allocate<ResultType>(ok_type);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

ReferenceType* TypeSystem::getReferenceType(Type* pointee, bool is_mutable) {
    auto* type = arena_.allocate<ReferenceType>(pointee, is_mutable);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

FunctionType* TypeSystem::getFunctionType(std::vector<Type*> params, Type* ret) {
    auto* type = arena_.allocate<FunctionType>(std::move(params), ret);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

GenericType* TypeSystem::getGenericType(const std::string& name) {
    auto* type = arena_.allocate<GenericType>(name);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

// 用户定义类型注册
void TypeSystem::registerStruct(StructType* type) {
    named_types_[type->getName()] = type;
}

void TypeSystem::registerEnum(EnumType* type) {
    named_types_[type->getName()] = type;
}

void TypeSystem::registerInterface(InterfaceType* type) {
    named_types_[type->getName()] = type;
}

Type* TypeSystem::lookupType(const std::string& name) {
    // 先查基础类型
    auto it = primitive_cache_.find(name);
    if (it != primitive_cache_.end()) {
        return it->second;
    }
    
    // 再查用户定义类型
    auto it2 = named_types_.find(name);
    if (it2 != named_types_.end()) {
        return it2->second;
    }
    
    return nullptr;
}

// 类型兼容性检查
bool TypeSystem::isAssignable(Type* from, Type* to) {
    return equals(from, to);
}

bool TypeSystem::equals(Type* t1, Type* t2) {
    if (!t1 || !t2) return false;
    return t1->equals(t2);
}

} // namespace pawc
