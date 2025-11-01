//===--- type.h - Type System Base (Refactored) -----------------*- C++ -*-===//
//
// PawLang Compiler - Type System Base Classes Only
// 
// 具体类型实现已拆分到：
// - primitive_types.h/cpp - 基础类型
// - composite_types.h/cpp - 复合类型  
// - generic_types.h/cpp - 泛型和特殊类型
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_H
#define PAW_TYPE_H

#include <string>

namespace pawc {

/// Type - 所有类型的基类
///
/// PawLang支持28种类型：
/// - 基础类型：i8-i128, u8-u128, f8-f128, bool, char, string, void (19种)
/// - 复合类型：Array, Slice, Tuple, Struct, Enum (5种)
/// - 特殊类型：Optional(T?), Result(T!), Reference, Function, Generic, Interface, SelfType (7种)
class Type {
public:
    enum class Kind {
        // 基础类型 (19种)
        I8, I16, I32, I64, I128,        // 有符号整数 (5)
        U8, U16, U32, U64, U128,        // 无符号整数 (5)
        F8, F16, F32, F64, F128,        // 浮点数 (5)
        Bool, Char, String, Void,       // 其他 (4)
        
        // 复合类型 (5种)
        Array,        // [T; N]
        Slice,        // [T]
        Tuple,        // (T1, T2, ...)
        Struct,       // 用户定义结构体
        Enum,         // 用户定义枚举
        
        // 特殊类型 (4种)
        Optional,     // T? - 可选值
        Result,       // T! - 错误处理
        Reference,    // &T, &~T
        Function,     // fn(T1, T2) -> T3
        
        // 泛型和接口 (3种)
        Generic,      // T (泛型参数)
        Interface,    // 接口类型
        SelfType,     // Self
    };
    
    virtual ~Type() = default;
    
    virtual Kind getKind() const = 0;
    virtual bool equals(const Type* other) const = 0;
    virtual std::string toString() const = 0;
    
    // 类型判断辅助方法
    bool isPrimitive() const {
        Kind k = getKind();
        return k >= Kind::I8 && k <= Kind::Void;
    }
    
    bool isInteger() const {
        Kind k = getKind();
        return (k >= Kind::I8 && k <= Kind::I128) ||
               (k >= Kind::U8 && k <= Kind::U128);
    }
    
    bool isSignedInteger() const {
        Kind k = getKind();
        return k >= Kind::I8 && k <= Kind::I128;
    }
    
    bool isUnsignedInteger() const {
        Kind k = getKind();
        return k >= Kind::U8 && k <= Kind::U128;
    }
    
    bool isFloat() const {
        Kind k = getKind();
        return k >= Kind::F8 && k <= Kind::F128;
    }
    
    bool isNumeric() const {
        return isInteger() || isFloat();
    }
    
    bool isBool() const { return getKind() == Kind::Bool; }
    bool isString() const { return getKind() == Kind::String; }
    bool isVoid() const { return getKind() == Kind::Void; }
    bool isArray() const { return getKind() == Kind::Array; }
    bool isTuple() const { return getKind() == Kind::Tuple; }
    bool isStruct() const { return getKind() == Kind::Struct; }
    bool isEnum() const { return getKind() == Kind::Enum; }
    bool isOptional() const { return getKind() == Kind::Optional; }
    bool isResult() const { return getKind() == Kind::Result; }
    bool isReference() const { return getKind() == Kind::Reference; }
    bool isFunction() const { return getKind() == Kind::Function; }
};

} // namespace pawc

#endif // PAW_TYPE_H
