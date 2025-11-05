//===--- primitive_types.cpp - Primitive Types Implementation ---*- C++ -*-===//
/// @file primitive_types.cpp
/// @brief Type system and semantic analysis implementation

#include "primitive_types.h"

namespace pawc {

std::string PrimitiveType::toString() const {
    switch (kind_) {
        case Kind::I8: return "i8";
        case Kind::I16: return "i16";
        case Kind::I32: return "i32";
        case Kind::I64: return "i64";
        case Kind::I128: return "i128";
        case Kind::U8: return "u8";
        case Kind::U16: return "u16";
        case Kind::U32: return "u32";
        case Kind::U64: return "u64";
        case Kind::U128: return "u128";
        case Kind::F8: return "f8";
        case Kind::F16: return "f16";
        case Kind::F32: return "f32";
        case Kind::F64: return "f64";
        case Kind::F128: return "f128";
        case Kind::Bool: return "bool";
        case Kind::Char: return "char";
        case Kind::String: return "string";
        case Kind::Void: return "void";
        default: return "<unknown>";
    }
}

} // namespace pawc

