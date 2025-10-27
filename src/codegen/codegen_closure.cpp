/**
 * @file codegen_closure.cpp
 * @brief 闭包代码生成实现
 * 
 * Phase 1: 简单闭包（无环境捕获）
 * Phase 2: 环境捕获闭包
 */

#include "codegen.h"
#include "../parser/ast.h"
#include "../parser/closure_analyzer.h"
#include <iostream>

namespace pawc {

llvm::Value* CodeGenerator::generateClosureExpr(const ClosureExpr* expr) {
    // Phase 2: 支持环境捕获
    // (x: i32, y: i32) -> i32 { x + y + captured_var }
    
    std::string closure_name = "closure_" + std::to_string(closure_counter_++);
    
    // 分析捕获的变量
    std::set<std::string> available_vars;
    for (const auto& pair : named_values_) {
        available_vars.insert(pair.first);
    }
    
    std::vector<std::string> captures = ClosureAnalyzer::analyzeCapturedVars(expr, available_vars);
    
    // 如果有捕获，生成捕获闭包
    if (!captures.empty()) {
        return generateCapturingClosure(expr, captures);
    }
    
    // 否则生成简单闭包（Phase 1 逻辑）
    return generateSimpleClosure(expr);
}

llvm::Value* CodeGenerator::generateSimpleClosure(const ClosureExpr* expr) {
    // 无捕获的简单闭包
    std::string closure_name = "simple_closure_" + std::to_string(closure_counter_++);
    
    // 1. 构造参数类型列表
    std::vector<llvm::Type*> param_types;
    for (const auto& param : expr->params) {
        if (!param.type) {
            std::cerr << "Error: Closure parameter '" << param.name 
                      << "' must have explicit type in Phase 1\n";
            return nullptr;
        }
        llvm::Type* llvm_type = convertType(param.type.get());
        if (!llvm_type) {
            std::cerr << "Error: Invalid type for parameter '" << param.name << "'\n";
            return nullptr;
        }
        param_types.push_back(llvm_type);
    }
    
    // 2. 确定返回类型
    llvm::Type* return_llvm_type = nullptr;
    if (expr->return_type) {
        // 显式返回类型
        return_llvm_type = convertType(expr->return_type.get());
    } else {
        // 需要从 body 推导（Phase 1: 先用 void）
        return_llvm_type = deduceClosureReturnType(expr->body.get());
        if (!return_llvm_type) {
            return_llvm_type = llvm::Type::getVoidTy(*context_);
        }
    }
    
    // 3. 创建函数类型
    llvm::FunctionType* fn_type = llvm::FunctionType::get(
        return_llvm_type,
        param_types,
        false  // not vararg
    );
    
    // 4. 创建内部函数
    llvm::Function* closure_fn = llvm::Function::Create(
        fn_type,
        llvm::Function::InternalLinkage,
        closure_name,
        module_.get()
    );
    
    // 5. 生成函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", closure_fn);
    llvm::BasicBlock* saved_block = builder_->GetInsertBlock();
    builder_->SetInsertPoint(entry);
    
    // 保存当前的 named_values_（进入新作用域）
    auto saved_named_values = named_values_;
    
    // 6. 绑定参数
    size_t param_idx = 0;
    for (auto& arg : closure_fn->args()) {
        std::string param_name = expr->params[param_idx].name;
        arg.setName(param_name);
        
        // 为参数分配栈空间
        llvm::AllocaInst* alloca = builder_->CreateAlloca(
            arg.getType(),
            nullptr,
            param_name
        );
        builder_->CreateStore(&arg, alloca);
        named_values_[param_name] = alloca;
        
        param_idx++;
    }
    
    // 7. 生成函数体
    const BlockStmt* block = static_cast<const BlockStmt*>(expr->body.get());
    bool has_return = false;
    
    for (const auto& stmt : block->statements) {
        generateStmt(stmt.get());
        
        // 检查是否有 return 语句
        if (stmt->kind == Stmt::Kind::Return) {
            has_return = true;
        }
    }
    
    // 8. 如果没有显式 return，添加默认 return
    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (return_llvm_type->isVoidTy()) {
            builder_->CreateRetVoid();
        } else if (!has_return && !block->statements.empty()) {
            // 检查最后一个语句是否是表达式
            const auto& last_stmt = block->statements.back();
            if (last_stmt->kind == Stmt::Kind::Expression) {
                // 最后一个表达式作为返回值（Rust 风格）
                const ExprStmt* expr_stmt = static_cast<const ExprStmt*>(last_stmt.get());
                llvm::Value* ret_val = generateExpr(expr_stmt->expression.get());
                if (ret_val) {
                    builder_->CreateRet(ret_val);
                } else {
                    builder_->CreateRetVoid();
                }
            } else {
                builder_->CreateRetVoid();
            }
        }
    }
    
    // 恢复之前的作用域
    named_values_ = saved_named_values;
    builder_->SetInsertPoint(saved_block);
    
    // 9. 返回函数指针
    return closure_fn;
}

// 推导闭包返回类型
llvm::Type* CodeGenerator::deduceClosureReturnType(const Stmt* body) {
    // 简单实现：查找 return 语句
    const BlockStmt* block = static_cast<const BlockStmt*>(body);
    
    for (const auto& stmt : block->statements) {
        if (stmt->kind == Stmt::Kind::Return) {
            const ReturnStmt* ret_stmt = static_cast<const ReturnStmt*>(stmt.get());
            if (ret_stmt->value) {
                // 从返回表达式推导类型（需要先生成表达式获取LLVM类型）
                // Phase 1: 简化处理，返回 i32
                // TODO: 实现完整的类型推导
                return llvm::Type::getInt32Ty(*context_);
            }
        }
    }
    
    // 检查最后一个语句是否是表达式
    if (!block->statements.empty()) {
        const auto& last_stmt = block->statements.back();
        if (last_stmt->kind == Stmt::Kind::Expression) {
            // Phase 1: 简化处理
            // TODO: 从表达式推导类型
            return llvm::Type::getInt32Ty(*context_);
        }
    }
    
    // 默认 void
    return llvm::Type::getVoidTy(*context_);
}

} // namespace pawc

