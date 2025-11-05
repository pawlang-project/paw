//===--- type_codegen.h - Type Code Generation -------------------*- C++ -*-===//
//
// PawLang Compiler - Type Mapping (28 Types → LLVM)
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_CODEGEN_H
#define PAW_TYPE_CODEGEN_H

#include "middleend/types/type.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/generic_types.h"
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <unordered_map>

namespace pawc {

/// TypeCodeGen - typesmaplinker/er
///
/// responsibilities：will28types/kindsPawLangtypesmaptoLLVMtypes
class TypeCodeGen {
public:
    TypeCodeGen(llvm::LLVMContext& llvm_context);
    
    /// mainentry point：maparbitrary/anyPawLangtypes
    llvm::Type* mapType(Type* paw_type);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // basetypesmap (18types/kinds)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::Type* mapPrimitiveType(Type* type);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // compositetypesmap
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::ArrayType* mapArrayType(ArrayType* type);
    llvm::StructType* mapSliceType(SliceType* type);
    llvm::StructType* mapTupleType(TupleType* type);
    llvm::StructType* mapStructType(StructType* type);
    llvm::StructType* mapEnumType(EnumType* type);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // specialtypesmap
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::StructType* mapOptionalType(OptionalType* type);
    llvm::StructType* mapResultType(ResultType* type);
    llvm::PointerType* mapReferenceType(ReferenceType* type);
    llvm::FunctionType* mapFunctionType(FunctionType* type);
    
private:
    llvm::LLVMContext& context_;
    std::unordered_map<Type*, llvm::Type*> cache_;
};

} // namespace pawc

#endif // PAW_TYPE_CODEGEN_H

