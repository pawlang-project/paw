/**
 * @file codegen_closure_capture.cpp
 * @brief 环境捕获闭包的代码生成
 */

#include "codegen.h"
#include "../parser/ast.h"
#include <iostream>

namespace pawc {

llvm::Value* CodeGenerator::generateCapturingClosure(
    const ClosureExpr* expr,
    const std::vector<std::string>& captures
) {
    // 生成捕获环境的闭包
    // (y: i32) -> i32 { x + y }  其中 x 被捕获
    
    std::cerr << "[DEBUG] Generating capturing closure with " << captures.size() << " captures" << std::endl;
    for (const auto& var : captures) {
        std::cerr << "  - Capturing: " << var << std::endl;
    }
    
    std::string closure_name = "capturing_closure_" + std::to_string(closure_counter_++);
    
    // 1. 收集捕获变量的类型和值
    std::vector<llvm::Type*> env_types;
    std::vector<llvm::Value*> env_values;
    
    for (const auto& var_name : captures) {
        auto it = named_values_.find(var_name);
        if (it != named_values_.end()) {
            llvm::Value* var_ptr = it->second;  // alloca
            
            // 获取变量类型
            auto type_it = variable_types_.find(var_name);
            if (type_it != variable_types_.end()) {
                llvm::Type* var_type = type_it->second;
                env_types.push_back(var_type);
                
                // 加载值
                llvm::Value* val = builder_->CreateLoad(var_type, var_ptr, var_name + "_captured");
                env_values.push_back(val);
            } else {
                std::cerr << "Warning: Type info not found for captured var: " << var_name << std::endl;
            }
        }
    }
    
    // 2. 创建环境结构体类型
    llvm::StructType* env_struct_type = llvm::StructType::create(
        *context_, env_types, closure_name + "_env"
    );
    
    // 3. 分配并填充环境
    llvm::Value* env_alloca = builder_->CreateAlloca(env_struct_type, nullptr, "env");
    
    for (size_t i = 0; i < env_values.size(); ++i) {
        llvm::Value* field_ptr = builder_->CreateStructGEP(env_struct_type, env_alloca, i);
        builder_->CreateStore(env_values[i], field_ptr);
    }
    
    // 4. 构造闭包函数参数类型（第一个参数是 env*）
    std::vector<llvm::Type*> param_types;
    param_types.push_back(llvm::PointerType::get(*context_, 0));  // env*
    
    for (size_t i = 0; i < expr->params.size(); ++i) {
        const auto& param = expr->params[i];
        
        if (!param.type) {
            // 需要推导
            llvm::Type* deduced_type = deduceClosureParamType(expr, i);
            if (!deduced_type) {
                std::cerr << "Error: Cannot deduce type for parameter '" << param.name 
                          << "'. Please provide explicit type annotation.\n";
                return nullptr;
            }
            param_types.push_back(deduced_type);
        } else {
            // 显式类型
            param_types.push_back(convertType(param.type.get()));
        }
    }
    
    // 5. 确定返回类型
    llvm::Type* return_type = nullptr;
    if (expr->return_type) {
        return_type = convertType(expr->return_type.get());
    } else {
        return_type = deduceClosureReturnType(expr->body.get());
        if (!return_type) {
            return_type = llvm::Type::getVoidTy(*context_);
        }
    }
    
    // 6. 创建闭包函数
    llvm::FunctionType* fn_type = llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function* closure_fn = llvm::Function::Create(
        fn_type,
        llvm::Function::InternalLinkage,
        closure_name,
        module_.get()
    );
    
    // 7. 生成函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", closure_fn);
    llvm::BasicBlock* saved_block = builder_->GetInsertBlock();
    builder_->SetInsertPoint(entry);
    
    // 保存当前作用域
    auto saved_named_values = named_values_;
    auto saved_variable_types = variable_types_;
    
    // 8. 从环境恢复捕获的变量
    auto env_arg = closure_fn->arg_begin();
    env_arg->setName("env");
    
    for (size_t i = 0; i < captures.size(); ++i) {
        const std::string& var_name = captures[i];
        llvm::Type* var_type = env_types[i];
        
        // 从环境加载
        llvm::Value* field_ptr = builder_->CreateStructGEP(env_struct_type, env_arg, i);
        llvm::Value* val = builder_->CreateLoad(var_type, field_ptr, var_name + "_from_env");
        
        // 创建局部变量
        llvm::AllocaInst* local = builder_->CreateAlloca(var_type, nullptr, var_name);
        builder_->CreateStore(val, local);
        
        named_values_[var_name] = local;
        variable_types_[var_name] = var_type;
    }
    
    // 9. 绑定闭包参数（跳过第一个 env 参数）
    auto arg_it = closure_fn->arg_begin();
    ++arg_it;  // skip env
    
    size_t param_idx = 0;
    for (; arg_it != closure_fn->arg_end(); ++arg_it, ++param_idx) {
        std::string param_name = expr->params[param_idx].name;
        arg_it->setName(param_name);
        
        llvm::Type* param_type = convertType(expr->params[param_idx].type.get());
        llvm::AllocaInst* alloca = builder_->CreateAlloca(param_type, nullptr, param_name);
        builder_->CreateStore(&(*arg_it), alloca);
        
        named_values_[param_name] = alloca;
        variable_types_[param_name] = param_type;
    }
    
    // 10. 生成函数体
    const BlockStmt* block = static_cast<const BlockStmt*>(expr->body.get());
    bool has_return = false;
    
    for (const auto& stmt : block->statements) {
        generateStmt(stmt.get());
        if (stmt->kind == Stmt::Kind::Return) {
            has_return = true;
        }
    }
    
    // 11. 默认返回
    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (return_type->isVoidTy()) {
            builder_->CreateRetVoid();
        } else if (!has_return && !block->statements.empty()) {
            const auto& last_stmt = block->statements.back();
            if (last_stmt->kind == Stmt::Kind::Expression) {
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
    
    // 恢复作用域
    named_values_ = saved_named_values;
    variable_types_ = saved_variable_types;
    builder_->SetInsertPoint(saved_block);
    
    // 12. 创建闭包结构 { fn*, env* }
    llvm::StructType* closure_struct = llvm::StructType::create(
        *context_,
        {llvm::PointerType::get(*context_, 0), llvm::PointerType::get(*context_, 0)},
        closure_name + "_struct"
    );
    
    llvm::Value* closure_val = builder_->CreateAlloca(closure_struct, nullptr, "closure");
    
    // 存储函数指针
    llvm::Value* fn_ptr_field = builder_->CreateStructGEP(closure_struct, closure_val, 0);
    builder_->CreateStore(closure_fn, fn_ptr_field);
    
    // 存储环境指针
    llvm::Value* env_ptr_field = builder_->CreateStructGEP(closure_struct, closure_val, 1);
    builder_->CreateStore(env_alloca, env_ptr_field);
    
    // Phase 2: 返回函数指针，并保存环境到临时变量
    last_generated_closure_env_ = env_alloca;
    
    return closure_fn;
}

} // namespace pawc

