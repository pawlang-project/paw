//===--- composite_types.cpp - Composite Types Implementation ---*- C++ -*-===//

#include "composite_types.h"

namespace pawc {

// ArrayType
bool ArrayType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Array) return false;
    auto* arr = static_cast<const ArrayType*>(other);
    return size_ == arr->size_ && element_->equals(arr->element_);
}

std::string ArrayType::toString() const {
    return "[" + element_->toString() + "; " + std::to_string(size_) + "]";
}

// SliceType
bool SliceType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Slice) return false;
    auto* slice = static_cast<const SliceType*>(other);
    return element_->equals(slice->element_);
}

std::string SliceType::toString() const {
    return "[" + element_->toString() + "]";
}

// TupleType
bool TupleType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Tuple) return false;
    auto* tuple = static_cast<const TupleType*>(other);
    if (elements_.size() != tuple->elements_.size()) return false;
    for (size_t i = 0; i < elements_.size(); ++i) {
        if (!elements_[i]->equals(tuple->elements_[i])) return false;
    }
    return true;
}

std::string TupleType::toString() const {
    std::string result = "(";
    for (size_t i = 0; i < elements_.size(); ++i) {
        if (i > 0) result += ", ";
        result += elements_[i]->toString();
    }
    result += ")";
    return result;
}

// StructType
Type* StructType::getFieldType(const std::string& field_name) const {
    for (const auto& [name, type] : fields_) {
        if (name == field_name) return type;
    }
    return nullptr;
}

bool StructType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Struct) return false;
    auto* st = static_cast<const StructType*>(other);
    return name_ == st->name_;
}

std::string StructType::toString() const {
    return name_;
}

// EnumType
Type* EnumType::getVariantDataType(const std::string& variant_name) const {
    for (const auto& [name, type] : variants_) {
        if (name == variant_name) return type;
    }
    return nullptr;
}

bool EnumType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Enum) return false;
    auto* en = static_cast<const EnumType*>(other);
    return name_ == en->name_;
}

std::string EnumType::toString() const {
    return name_;
}

} // namespace pawc

