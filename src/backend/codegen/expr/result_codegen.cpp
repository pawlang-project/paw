//===--- results_codegen.cpp - Result Type CodeGen ---------------*- C++ -*-===//
/// @file results_codegen.cpp
/// @brief Implementation file
///
//
// Resulttypesrelatedcode generation：NullLiteral, ok/err builtin, TryExpr
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "middleend/types/generic_types.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// NoneLiteral - noneliteralgenerate
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(NoneLiteral* node) {
    // noneliteral：generatecorrecttypesof/theOptional
    // Optionaltypesstruct: { i1 has_value, T value }
    auto& builder = context_->getBuilder();
    
    // fromASTnodegettypes（Semashouldalreadyset）
    Type* none_type = node->getType();
    
    // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
    llvm::Type* optional_llvm_type = nullptr;
    
    if (none_type && none_type->getKind() == Type::Kind::Optional) {
        // useSemainferof/thetypes
        optional_llvm_type = context_->getLLVMType(none_type);
    } else {
        // fallbackbottom：createOptional<void>
        llvm::StructType* optional_type = llvm::StructType::get(
            context_->getLLVMContext(),
            {
                llvm::Type::getInt1Ty(context_->getLLVMContext()),  // has_value
                llvm::Type::getInt8Ty(context_->getLLVMContext())   // voidplaceholder
            }
        );
        optional_llvm_type = optional_type;
    }
    
    // createundefstructbody/struct
    llvm::Value* optional_value = llvm::UndefValue::get(optional_llvm_type);
    
    // sethas_value = false
    optional_value = builder.CreateInsertValue(
        optional_value,
        llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // false
        0,
        "null"
    );
    
    results_ = optional_value;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// TryExpr - ? operationoperatorcode generation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(TryExpr* node) {
    auto& builder = context_->getBuilder();
    
    // generationexpr
    node->getExpr()->accept(this);
    llvm::Value* try_value = results_;
    
    if (!try_value) {
        results_ = nullptr;
        return;
    }
    
    Type* expr_type = node->getExpr()->getType();
    if (!expr_type) {
        results_ = nullptr;
        return;
    }
    
    // === according tooperationoperator typefractionalprocess ===
    if (node->isResultTry()) {
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        // expr! - Result<T> unwrap（errorprocess）
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        
        if (expr_type->getKind() != Type::Kind::Result) {
            results_ = nullptr;
            return;
        }
        
        auto* results_type = static_cast<ResultType*>(expr_type);
        
        // extractis_okfield（index0）
        llvm::Value* is_ok = builder.CreateExtractValue(try_value, 0, "is_ok");
        
        // getcurrentfunction
        llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
        
        // Create basic block
        llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "results_ok", current_fn);
        llvm::BasicBlock* err_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "results_err", current_fn);
        
        // conditionbranch
        builder.CreateCondBr(is_ok, ok_bb, err_bb);
        
        // === errorbranch：extracterrorandpropagate ===
        builder.SetInsertPoint(err_bb);
        
        // extracterror message（index2）
        llvm::Value* err_msg = builder.CreateExtractValue(try_value, 2, "err_msg");
        
        // constructcurrentfunctionof/theResulterrorreturnvalue
        llvm::Type* current_fn_ret_type = current_fn->getReturnType();
        
        // createerrorreturnvalue: { false, undef, err_msg }
        llvm::Value* error_return = llvm::UndefValue::get(current_fn_ret_type);
        error_return = builder.CreateInsertValue(error_return,
            llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // is_ok = false
            0);
        error_return = builder.CreateInsertValue(error_return, err_msg, 2);  // error = err_msg
        
        // returnerror
        builder.CreateRet(error_return);
        
        // === successbranch：extractvalueandcontinue ===
        builder.SetInsertPoint(ok_bb);
        
        // extractvalue（index1）
        results_ = builder.CreateExtractValue(try_value, 1, "value");
    }
    else if (node->isOptionalTry()) {
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        // expr? - Optional<T> unwrap（cannone/null）
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        
        if (expr_type->getKind() != Type::Kind::Optional) {
            results_ = nullptr;
            return;
        }
        
        auto* optional_type = static_cast<OptionalType*>(expr_type);
        
        // Optionalstructbody/struct: { i1 has_value, T value }
        // extracthas_valuefield（index0）
        llvm::Value* has_value = builder.CreateExtractValue(try_value, 0, "has_value");
        
        // getcurrentfunction
        llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
        
        // Create basic block
        llvm::BasicBlock* some_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "optional_some", current_fn);
        llvm::BasicBlock* none_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "optional_none", current_fn);
        
        // conditionbranch
        builder.CreateCondBr(has_value, some_bb, none_bb);
        
        // === Nonebranch：propagatenull（returnOptional<U>of/theNone） ===
        builder.SetInsertPoint(none_bb);
        
        // ifcurrentfunctionreturnOptional<U>，returnNone
        llvm::Type* current_fn_ret_type = current_fn->getReturnType();
        
        // createNonereturnvalue: { false, undef }
        llvm::Value* none_return = llvm::UndefValue::get(current_fn_ret_type);
        none_return = builder.CreateInsertValue(none_return,
            llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // has_value = false
            0);
        
        // returnNone
        builder.CreateRet(none_return);
        
        // === Somebranch：extractvalueandcontinue ===
        builder.SetInsertPoint(some_bb);
        
        // extractvalue（index1）
        results_ = builder.CreateExtractValue(try_value, 1, "value");
    }
    else {
        // not yetknownoperationoperator
        results_ = nullptr;
    }
}

} // namespace pawc

