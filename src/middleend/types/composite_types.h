//===--- composite_types.h - Composite Types --------------------*- C++ -*-===//
//
// 复合类型: Array, Slice, Tuple, Struct, Enum
//
//===----------------------------------------------------------------------===//

#ifndef PAW_COMPOSITE_TYPES_H
#define PAW_COMPOSITE_TYPES_H

#include "type.h"
#include <vector>
#include <string>

namespace pawc {

/// ArrayType - 数组类型 [T; N]
class ArrayType : public Type {
public:
    ArrayType(Type* element, size_t size)
        : element_(element), size_(size) {}
    
    Kind getKind() const override { return Kind::Array; }
    Type* getElementType() const { return element_; }
    size_t getSize() const { return size_; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    Type* element_;
    size_t size_;
};

/// SliceType - 切片类型 [T]
class SliceType : public Type {
public:
    explicit SliceType(Type* element) : element_(element) {}
    
    Kind getKind() const override { return Kind::Slice; }
    Type* getElementType() const { return element_; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    Type* element_;
};

/// TupleType - 元组类型 (T1, T2, ...)
class TupleType : public Type {
public:
    explicit TupleType(std::vector<Type*> elements)
        : elements_(std::move(elements)) {}
    
    Kind getKind() const override { return Kind::Tuple; }
    const std::vector<Type*>& getElementTypes() const { return elements_; }
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    std::vector<Type*> elements_;
};

/// StructType - 结构体类型
class StructType : public Type {
public:
    using Field = std::pair<std::string, Type*>;
    
    StructType(const std::string& name, std::vector<Field> fields)
        : name_(name), fields_(std::move(fields)) {}
    
    Kind getKind() const override { return Kind::Struct; }
    const std::string& getName() const { return name_; }
    const std::vector<Field>& getFields() const { return fields_; }
    Type* getFieldType(const std::string& field_name) const;
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    std::string name_;
    std::vector<Field> fields_;
};

/// EnumType - 枚举类型
class EnumType : public Type {
public:
    using Variant = std::pair<std::string, Type*>;
    
    EnumType(const std::string& name, std::vector<Variant> variants)
        : name_(name), variants_(std::move(variants)) {}
    
    Kind getKind() const override { return Kind::Enum; }
    const std::string& getName() const { return name_; }
    const std::vector<Variant>& getVariants() const { return variants_; }
    Type* getVariantDataType(const std::string& variant_name) const;
    
    bool equals(const Type* other) const override;
    std::string toString() const override;
    
private:
    std::string name_;
    std::vector<Variant> variants_;
};

} // namespace pawc

#endif // PAW_COMPOSITE_TYPES_H

