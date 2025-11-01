//===--- generic_types.cpp - Generic Types Implementation -------*- C++ -*-===//

#include "generic_types.h"

namespace pawc {

// GenericType
bool GenericType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Generic) return false;
    auto* gen = static_cast<const GenericType*>(other);
    return name_ == gen->name_;
}

std::string GenericType::toString() const {
    return name_;
}

// OptionalType
bool OptionalType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Optional) return false;
    auto* opt = static_cast<const OptionalType*>(other);
    return inner_->equals(opt->inner_);
}

std::string OptionalType::toString() const {
    return inner_->toString() + "?";
}

// ResultType
bool ResultType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Result) return false;
    auto* res = static_cast<const ResultType*>(other);
    return ok_type_->equals(res->ok_type_);
}

std::string ResultType::toString() const {
    return ok_type_->toString() + "!";
}

// ReferenceType
bool ReferenceType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Reference) return false;
    auto* ref = static_cast<const ReferenceType*>(other);
    return is_mutable_ == ref->is_mutable_ && 
           pointee_->equals(ref->pointee_);
}

std::string ReferenceType::toString() const {
    return "&" + std::string(is_mutable_ ? "~" : "") + pointee_->toString();
}

// FunctionType
bool FunctionType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Function) return false;
    auto* func = static_cast<const FunctionType*>(other);
    if (params_.size() != func->params_.size()) return false;
    if (!return_->equals(func->return_)) return false;
    for (size_t i = 0; i < params_.size(); ++i) {
        if (!params_[i]->equals(func->params_[i])) return false;
    }
    return true;
}

std::string FunctionType::toString() const {
    std::string result = "fn(";
    for (size_t i = 0; i < params_.size(); ++i) {
        if (i > 0) result += ", ";
        result += params_[i]->toString();
    }
    result += ") -> " + return_->toString();
    return result;
}

// InterfaceType
bool InterfaceType::equals(const Type* other) const {
    if (!other || other->getKind() != Kind::Interface) return false;
    auto* iface = static_cast<const InterfaceType*>(other);
    return name_ == iface->name_;
}

std::string InterfaceType::toString() const {
    return name_;
}

} // namespace pawc

