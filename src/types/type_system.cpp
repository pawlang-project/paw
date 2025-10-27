/**
 * @file type_system.cpp
 * @brief TypeSystem 实现
 */

#include "type_system.h"
#include <algorithm>

namespace pawc {

TypeSystem::TypeSystem() : self_type_(nullptr) {
    // 初始化 Self 类型单例
    auto self = std::make_unique<SelfType>();
    self_type_ = self.get();
    type_pool_.push_back(std::move(self));
}

// ============================================================================
// 类型创建
// ============================================================================

Type* TypeSystem::getPrimitiveType(PrimitiveType::Primitive prim) {
    // 检查缓存
    auto it = primitive_cache_.find(prim);
    if (it != primitive_cache_.end()) {
        return it->second;
    }
    
    // 创建并缓存
    auto type = std::make_unique<PrimitiveType>(prim);
    Type* ptr = type.get();
    primitive_cache_[prim] = ptr;
    type_pool_.push_back(std::move(type));
    
    return ptr;
}

Type* TypeSystem::getNamedType(const std::string& name, 
                                std::vector<std::unique_ptr<Type>> generic_args) {
    return internType(std::make_unique<NamedType>(name, std::move(generic_args)));
}

Type* TypeSystem::getGenericType(const std::string& name) {
    return internType(std::make_unique<GenericType>(name));
}

Type* TypeSystem::getSelfType() {
    return self_type_;
}

Type* TypeSystem::getArrayType(Type* element, int size) {
    return internType(std::make_unique<ArrayType>(element->clone(), size));
}

Type* TypeSystem::getSliceType(Type* element) {
    return internType(std::make_unique<SliceType>(element->clone()));
}

Type* TypeSystem::getTupleType(std::vector<std::unique_ptr<Type>> elements) {
    return internType(std::make_unique<TupleType>(std::move(elements)));
}

Type* TypeSystem::getReferenceType(Type* pointee, bool is_mutable) {
    return internType(std::make_unique<ReferenceType>(pointee->clone(), is_mutable));
}

Type* TypeSystem::getOptionalType(Type* inner) {
    return internType(std::make_unique<OptionalType>(inner->clone()));
}

Type* TypeSystem::getFunctionType(std::vector<std::unique_ptr<Type>> params, 
                                   std::unique_ptr<Type> return_type) {
    return internType(std::make_unique<FunctionType>(std::move(params), std::move(return_type)));
}

// ============================================================================
// 类型查询与比较
// ============================================================================

bool TypeSystem::equals(const Type* a, const Type* b) {
    if (a == b) return true;  // 指针相同
    if (!a || !b) return false;
    return a->equals(b);
}

bool TypeSystem::isAssignable(const Type* from, const Type* to) {
    // 基本规则：类型必须相等才能赋值
    if (equals(from, to)) return true;
    
    // 特殊规则：T 可以赋值给 T?
    if (to->getKind() == Type::Kind::Optional) {
        const auto* opt = static_cast<const OptionalType*>(to);
        return equals(from, opt->getInnerType());
    }
    
    // 特殊规则：&T 可以赋值给 &T (mut -> immut 允许)
    if (from->getKind() == Type::Kind::Reference && to->getKind() == Type::Kind::Reference) {
        const auto* from_ref = static_cast<const ReferenceType*>(from);
        const auto* to_ref = static_cast<const ReferenceType*>(to);
        
        // &mut T 可以赋值给 &T（去除可变性）
        if (from_ref->isMutable() && !to_ref->isMutable()) {
            return equals(from_ref->getPointeeType(), to_ref->getPointeeType());
        }
    }
    
    return false;
}

Type* TypeSystem::commonType(const Type* a, const Type* b) {
    if (equals(a, b)) return const_cast<Type*>(a);
    
    // 如果一个是 T，另一个是 T?，返回 T?
    if (a->getKind() == Type::Kind::Optional && b->getKind() != Type::Kind::Optional) {
        const auto* opt_a = static_cast<const OptionalType*>(a);
        if (equals(opt_a->getInnerType(), b)) {
            return const_cast<Type*>(a);
        }
    }
    if (b->getKind() == Type::Kind::Optional && a->getKind() != Type::Kind::Optional) {
        const auto* opt_b = static_cast<const OptionalType*>(b);
        if (equals(opt_b->getInnerType(), a)) {
            return const_cast<Type*>(b);
        }
    }
    
    // 否则没有公共类型
    return nullptr;
}

bool TypeSystem::isSubtype(const Type* sub, const Type* super) {
    // 基本实现：只支持相等性
    // 未来可以扩展为支持真正的子类型关系（如接口实现）
    return equals(sub, super);
}

std::string TypeSystem::toString(const Type* type) {
    return type ? type->toString() : "null";
}

// ============================================================================
// 类型注册
// ============================================================================

void TypeSystem::registerType(const std::string& name, Type* type) {
    named_types_[name] = type;
}

Type* TypeSystem::lookupType(const std::string& name) {
    auto it = named_types_.find(name);
    if (it != named_types_.end()) {
        return it->second;
    }
    return nullptr;
}

bool TypeSystem::hasType(const std::string& name) const {
    return named_types_.find(name) != named_types_.end();
}

// ============================================================================
// 工具方法
// ============================================================================

void TypeSystem::clear() {
    // 清空类型池（保留原始类型缓存和 Self 类型）
    type_pool_.clear();
    named_types_.clear();
    
    // 重新初始化 Self 类型
    auto self = std::make_unique<SelfType>();
    self_type_ = self.get();
    type_pool_.push_back(std::move(self));
}

Type* TypeSystem::internType(std::unique_ptr<Type> type) {
    Type* ptr = type.get();
    type_pool_.push_back(std::move(type));
    return ptr;
}

std::vector<std::unique_ptr<Type>> TypeSystem::cloneTypeVector(
    const std::vector<std::unique_ptr<Type>>& types) {
    std::vector<std::unique_ptr<Type>> cloned;
    cloned.reserve(types.size());
    for (const auto& type : types) {
        cloned.push_back(type->clone());
    }
    return cloned;
}

} // namespace pawc

