//===--- result_codegen.cpp - Result Type CodeGen ---------------*- C++ -*-===//
//
// Result类型相关代码生成：NullLiteral, ok/err builtin, TryExpr
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
// NullLiteral - null字面量生成
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(NullLiteral* node) {
    // null字面量：生成正确类型的Optional
    // Optional类型结构: { i1 has_value, T value }
    auto& builder = context_->getBuilder();
    
    // 从AST节点获取类型（Sema应该已设置）
    Type* null_type = node->getType();
    
    // TypeCodeGen统一使用CodeGenContext::getLLVMType()
    llvm::Type* optional_llvm_type = nullptr;
    
    if (null_type && null_type->getKind() == Type::Kind::Optional) {
        // 使用Sema推导的类型
        optional_llvm_type = context_->getLLVMType(null_type);
    } else {
        // 兜底：创建Optional<void>
        llvm::StructType* optional_type = llvm::StructType::get(
            context_->getLLVMContext(),
            {
                llvm::Type::getInt1Ty(context_->getLLVMContext()),  // has_value
                llvm::Type::getInt8Ty(context_->getLLVMContext())   // void占位
            }
        );
        optional_llvm_type = optional_type;
    }
    
    // 创建undef结构体
    llvm::Value* optional_value = llvm::UndefValue::get(optional_llvm_type);
    
    // 设置has_value = false
    optional_value = builder.CreateInsertValue(
        optional_value,
        llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // false
        0,
        "null"
    );
    
    result_ = optional_value;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// TryExpr - ? 操作符代码生成
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(TryExpr* node) {
    auto& builder = context_->getBuilder();
    
    // 生成expr
    node->getExpr()->accept(this);
    llvm::Value* try_value = result_;
    
    if (!try_value) {
        result_ = nullptr;
        return;
    }
    
    Type* expr_type = node->getExpr()->getType();
    if (!expr_type) {
        result_ = nullptr;
        return;
    }
    
    // === 根据操作符区分处理 ===
    if (node->isResultTry()) {
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        // expr! - Result<T> unwrap（错误处理）
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        
        if (expr_type->getKind() != Type::Kind::Result) {
            result_ = nullptr;
            return;
        }
        
        auto* result_type = static_cast<ResultType*>(expr_type);
        
        // 提取is_ok字段（索引0）
        llvm::Value* is_ok = builder.CreateExtractValue(try_value, 0, "is_ok");
        
        // 获取当前函数
        llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
        
        // 创建基本块
        llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "result_ok", current_fn);
        llvm::BasicBlock* err_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "result_err", current_fn);
        
        // 条件分支
        builder.CreateCondBr(is_ok, ok_bb, err_bb);
        
        // === 错误分支：提取错误并传播 ===
        builder.SetInsertPoint(err_bb);
        
        // 提取error message（索引2）
        llvm::Value* err_msg = builder.CreateExtractValue(try_value, 2, "err_msg");
        
        // 构造当前函数的Result错误返回值
        llvm::Type* current_fn_ret_type = current_fn->getReturnType();
        
        // 创建错误返回值: { false, undef, err_msg }
        llvm::Value* error_return = llvm::UndefValue::get(current_fn_ret_type);
        error_return = builder.CreateInsertValue(error_return,
            llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // is_ok = false
            0);
        error_return = builder.CreateInsertValue(error_return, err_msg, 2);  // error = err_msg
        
        // 返回错误
        builder.CreateRet(error_return);
        
        // === 成功分支：提取value并继续 ===
        builder.SetInsertPoint(ok_bb);
        
        // 提取value（索引1）
        result_ = builder.CreateExtractValue(try_value, 1, "value");
    }
    else if (node->isOptionalTry()) {
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        // expr? - Optional<T> unwrap（可空值）
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        
        if (expr_type->getKind() != Type::Kind::Optional) {
            result_ = nullptr;
            return;
        }
        
        auto* optional_type = static_cast<OptionalType*>(expr_type);
        
        // Optional结构体: { i1 has_value, T value }
        // 提取has_value字段（索引0）
        llvm::Value* has_value = builder.CreateExtractValue(try_value, 0, "has_value");
        
        // 获取当前函数
        llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
        
        // 创建基本块
        llvm::BasicBlock* some_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "optional_some", current_fn);
        llvm::BasicBlock* none_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "optional_none", current_fn);
        
        // 条件分支
        builder.CreateCondBr(has_value, some_bb, none_bb);
        
        // === None分支：传播null（返回Optional<U>的None） ===
        builder.SetInsertPoint(none_bb);
        
        // 如果当前函数返回Optional<U>，返回None
        llvm::Type* current_fn_ret_type = current_fn->getReturnType();
        
        // 创建None返回值: { false, undef }
        llvm::Value* none_return = llvm::UndefValue::get(current_fn_ret_type);
        none_return = builder.CreateInsertValue(none_return,
            llvm::ConstantInt::get(builder.getInt1Ty(), 0),  // has_value = false
            0);
        
        // 返回None
        builder.CreateRet(none_return);
        
        // === Some分支：提取value并继续 ===
        builder.SetInsertPoint(some_bb);
        
        // 提取value（索引1）
        result_ = builder.CreateExtractValue(try_value, 1, "value");
    }
    else {
        // 未知操作符
        result_ = nullptr;
    }
}

} // namespace pawc

