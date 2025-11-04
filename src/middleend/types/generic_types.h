//===--- generic_types.h - Generic and Special Types ------------*- C++ -*-===//
//
// 泛型和特殊类型: Generic, Optional, Result, Reference, Function, Interface
//
//===----------------------------------------------------------------------===//

#ifndef PAW_GENERIC_TYPES_H
#define PAW_GENERIC_TYPES_H

#include "type.h"
#include <vector>
#include <string>

namespace pawc {

// 前向声明
class Expr;

/// GenericType - 泛型参数类型 T
class GenericType : public Type {
public:
    explicit GenericType(const std::string& name) : name_(name) {}
    
    Kind getKind() const override { return Kind::Generic; }
    const std::string& getName() const { return name_; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    std::string name_;
};

/// OptionalType - Optional类型 T?
class OptionalType : public Type {
public:
    explicit OptionalType(Type* inner) : inner_(inner) {}
    
    Kind getKind() const override { return Kind::Optional; }
    Type* getInnerType() const { return inner_; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    Type* inner_;
};

/// ResultType - Result类型 T!
class ResultType : public Type {
public:
    explicit ResultType(Type* ok_type) : ok_type_(ok_type) {}
    
    Kind getKind() const override { return Kind::Result; }
    Type* getOkType() const { return ok_type_; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    Type* ok_type_;
};

/// ReferenceType - 引用类型 &T, &~T
class ReferenceType : public Type {
public:
    ReferenceType(Type* pointee, bool is_mutable)
        : pointee_(pointee), is_mutable_(is_mutable) {}
    
    Kind getKind() const override { return Kind::Reference; }
    Type* getPointeeType() const { return pointee_; }
    bool isMutable() const { return is_mutable_; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    Type* pointee_;
    bool is_mutable_;
};

/// FunctionType - 函数类型 fn(T1, T2) -> T3
class FunctionType : public Type {
public:
    FunctionType(std::vector<Type*> params, Type* ret)
        : params_(std::move(params)), return_(ret), has_captures_(false) {}
    
    Kind getKind() const override { return Kind::Function; }
    const std::vector<Type*>& getParamTypes() const { return params_; }
    Type* getReturnType() const { return return_; }
    
    // 闭包捕获标记（CodeGen使用）
    bool hasCaptures() const { return has_captures_; }
    void setHasCaptures(bool has_captures) { has_captures_ = has_captures; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    std::vector<Type*> params_;
    Type* return_;
    bool has_captures_;  // 闭包是否有捕获变量
};

/// InterfaceType - 接口类型
class InterfaceType : public Type {
public:
    struct MethodSignature {
        std::string name;
        std::vector<Type*> param_types;
        std::vector<std::string> param_names;  // 🔧 新增：参数名
        Type* return_type;
        bool has_default_impl;  // 🔧 新增：是否有默认实现
        Expr* default_body;     // 🔧 新增：默认实现的body（不拥有所有权）
        
        MethodSignature(std::string n, std::vector<Type*> p, Type* r, bool has_default = false, Expr* body = nullptr, std::vector<std::string> pnames = {})
            : name(std::move(n)), param_types(std::move(p)), param_names(std::move(pnames)), return_type(r), 
              has_default_impl(has_default), default_body(body) {}
    };
    
    InterfaceType(const std::string& name, std::vector<MethodSignature> methods,
                  std::vector<std::string> generic_param_names = {})
        : name_(name), methods_(std::move(methods)), generic_param_names_(std::move(generic_param_names)) {}
    
    explicit InterfaceType(const std::string& name) : name_(name) {}
    
    Kind getKind() const override { return Kind::Interface; }
    const std::string& getName() const { return name_; }
    const std::vector<MethodSignature>& getMethods() const { return methods_; }
    const std::vector<std::string>& getGenericParamNames() const { return generic_param_names_; }
    
    bool isGeneric() const { return !generic_param_names_.empty(); }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    std::string name_;
    std::vector<MethodSignature> methods_;
    std::vector<std::string> generic_param_names_;  // 泛型参数名称: ["T", "U", ...]
};

/// SelfType - Self类型（用于接口方法）
class SelfType : public Type {
public:
    SelfType() = default;
    
    Kind getKind() const override { return Kind::SelfType; }
    
    bool equals(const Type* other) const override {
        return other && other->getKind() == Kind::SelfType;
    }
    
    std::string toString() const override { return "Self"; }
};

} // namespace pawc

#endif // PAW_GENERIC_TYPES_H

