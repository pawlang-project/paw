//===--- primitive_types.h - Primitive Types --------------------*- C++ -*-===//
//
// 基础类型: i8-i128, u8-u128, f8-f128, bool, char, string, void
//
//===----------------------------------------------------------------------===//

#ifndef PAW_PRIMITIVE_TYPES_H
#define PAW_PRIMITIVE_TYPES_H

#include "type.h"
#include <string>

namespace pawc {

/// PrimitiveType - 基础类型
class PrimitiveType : public Type {
public:
    explicit PrimitiveType(Kind kind) : kind_(kind) {}
    
    Kind getKind() const override { return kind_; }
    
    bool equals(const Type* other) const override {
        return other && other->getKind() == kind_;
    }
    
    std::string toString() const override;
    
private:
    Kind kind_;
};

} // namespace pawc

#endif // PAW_PRIMITIVE_TYPES_H

