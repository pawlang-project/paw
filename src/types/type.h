/**
 * @file type.h
 * @brief PawLang 类型系统 - Type 基类和子类定义
 * 
 * 提供独立的类型系统，支持：
 * - 原始类型（i32, f64, bool等）
 * - 命名类型（struct, enum, interface）
 * - 复合类型（数组、切片、元组、引用）
 * - 泛型和特殊类型（Generic, Self, Optional）
 * 
 * @version 0.3.0-dev (Phase 2)
 * @date 2025-10-27
 */

#ifndef PAWC_TYPE_H
#define PAWC_TYPE_H

#include <string>
#include <vector>
#include <memory>

namespace pawc {

/**
 * 类型基类
 * 
 * 所有类型的抽象基类，提供统一的接口。
 * 使用访问者模式和虚函数实现多态。
 */
class Type {
public:
    /**
     * 类型种类枚举
     */
    enum class Kind {
        Primitive,   // 原始类型：i32, f64, bool等
        Named,       // 命名类型：用户定义的struct, enum, interface
        Array,       // 数组类型：[T; N]
        Slice,       // 切片类型：[T]
        Tuple,       // 元组类型：(T, U, V)
        Reference,   // 引用类型：&T, &mut T
        Function,    // 函数类型：fn(T1, T2) -> T3
        Generic,     // 泛型参数：T, U, V
        SelfType,    // Self类型（在方法中）
        Optional     // 可选类型：T?
    };
    
    virtual ~Type() = default;
    
    /**
     * 获取类型种类
     */
    virtual Kind getKind() const = 0;
    
    /**
     * 类型相等性比较
     * @param other 要比较的类型
     * @return 是否相等
     */
    virtual bool equals(const Type* other) const = 0;
    
    /**
     * 获取类型的字符串表示（用于错误信息和调试）
     * @return 类型字符串
     */
    virtual std::string toString() const = 0;
    
    /**
     * 克隆类型（深拷贝）
     * @return 新的类型实例
     */
    virtual std::unique_ptr<Type> clone() const = 0;
    
protected:
    Type() = default;
    
    // 禁止拷贝和赋值（使用 clone()）
    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;
};

// ============================================================================
// 原始类型
// ============================================================================

/**
 * 原始类型（内置类型）
 */
class PrimitiveType : public Type {
public:
    /**
     * 原始类型枚举
     */
    enum class Primitive {
        // 有符号整数
        I8, I16, I32, I64, I128,
        // 无符号整数
        U8, U16, U32, U64, U128,
        // 浮点数
        F32, F64,
        // 其他
        Bool, Char, String, Void
    };
    
    explicit PrimitiveType(Primitive prim) : primitive_(prim) {}
    
    Kind getKind() const override { return Kind::Primitive; }
    
    bool equals(const Type* other) const override {
        if (!other || other->getKind() != Kind::Primitive) return false;
        return primitive_ == static_cast<const PrimitiveType*>(other)->primitive_;
    }
    
    std::string toString() const override;
    
    std::unique_ptr<Type> clone() const override {
        return std::make_unique<PrimitiveType>(primitive_);
    }
    
    Primitive getPrimitive() const { return primitive_; }
    
private:
    Primitive primitive_;
};

// ============================================================================
// 命名类型
// ============================================================================

/**
 * 命名类型（用户定义类型）
 * 
 * 包括 struct, enum, interface 等用户定义的类型。
 * 支持泛型参数，如 Vec<i32>, Result<T, E>
 */
class NamedType : public Type {
public:
    NamedType(std::string name, std::vector<std::unique_ptr<Type>> generic_args = {})
        : name_(std::move(name)), generic_args_(std::move(generic_args)) {}
    
    Kind getKind() const override { return Kind::Named; }
    
    bool equals(const Type* other) const override;
    
    std::string toString() const override;
    
    std::unique_ptr<Type> clone() const override;
    
    const std::string& getName() const { return name_; }
    const std::vector<std::unique_ptr<Type>>& getGenericArgs() const { return generic_args_; }
    
private:
    std::string name_;
    std::vector<std::unique_ptr<Type>> generic_args_;
};

// ============================================================================
// 泛型类型
// ============================================================================

/**
 * 泛型参数类型
 * 
 * 表示泛型参数，如 T, U, V 等。
 */
class GenericType : public Type {
public:
    explicit GenericType(std::string name) : name_(std::move(name)) {}
    
    Kind getKind() const override { return Kind::Generic; }
    
    bool equals(const Type* other) const override {
        if (!other || other->getKind() != Kind::Generic) return false;
        return name_ == static_cast<const GenericType*>(other)->name_;
    }
    
    std::string toString() const override { return name_; }
    
    std::unique_ptr<Type> clone() const override {
        return std::make_unique<GenericType>(name_);
    }
    
    const std::string& getName() const { return name_; }
    
private:
    std::string name_;
};

// ============================================================================
// Self 类型
// ============================================================================

/**
 * Self 类型
 * 
 * 在结构体方法中表示当前类型本身。
 */
class SelfType : public Type {
public:
    SelfType() = default;
    
    Kind getKind() const override { return Kind::SelfType; }
    
    bool equals(const Type* other) const override {
        return other && other->getKind() == Kind::SelfType;
    }
    
    std::string toString() const override { return "Self"; }
    
    std::unique_ptr<Type> clone() const override {
        return std::make_unique<SelfType>();
    }
};

// ============================================================================
// 可选类型
// ============================================================================

/**
 * 可选类型（T?）
 * 
 * 用于错误处理和表示可能不存在的值。
 */
class OptionalType : public Type {
public:
    explicit OptionalType(std::unique_ptr<Type> inner)
        : inner_type_(std::move(inner)) {}
    
    Kind getKind() const override { return Kind::Optional; }
    
    bool equals(const Type* other) const override {
        if (!other || other->getKind() != Kind::Optional) return false;
        return inner_type_->equals(static_cast<const OptionalType*>(other)->inner_type_.get());
    }
    
    std::string toString() const override {
        return inner_type_->toString() + "?";
    }
    
    std::unique_ptr<Type> clone() const override {
        return std::make_unique<OptionalType>(inner_type_->clone());
    }
    
    const Type* getInnerType() const { return inner_type_.get(); }
    
private:
    std::unique_ptr<Type> inner_type_;
};

// ============================================================================
// 数组类型
// ============================================================================

/**
 * 数组类型 ([T; N])
 * 
 * 固定大小的数组。
 */
class ArrayType : public Type {
public:
    ArrayType(std::unique_ptr<Type> element, int size)
        : element_type_(std::move(element)), size_(size) {}
    
    Kind getKind() const override { return Kind::Array; }
    
    bool equals(const Type* other) const override {
        if (!other || other->getKind() != Kind::Array) return false;
        const auto* arr = static_cast<const ArrayType*>(other);
        return size_ == arr->size_ && element_type_->equals(arr->element_type_.get());
    }
    
    std::string toString() const override {
        return "[" + element_type_->toString() + "; " + std::to_string(size_) + "]";
    }
    
    std::unique_ptr<Type> clone() const override {
        return std::make_unique<ArrayType>(element_type_->clone(), size_);
    }
    
    const Type* getElementType() const { return element_type_.get(); }
    int getSize() const { return size_; }
    
private:
    std::unique_ptr<Type> element_type_;
    int size_;
};

// ============================================================================
// 切片类型
// ============================================================================

/**
 * 切片类型 ([T])
 * 
 * 动态大小的数组视图。
 */
class SliceType : public Type {
public:
    explicit SliceType(std::unique_ptr<Type> element)
        : element_type_(std::move(element)) {}
    
    Kind getKind() const override { return Kind::Slice; }
    
    bool equals(const Type* other) const override {
        if (!other || other->getKind() != Kind::Slice) return false;
        return element_type_->equals(static_cast<const SliceType*>(other)->element_type_.get());
    }
    
    std::string toString() const override {
        return "[" + element_type_->toString() + "]";
    }
    
    std::unique_ptr<Type> clone() const override {
        return std::make_unique<SliceType>(element_type_->clone());
    }
    
    const Type* getElementType() const { return element_type_.get(); }
    
private:
    std::unique_ptr<Type> element_type_;
};

// ============================================================================
// 元组类型
// ============================================================================

/**
 * 元组类型 ((T, U, V))
 * 
 * 多个类型的组合。
 */
class TupleType : public Type {
public:
    explicit TupleType(std::vector<std::unique_ptr<Type>> elements)
        : element_types_(std::move(elements)) {}
    
    Kind getKind() const override { return Kind::Tuple; }
    
    bool equals(const Type* other) const override;
    
    std::string toString() const override;
    
    std::unique_ptr<Type> clone() const override;
    
    const std::vector<std::unique_ptr<Type>>& getElementTypes() const { return element_types_; }
    
private:
    std::vector<std::unique_ptr<Type>> element_types_;
};

// ============================================================================
// 引用类型
// ============================================================================

/**
 * 引用类型 (&T, &mut T)
 * 
 * 指向其他类型的引用，可以是可变或不可变的。
 */
class ReferenceType : public Type {
public:
    ReferenceType(std::unique_ptr<Type> pointee, bool is_mutable)
        : pointee_type_(std::move(pointee)), is_mutable_(is_mutable) {}
    
    Kind getKind() const override { return Kind::Reference; }
    
    bool equals(const Type* other) const override {
        if (!other || other->getKind() != Kind::Reference) return false;
        const auto* ref = static_cast<const ReferenceType*>(other);
        return is_mutable_ == ref->is_mutable_ && pointee_type_->equals(ref->pointee_type_.get());
    }
    
    std::string toString() const override {
        return "&" + std::string(is_mutable_ ? "mut " : "") + pointee_type_->toString();
    }
    
    std::unique_ptr<Type> clone() const override {
        return std::make_unique<ReferenceType>(pointee_type_->clone(), is_mutable_);
    }
    
    const Type* getPointeeType() const { return pointee_type_.get(); }
    bool isMutable() const { return is_mutable_; }
    
private:
    std::unique_ptr<Type> pointee_type_;
    bool is_mutable_;
};

// ============================================================================
// 函数类型
// ============================================================================

/**
 * 函数类型 (fn(T1, T2) -> T3)
 * 
 * 表示函数的类型签名。
 */
class FunctionType : public Type {
public:
    FunctionType(std::vector<std::unique_ptr<Type>> params, std::unique_ptr<Type> return_type)
        : param_types_(std::move(params)), return_type_(std::move(return_type)) {}
    
    Kind getKind() const override { return Kind::Function; }
    
    bool equals(const Type* other) const override;
    
    std::string toString() const override;
    
    std::unique_ptr<Type> clone() const override;
    
    const std::vector<std::unique_ptr<Type>>& getParamTypes() const { return param_types_; }
    const Type* getReturnType() const { return return_type_.get(); }
    
private:
    std::vector<std::unique_ptr<Type>> param_types_;
    std::unique_ptr<Type> return_type_;
};

} // namespace pawc

#endif // PAWC_TYPE_H

