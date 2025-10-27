/**
 * @file type.cpp
 * @brief Type 类的实现
 */

#include "type.h"
#include <sstream>

namespace pawc {

// ============================================================================
// PrimitiveType 实现
// ============================================================================

std::string PrimitiveType::toString() const {
    switch (primitive_) {
        case Primitive::I8:     return "i8";
        case Primitive::I16:    return "i16";
        case Primitive::I32:    return "i32";
        case Primitive::I64:    return "i64";
        case Primitive::I128:   return "i128";
        case Primitive::U8:     return "u8";
        case Primitive::U16:    return "u16";
        case Primitive::U32:    return "u32";
        case Primitive::U64:    return "u64";
        case Primitive::U128:   return "u128";
        case Primitive::F32:    return "f32";
        case Primitive::F64:    return "f64";
        case Primitive::Bool:   return "bool";
        case Primitive::Char:   return "char";
        case Primitive::String: return "string";
        case Primitive::Void:   return "void";
    }
    return "unknown";
}

// ============================================================================
// NamedType 实现
// ============================================================================

bool NamedType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Named) return false;
    
    const auto* named = static_cast<const NamedType*>(other);
    if (name_ != named->name_) return false;
    
    // 比较泛型参数
    if (generic_args_.size() != named->generic_args_.size()) return false;
    
    for (size_t i = 0; i < generic_args_.size(); ++i) {
        if (!generic_args_[i]->equals(named->generic_args_[i].get())) {
            return false;
        }
    }
    
    return true;
}

std::string NamedType::toString() const {
    std::string result = name_;
    
    if (!generic_args_.empty()) {
        result += "<";
        for (size_t i = 0; i < generic_args_.size(); ++i) {
            if (i > 0) result += ", ";
            result += generic_args_[i]->toString();
        }
        result += ">";
    }
    
    return result;
}

std::unique_ptr<Type> NamedType::clone() const {
    std::vector<std::unique_ptr<Type>> cloned_args;
    cloned_args.reserve(generic_args_.size());
    for (const auto& arg : generic_args_) {
        cloned_args.push_back(arg->clone());
    }
    return std::make_unique<NamedType>(name_, std::move(cloned_args));
}

// ============================================================================
// TupleType 实现
// ============================================================================

bool TupleType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Tuple) return false;
    
    const auto* tuple = static_cast<const TupleType*>(other);
    if (element_types_.size() != tuple->element_types_.size()) return false;
    
    for (size_t i = 0; i < element_types_.size(); ++i) {
        if (!element_types_[i]->equals(tuple->element_types_[i].get())) {
            return false;
        }
    }
    
    return true;
}

std::string TupleType::toString() const {
    if (element_types_.empty()) return "()";
    
    std::string result = "(";
    for (size_t i = 0; i < element_types_.size(); ++i) {
        if (i > 0) result += ", ";
        result += element_types_[i]->toString();
    }
    result += ")";
    return result;
}

std::unique_ptr<Type> TupleType::clone() const {
    std::vector<std::unique_ptr<Type>> cloned_elements;
    cloned_elements.reserve(element_types_.size());
    for (const auto& elem : element_types_) {
        cloned_elements.push_back(elem->clone());
    }
    return std::make_unique<TupleType>(std::move(cloned_elements));
}

// ============================================================================
// FunctionType 实现
// ============================================================================

bool FunctionType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Function) return false;
    
    const auto* func = static_cast<const FunctionType*>(other);
    
    // 比较参数类型
    if (param_types_.size() != func->param_types_.size()) return false;
    for (size_t i = 0; i < param_types_.size(); ++i) {
        if (!param_types_[i]->equals(func->param_types_[i].get())) {
            return false;
        }
    }
    
    // 比较返回类型
    return return_type_->equals(func->return_type_.get());
}

std::string FunctionType::toString() const {
    std::string result = "fn(";
    for (size_t i = 0; i < param_types_.size(); ++i) {
        if (i > 0) result += ", ";
        result += param_types_[i]->toString();
    }
    result += ") -> " + return_type_->toString();
    return result;
}

std::unique_ptr<Type> FunctionType::clone() const {
    std::vector<std::unique_ptr<Type>> cloned_params;
    cloned_params.reserve(param_types_.size());
    for (const auto& param : param_types_) {
        cloned_params.push_back(param->clone());
    }
    return std::make_unique<FunctionType>(std::move(cloned_params), return_type_->clone());
}

} // namespace pawc

