/**
 * @file type_system.cpp
 * @brief TypeSystem 实现
 */

#include "type_system.h"
#include <algorithm>

namespace pawc {

TypeSystem::TypeSystem() : self_type_(nullptr) {
    // 初始化 Self 类型单例
    auto self = std::make_unique<types::SelfType>();
    self_type_ = self.get();
    type_pool_.push_back(std::move(self));
}

// ============================================================================
// 类型创建
// ============================================================================

types::Type* TypeSystem::getPrimitiveType(types::PrimitiveType::Primitive prim) {
    // 检查缓存
    auto it = primitive_cache_.find(prim);
    if (it != primitive_cache_.end()) {
        return it->second;
    }
    
    // 创建并缓存
    auto type = std::make_unique<types::PrimitiveType>(prim);
    types::Type* ptr = type.get();
    primitive_cache_[prim] = ptr;
    type_pool_.push_back(std::move(type));
    
    return ptr;
}

types::Type* TypeSystem::getNamedType(const std::string& name, 
                                std::vector<std::unique_ptr<types::Type>> generic_args) {
    return internType(std::make_unique<types::NamedType>(name, std::move(generic_args)));
}

types::Type* TypeSystem::getGenericType(const std::string& name) {
    return internType(std::make_unique<types::GenericType>(name));
}

types::Type* TypeSystem::getSelfType() {
    return self_type_;
}

types::Type* TypeSystem::getArrayType(types::Type* element, int size) {
    return internType(std::make_unique<types::ArrayType>(element->clone(), size));
}

types::Type* TypeSystem::getSliceType(types::Type* element) {
    return internType(std::make_unique<types::SliceType>(element->clone()));
}

types::Type* TypeSystem::getTupleType(std::vector<std::unique_ptr<types::Type>> elements) {
    return internType(std::make_unique<types::TupleType>(std::move(elements)));
}

types::Type* TypeSystem::getReferenceType(types::Type* pointee, bool is_mutable) {
    return internType(std::make_unique<types::ReferenceType>(pointee->clone(), is_mutable));
}

types::Type* TypeSystem::getOptionalType(types::Type* inner) {
    return internType(std::make_unique<types::OptionalType>(inner->clone()));
}

types::Type* TypeSystem::getFunctionType(std::vector<std::unique_ptr<types::Type>> params, 
                                   std::unique_ptr<types::Type> return_type) {
    return internType(std::make_unique<types::FunctionType>(std::move(params), std::move(return_type)));
}

// ============================================================================
// 类型查询与比较
// ============================================================================

bool TypeSystem::equals(const types::Type* a, const types::Type* b) {
    if (a == b) return true;  // 指针相同
    if (!a || !b) return false;
    return a->equals(b);
}

bool TypeSystem::isAssignable(const types::Type* from, const types::Type* to) {
    // 基本规则：类型必须相等才能赋值
    if (equals(from, to)) return true;
    
    // 特殊规则：T 可以赋值给 T?
    if (to->getKind() == types::Type::Kind::Optional) {
        const auto* opt = static_cast<const types::OptionalType*>(to);
        return equals(from, opt->getInnerType());
    }
    
    // 特殊规则：&T 可以赋值给 &T (mut -> immut 允许)
    if (from->getKind() == types::Type::Kind::Reference && to->getKind() == types::Type::Kind::Reference) {
        const auto* from_ref = static_cast<const types::ReferenceType*>(from);
        const auto* to_ref = static_cast<const types::ReferenceType*>(to);
        
        // &mut T 可以赋值给 &T（去除可变性）
        if (from_ref->isMutable() && !to_ref->isMutable()) {
            return equals(from_ref->getPointeeType(), to_ref->getPointeeType());
        }
    }
    
    return false;
}

types::Type* TypeSystem::commonType(const types::Type* a, const types::Type* b) {
    if (equals(a, b)) return const_cast<types::Type*>(a);
    
    // 如果一个是 T，另一个是 T?，返回 T?
    if (a->getKind() == types::Type::Kind::Optional && b->getKind() != types::Type::Kind::Optional) {
        const auto* opt_a = static_cast<const types::OptionalType*>(a);
        if (equals(opt_a->getInnerType(), b)) {
            return const_cast<types::Type*>(a);
        }
    }
    if (b->getKind() == types::Type::Kind::Optional && a->getKind() != types::Type::Kind::Optional) {
        const auto* opt_b = static_cast<const types::OptionalType*>(b);
        if (equals(opt_b->getInnerType(), a)) {
            return const_cast<types::Type*>(b);
        }
    }
    
    // 否则没有公共类型
    return nullptr;
}

bool TypeSystem::isSubtype(const types::Type* sub, const types::Type* super) {
    // 基本实现：只支持相等性
    // 未来可以扩展为支持真正的子类型关系（如接口实现）
    return equals(sub, super);
}

std::string TypeSystem::toString(const types::Type* type) {
    if (!type) return "null";
    
    // 性能优化：检查缓存
    auto it = string_cache_.find(type);
    if (it != string_cache_.end()) {
        return it->second;
    }
    
    // 计算并缓存
    std::string result = type->toString();
    string_cache_[type] = result;
    return result;
}

// ============================================================================
// 类型注册
// ============================================================================

void TypeSystem::registerType(const std::string& name, types::Type* type) {
    named_types_[name] = type;
}

types::Type* TypeSystem::lookupType(const std::string& name) {
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
    auto self = std::make_unique<types::SelfType>();
    self_type_ = self.get();
    type_pool_.push_back(std::move(self));
}

types::Type* TypeSystem::internType(std::unique_ptr<types::Type> type) {
    types::Type* ptr = type.get();
    type_pool_.push_back(std::move(type));
    return ptr;
}

std::vector<std::unique_ptr<types::Type>> TypeSystem::cloneTypeVector(
    const std::vector<std::unique_ptr<types::Type>>& types) {
    std::vector<std::unique_ptr<types::Type>> cloned;
    cloned.reserve(types.size());
    for (const auto& type : types) {
        cloned.push_back(type->clone());
    }
    return cloned;
}

} // namespace pawc

