//===--- type_codegen.cpp - Type Mapping Implementation ----------*- C++ -*-===//

#include "type_codegen.h"
#include "middleend/types/primitive_types.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/generic_types.h"
#include <iostream>

namespace pawc {

TypeCodeGen::TypeCodeGen(llvm::LLVMContext& llvm_context)
    : context_(llvm_context) {}

llvm::Type* TypeCodeGen::mapType(Type* paw_type) {
    // 检查缓存
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
        // 默认：i8指针（opaque pointer）
        llvm_type = llvm::PointerType::getUnqual(context_);
    }
    
    // 缓存
    cache_[paw_type] = llvm_type;
    
    return llvm_type;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 基础类型映射 (18种) - 完整精度
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Type* TypeCodeGen::mapPrimitiveType(Type* type) {
    switch (type->getKind()) {
        // 有符号整数
        case Type::Kind::I8:    return llvm::Type::getInt8Ty(context_);
        case Type::Kind::I16:   return llvm::Type::getInt16Ty(context_);
        case Type::Kind::I32:   return llvm::Type::getInt32Ty(context_);
        case Type::Kind::I64:   return llvm::Type::getInt64Ty(context_);
        case Type::Kind::I128:  return llvm::Type::getInt128Ty(context_);
        
        // 无符号整数
        case Type::Kind::U8:    return llvm::Type::getInt8Ty(context_);
        case Type::Kind::U16:   return llvm::Type::getInt16Ty(context_);
        case Type::Kind::U32:   return llvm::Type::getInt32Ty(context_);
        case Type::Kind::U64:   return llvm::Type::getInt64Ty(context_);
        case Type::Kind::U128:  return llvm::Type::getInt128Ty(context_);
        
        // 浮点数 - 完整精度，不近似
        case Type::Kind::F8:    return llvm::Type::getBFloatTy(context_);  // bfloat16
        case Type::Kind::F16:   return llvm::Type::getHalfTy(context_);    // half
        case Type::Kind::F32:   return llvm::Type::getFloatTy(context_);   // float
        case Type::Kind::F64:   return llvm::Type::getDoubleTy(context_);  // double
        case Type::Kind::F128:  
            // 软件f128实现：使用真正的fp128类型
            // LLVM会自动链接compiler-rt提供软件运算（高性能）
            return llvm::Type::getFP128Ty(context_);
        
        // 其他
        case Type::Kind::Bool:   return llvm::Type::getInt1Ty(context_);
        case Type::Kind::Char:   return llvm::Type::getInt8Ty(context_);
        case Type::Kind::String: return llvm::PointerType::getUnqual(context_);
        case Type::Kind::Void:   return llvm::Type::getVoidTy(context_);
        
        default:
            return llvm::PointerType::getUnqual(context_);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 复合类型映射
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::ArrayType* TypeCodeGen::mapArrayType(ArrayType* type) {
    llvm::Type* element_type = mapType(type->getElementType());
    return llvm::ArrayType::get(element_type, type->getSize());
}

llvm::StructType* TypeCodeGen::mapSliceType(SliceType* type) {
    // Slice<T> = { ptr data, i64 len }
    // 切片是动态大小的数组视图（胖指针）
    return llvm::StructType::get(
        context_,
        {
            llvm::PointerType::getUnqual(context_),  // data指针
            llvm::Type::getInt64Ty(context_)         // 长度
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
    // 🔧 关键修复：通过名称查找已存在类型，确保全局唯一
    std::string struct_name = type->getName();
    
    // 1. 首先通过名称查找已存在的LLVM类型
    llvm::StructType* existing_type = llvm::StructType::getTypeByName(context_, struct_name);
    if (existing_type) {
        // 类型已存在，直接返回
        cache_[type] = existing_type;
        return existing_type;
    }
    
    // 2. 不存在，创建opaque类型
    llvm::StructType* new_type = llvm::StructType::create(context_, struct_name);
    
    // 立即缓存
    cache_[type] = new_type;
    
    // 3. 生成字段类型
    std::vector<llvm::Type*> field_types;
    for (const auto& [name, field_type] : type->getFields()) {
        field_types.push_back(mapType(field_type));
    }
    
    // 4. 设置body
    new_type->setBody(field_types);
    
    return new_type;
}

llvm::StructType* TypeCodeGen::mapEnumType(EnumType* type) {
    // Enum = { i32 tag, union { variants... } }
    // 优化方案：计算所有variant的最大size，使用inline storage
    
    auto* i32_type = llvm::Type::getInt32Ty(context_);
    
    // 计算所有variant的最大size（使用指针大小作为估算）
    size_t max_size = 0;
    for (const auto& [variant_name, variant_type] : type->getVariants()) {
        if (variant_type) {
            llvm::Type* variant_llvm_type = mapType(variant_type);
            
            // 估算大小（基于类型）
            size_t variant_size = 0;
            if (variant_llvm_type->isIntegerTy()) {
                variant_size = variant_llvm_type->getIntegerBitWidth() / 8;
            } else if (variant_llvm_type->isPointerTy()) {
                variant_size = 8;  // 指针大小
            } else if (variant_llvm_type->isFloatTy() || variant_llvm_type->isHalfTy()) {
                variant_size = 4;
            } else if (variant_llvm_type->isDoubleTy()) {
                variant_size = 8;
            } else if (variant_llvm_type->isStructTy()) {
                // 结构体：估算为所有字段大小之和（简化）
                variant_size = 16;  // 保守估计
            } else {
                variant_size = 8;  // 默认
            }
            
            if (variant_size > max_size) {
                max_size = variant_size;
            }
        }
    }
    
    // 如果所有variant都无数据，只需要tag
    if (max_size == 0) {
        std::vector<llvm::Type*> fields = {i32_type};
        return llvm::StructType::get(context_, fields);
    }
    
    // 如果max_size较小（<= 24字节），使用inline storage
    // 否则使用指针（避免enum太大）
    if (max_size <= 24) {
        // { i32 tag, [max_size x i8] data }
        auto* data_type = llvm::ArrayType::get(
            llvm::Type::getInt8Ty(context_),
            max_size
        );
        std::vector<llvm::Type*> fields = {i32_type, data_type};
        return llvm::StructType::get(context_, fields);
    } else {
        // { i32 tag, ptr data } - 对于大型variant使用堆分配
        auto* ptr_type = llvm::PointerType::getUnqual(context_);
        std::vector<llvm::Type*> fields = {i32_type, ptr_type};
        return llvm::StructType::get(context_, fields);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 特殊类型映射
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
    llvm::Type* pointee_type = mapType(type->getPointeeType());
    return pointee_type->getPointerTo();
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

