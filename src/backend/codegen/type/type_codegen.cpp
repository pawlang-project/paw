//===--- type_codegen.cpp - Type Mapping Implementation ----------*- C++ -*-===//
/// @file type_codegen.cpp
/// @brief Code generation implementation

#include "type_codegen.h"
#include "middleend/types/primitive_types.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/generic_types.h"
#include <iostream>

namespace pawc {

TypeCodeGen::TypeCodeGen(llvm::LLVMContext& llvm_context)
    : context_(llvm_context) {}

llvm::Type* TypeCodeGen::mapType(Type* paw_type) {
    // Check cache first
    auto it = cache_.find(paw_type);
    if (it != cache_.end()) {
        return it->second;
    }
    
    llvm::Type* llvm_type = nullptr;
    
    if (paw_type->isPrimitive()) {
        llvm_type = mapPrimitiveType(paw_type);
    } else if (paw_type->isArray()) {
        llvm_type = mapArrayType(static_cast<ArrayType*>(paw_type));
    } else if (paw_type->isSlice()) {
        llvm_type = mapSliceType(static_cast<SliceType*>(paw_type));
    } else if (paw_type->isTuple()) {
        llvm_type = mapTupleType(static_cast<TupleType*>(paw_type));
    } else if (paw_type->isStruct()) {
        llvm_type = mapStructType(static_cast<StructType*>(paw_type));
    } else if (paw_type->isEnum()) {
        llvm_type = mapEnumType(static_cast<EnumType*>(paw_type));
    } else if (paw_type->isOptional()) {
        llvm_type = mapOptionalType(static_cast<OptionalType*>(paw_type));
    } else if (paw_type->isResult()) {
        llvm_type = mapResultType(static_cast<ResultType*>(paw_type));
    } else if (paw_type->isReference()) {
        llvm_type = mapReferenceType(static_cast<ReferenceType*>(paw_type));
    } else if (paw_type->isFunction()) {
        llvm_type = mapFunctionType(static_cast<FunctionType*>(paw_type));
    } else {
        // default：i8pointer（opaque pointer）
        llvm_type = llvm::PointerType::getUnqual(context_);
    }
    
    // cache
    cache_[paw_type] = llvm_type;
    
    return llvm_type;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// basetypesmap (18types/kinds) - completeprecision
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Type* TypeCodeGen::mapPrimitiveType(Type* type) {
    switch (type->getKind()) {
        // Signed integers
        case Type::Kind::I8:    return llvm::Type::getInt8Ty(context_);
        case Type::Kind::I16:   return llvm::Type::getInt16Ty(context_);
        case Type::Kind::I32:   return llvm::Type::getInt32Ty(context_);
        case Type::Kind::I64:   return llvm::Type::getInt64Ty(context_);
        case Type::Kind::I128:  return llvm::Type::getInt128Ty(context_);
        
        // Unsigned integers
        case Type::Kind::U8:    return llvm::Type::getInt8Ty(context_);
        case Type::Kind::U16:   return llvm::Type::getInt16Ty(context_);
        case Type::Kind::U32:   return llvm::Type::getInt32Ty(context_);
        case Type::Kind::U64:   return llvm::Type::getInt64Ty(context_);
        case Type::Kind::U128:  return llvm::Type::getInt128Ty(context_);
        
        // Floating-point numbers - completeprecision，notapproximate
        case Type::Kind::F8:    return llvm::Type::getBFloatTy(context_);  // bfloat16
        case Type::Kind::F16:   return llvm::Type::getHalfTy(context_);    // half
        case Type::Kind::F32:   return llvm::Type::getFloatTy(context_);   // float
        case Type::Kind::F64:   return llvm::Type::getDoubleTy(context_);  // double
        case Type::Kind::F128:  
            // softwaref128implementation：usetruepositiveof/thefp128types
            // LLVMwillself/fromdynamiclinkcompiler-rtProvide software operations（highperformancecan）
            return llvm::Type::getFP128Ty(context_);
        
        // Other types
        case Type::Kind::Bool:   return llvm::Type::getInt1Ty(context_);
        case Type::Kind::Char:   return llvm::Type::getInt8Ty(context_);
        case Type::Kind::String: return llvm::PointerType::getUnqual(context_);
        case Type::Kind::Void:   return llvm::Type::getVoidTy(context_);
        
        default:
            return llvm::PointerType::getUnqual(context_);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// compositetypesmap
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::ArrayType* TypeCodeGen::mapArrayType(ArrayType* type) {
    llvm::Type* element_type = mapType(type->getElementType());
    return llvm::ArrayType::get(element_type, type->getSize());
}

llvm::StructType* TypeCodeGen::mapSliceType(SliceType* type) {
    // Slice<T> = { ptr data, i64 len }
    // Slice is a dynamic-size array view (fat pointer)
    return llvm::StructType::get(
        context_,
        {
            llvm::PointerType::getUnqual(context_),  // datapointer
            llvm::Type::getInt64Ty(context_)         // length
        }
    );
}

llvm::StructType* TypeCodeGen::mapTupleType(TupleType* type) {
    std::vector<llvm::Type*> element_types;
    for (auto* elem_type : type->getElementTypes()) {
        element_types.push_back(mapType(elem_type));
    }
    return llvm::StructType::get(context_, element_types);
}

llvm::StructType* TypeCodeGen::mapStructType(StructType* type) {
    // 🔧 Key fix：through/vianamelookupalreadyexiststypes，ensureglobalunique
    std::string struct_name = type->getName();
    
    // 1. firstFirstthrough/vianamelookup existing LLVM types
    llvm::StructType* existing_type = llvm::StructType::getTypeByName(context_, struct_name);
    if (existing_type) {
        // type already exists，directlyreturn
        cache_[type] = existing_type;
        return existing_type;
    }
    
    // 2. notexists，create opaque type
    llvm::StructType* new_type = llvm::StructType::create(context_, struct_name);
    
    // immediatelycache
    cache_[type] = new_type;
    
    // 3. generate field types
    std::vector<llvm::Type*> field_types;
    for (const auto& [name, field_type] : type->getFields()) {
        field_types.push_back(mapType(field_type));
    }
    
    // 4. setbody
    new_type->setBody(field_types);
    
    return new_type;
}

llvm::StructType* TypeCodeGen::mapEnumType(EnumType* type) {
    // Enum = { i32 tag, union { variants... } }
    // optimizationsolution：computeAllvariantof/themost/lastlarge/bigsize，useinline storage
    
    auto* i32_type = llvm::Type::getInt32Ty(context_);
    
    // Compute max size of all variants (use pointer size as estimate)
    size_t max_size = 0;
    for (const auto& [variant_name, variant_type] : type->getVariants()) {
        if (variant_type) {
            llvm::Type* variant_llvm_type = mapType(variant_type);
            
            // estimatesize（based ontypes）
            size_t variant_size = 0;
            if (variant_llvm_type->isIntegerTy()) {
                variant_size = variant_llvm_type->getIntegerBitWidth() / 8;
            } else if (variant_llvm_type->isPointerTy()) {
                variant_size = 8;  // pointersize
            } else if (variant_llvm_type->isFloatTy() || variant_llvm_type->isHalfTy()) {
                variant_size = 4;
            } else if (variant_llvm_type->isDoubleTy()) {
                variant_size = 8;
            } else if (variant_llvm_type->isStructTy()) {
                // Struct: estimate as sum of all field sizes (simplified)
                variant_size = 16;  // Conservative estimate
            } else {
                variant_size = 8;  // default
            }
            
            if (variant_size > max_size) {
                max_size = variant_size;
            }
        }
    }
    
    // ifAllvariantallnodata，only needs towant/needtag
    if (max_size == 0) {
        std::vector<llvm::Type*> fields = {i32_type};
        return llvm::StructType::get(context_, fields);
    }
    
    // If max_size is small (<= 24 bytes), use inline storage
    // Otherwise use pointer (avoid enum being too large)
    if (max_size <= 24) {
        // { i32 tag, [max_size x i8] data }
        auto* data_type = llvm::ArrayType::get(
            llvm::Type::getInt8Ty(context_),
            max_size
        );
        std::vector<llvm::Type*> fields = {i32_type, data_type};
        return llvm::StructType::get(context_, fields);
    } else {
        // { i32 tag, ptr data } - For large variants use heap allocation
        auto* ptr_type = llvm::PointerType::getUnqual(context_);
        std::vector<llvm::Type*> fields = {i32_type, ptr_type};
        return llvm::StructType::get(context_, fields);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// specialtypesmap
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::StructType* TypeCodeGen::mapOptionalType(OptionalType* type) {
    // Optional<T> = { i1 has_value, T value }
    llvm::Type* inner_type = mapType(type->getInnerType());
    return llvm::StructType::get(
        context_,
        {
            llvm::Type::getInt1Ty(context_),
            inner_type
        }
    );
}

llvm::StructType* TypeCodeGen::mapResultType(ResultType* type) {
    // Result<T> = { i1 is_ok, T ok_value, ptr err_msg }
    llvm::Type* ok_type = mapType(type->getOkType());
    return llvm::StructType::get(
        context_,
        {
            llvm::Type::getInt1Ty(context_),
            ok_type,
            llvm::PointerType::getUnqual(context_)
        }
    );
}

llvm::PointerType* TypeCodeGen::mapReferenceType(ReferenceType* type) {
    // Return opaque pointer (LLVM opaque pointer model)
    return llvm::PointerType::getUnqual(context_);
}

llvm::FunctionType* TypeCodeGen::mapFunctionType(FunctionType* type) {
    llvm::Type* return_type = mapType(type->getReturnType());
    std::vector<llvm::Type*> param_types;
    for (auto* param_type : type->getParamTypes()) {
        param_types.push_back(mapType(param_type));
    }
    return llvm::FunctionType::get(return_type, param_types, false);
}

} // namespace pawc

