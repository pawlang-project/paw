//===--- type.h - Type System Base (Refactored) -----------------*- C++ -*-===//
//
// PawLang Compiler - Type System Base Classes
//
// This file defines the abstract Type base class and the Kind enumeration
// for all 28 type kinds in PawLang.
//
// Type Hierarchy:
//   Type (abstract base)
//   ├── Primitive Types (19 kinds) - primitive_types.h
//   │   ├── Integers: i8, i16, i32, i64, i128, u8, u16, u32, u64, u128
//   │   ├── Floats: f8, f16, f32, f64, f128
//   │   └── Others: bool, char, string, void
//   ├── Composite Types (5 kinds) - composite_types.h
//   │   ├── Array: [T; N]
//   │   ├── Slice: [T]
//   │   ├── Tuple: (T1, T2, ...)
//   │   ├── Struct: User-defined structs
//   │   └── Enum: User-defined enums
//   └── Special Types (7 kinds) - generic_types.h
//       ├── Optional: T?
//       ├── Result: T!
//       ├── Reference: &T, &~T
//       ├── Function: fn(T1, T2) -> R
//       ├── Generic: Type parameters (T, U, V)
//       ├── Interface: Interface types
//       └── SelfType: Self keyword
//
// Design Principles:
//   - Zero-cost abstractions: Types compile to efficient machine code
//   - Compile-time type checking: All type errors caught before runtime
//   - Type inference: Types deduced from context where possible
//   - Generics: Monomorphization for zero runtime overhead
//
// Memory Management:
//   - All Type instances owned by TypeSystem
//   - Never manually delete Type pointers
//   - Type interning: Identical types share same instance
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_H
#define PAW_TYPE_H

#include <string>

namespace pawc {

/// Type - Abstract base class for all types in PawLang
///
/// The Type class is the foundation of PawLang's type system, representing
/// all possible types that values can have.
///
/// Type Categories (28 total):
///   - Primitive types: i8-i128, u8-u128, f8-f128, bool, char, string, void (19)
///   - Composite types: Array, Slice, Tuple, Struct, Enum (5)
///   - Special types: Optional, Result, Reference, Function, Generic, Interface, SelfType (7)
///
/// Key Operations:
///   - getKind(): Identifies which concrete type this is
///   - equals(): Structural type equality
///   - toString(): Human-readable type representation
///
/// Type Checking:
///   - isPrimitive(), isInteger(), isFloat(): Type category checks
///   - equals(): Used for type compatibility
///   - Subtype relationships handled by TypeChecker
///
/// Ownership:
///   - All Type instances owned by TypeSystem (singleton pattern)
///   - Never manually delete Type*
///   - Use raw pointers everywhere (managed lifetime)
class Type {
public:
    /// Type kind enumeration - all 28 type kinds
    enum class Kind {
        // Primitive types (19 kinds)
        I8, I16, I32, I64, I128,        ///< Signed integers (5)
        U8, U16, U32, U64, U128,        ///< Unsigned integers (5)
        F8, F16, F32, F64, F128,        ///< Floating-point (5)
        Bool, Char, String, Void,       ///< Other primitives (4)
        
        // Composite types (5 kinds)
        Array,        ///< Fixed-size array [T; N]
        Slice,        ///< Dynamic slice [T]
        Tuple,        ///< Tuple (T1, T2, ...)
        Struct,       ///< User-defined struct
        Enum,         ///< User-defined enum
        
        // Special types (4 kinds)
        Optional,     ///< Optional value T?
        Result,       ///< Result type T! (error handling)
        Reference,    ///< Reference &T or &~T
        Function,     ///< Function type fn(T1, T2) -> R
        
        // Generic and interface types (3 kinds)
        Generic,      ///< Generic type parameter (T, U, V)
        Interface,    ///< Interface type
        SelfType,     ///< Self keyword (in interface methods)
    };
    
    virtual ~Type() = default;
    
    virtual Kind getKind() const = 0;
    virtual bool equals(const Type* other) const = 0;
    virtual std::string toString() const = 0;
    
    // typedeterminehelpermethod
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
    bool isSlice() const { return getKind() == Kind::Slice; }
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
