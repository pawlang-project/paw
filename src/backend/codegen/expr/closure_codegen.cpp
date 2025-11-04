//===--- closure_codegen.cpp - Closure CodeGen ------------------*- C++ -*-===//
//
// 闭包代码生成：完整实现 Phase 2-3
// Phase 2: 环境捕获
// Phase 3: 闭包对象和调用
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "middleend/types/generic_types.h"
#include <llvm/IR/Function.h>
#include <sstream>
#include <iostream>

namespace pawc {

void ExprCodeGen::visit(ClosureExpr* node) {
    // 完整的闭包实现（Phase 1-3）
    // Phase 1: 基础函数生成 ✅
    // Phase 2: 环境捕获 ✅
    // Phase 3: 闭包对象 ✅
    
    auto& builder = context_->getBuilder();
    static int closure_counter = 0;
    
    // 生成唯一的闭包函数名
    std::stringstream ss;
    ss << "closure_" << closure_counter++;
    std::string closure_fn_name = ss.str();
    node->setGeneratedName(closure_fn_name);
    
    // === Phase 2: 环境捕获分析 ===
    // 捕获变量分析在Sema阶段完成（CaptureAnalyzer）
    // 优点：分离关注点，语义分析和代码生成解耦
    // CodeGen只需使用分析结果生成闭包环境
    const auto& captured_vars = node->getCapturedVars();
    
    // === Phase 3: 闭包对象设计 ===
    // 闭包对象 = { 函数指针, 捕获变量... }
    // 但LLVM不允许直接分配函数类型，所以我们：
    // 1. 如果没有捕获变量：返回函数指针
    // 2. 如果有捕获变量：返回闭包结构体指针
    
    // 构建参数类型（不包括闭包上下文）
    std::vector<llvm::Type*> param_types;
    for (const auto& param : node->getParams()) {
        param_types.push_back(context_->getLLVMType(param.type));
    }
    
    // 返回类型
    llvm::Type* return_type = node->getReturnType() ?
        context_->getLLVMType(node->getReturnType()) :
        builder.getVoidTy();
    
    // === 情况1: 无捕获变量 - 简单函数指针 ===
    if (captured_vars.empty()) {
        // 创建普通函数
        auto* fn_type = llvm::FunctionType::get(return_type, param_types, false);
        llvm::Function* closure_fn = llvm::Function::Create(
            fn_type,
            llvm::Function::InternalLinkage,
            closure_fn_name,
            context_->getModule()
        );
        
        // 保存当前插入点
        llvm::BasicBlock* saved_bb = builder.GetInsertBlock();
        
        // 创建函数entry块
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(
            builder.getContext(),
            "entry",
            closure_fn
        );
        builder.SetInsertPoint(entry_bb);
        
        // === 完整函数体生成 ===
        // 1. 进入闭包函数作用域
        context_->enterScope();
        
        // 2. 绑定参数到符号表
        size_t param_idx = 0;
        for (auto& arg : closure_fn->args()) {
            const auto& param = node->getParams()[param_idx];
            arg.setName(param.name);
            
            // 创建alloca存储参数
            llvm::AllocaInst* param_alloca = context_->createEntryBlockAlloca(
                closure_fn,
                param.name,
                arg.getType()
            );
            builder.CreateStore(&arg, param_alloca);
            context_->defineVariable(param.name, param_alloca);
            
            param_idx++;
        }
        
        // 3. 生成闭包body
        llvm::Value* body_result = nullptr;
        if (node->getBody()) {
            node->getBody()->accept(this);
            body_result = result_;  // 🔧 保存body的返回值
        }
        
        // 4. 如果没有终止指令，添加默认return
        if (!builder.GetInsertBlock()->getTerminator()) {
            if (return_type->isVoidTy()) {
                builder.CreateRetVoid();
            } else if (body_result) {
                // 🔧 如果body产生了值，返回它（隐式返回）
                builder.CreateRet(body_result);
            } else {
                // 如果body没有产生值，返回零值
                llvm::Value* zero = llvm::Constant::getNullValue(return_type);
                builder.CreateRet(zero);
            }
        }
        
        // 5. 退出作用域
        context_->exitScope();
        
        // 恢复插入点
        builder.SetInsertPoint(saved_bb);
        
        // 返回函数指针
        result_ = closure_fn;
        return;
    }
    
    // === 情况2: 有捕获变量 - 闭包结构体 ===
    // 结构体: { fn_ptr, captured_var1, captured_var2, ... }
    std::vector<llvm::Type*> closure_struct_fields;
    
    // 字段0: 函数指针（带闭包上下文参数）
    std::vector<llvm::Type*> fn_param_types_with_ctx;
    fn_param_types_with_ctx.push_back(
        llvm::PointerType::getUnqual(builder.getContext())  // 闭包上下文指针
    );
    for (auto* param_type : param_types) {
        fn_param_types_with_ctx.push_back(param_type);
    }
    
    auto* fn_type_with_ctx = llvm::FunctionType::get(
        return_type,
        fn_param_types_with_ctx,
        false
    );
    auto* fn_ptr_type = llvm::PointerType::getUnqual(fn_type_with_ctx);
    closure_struct_fields.push_back(fn_ptr_type);
    
    // 字段1+: 捕获的变量
    for (const auto& captured : captured_vars) {
        closure_struct_fields.push_back(context_->getLLVMType(captured.type));
    }
    
    // 创建闭包结构体类型
    llvm::StructType* closure_struct_type = llvm::StructType::create(
        builder.getContext(),
        closure_struct_fields,
        "closure_t_" + closure_fn_name
    );
    
    // 创建闭包函数（带上下文参数）
    llvm::Function* closure_fn = llvm::Function::Create(
        fn_type_with_ctx,
        llvm::Function::InternalLinkage,
        closure_fn_name,
        context_->getModule()
    );
    
    // 保存当前插入点
    llvm::BasicBlock* saved_bb = builder.GetInsertBlock();
    
    // 创建函数entry块
    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(
        builder.getContext(),
        "entry",
        closure_fn
    );
    builder.SetInsertPoint(entry_bb);
    
    // === 完整函数体生成（带捕获变量）===
    // 1. 进入闭包函数作用域
    context_->enterScope();
    
    // 2. 绑定参数到符号表（跳过第一个上下文参数）
    auto args_iter = closure_fn->arg_begin();
    args_iter++;  // 跳过闭包上下文指针
    
    size_t param_idx = 0;
    for (; args_iter != closure_fn->arg_end(); ++args_iter) {
        const auto& param = node->getParams()[param_idx];
        args_iter->setName(param.name);
        
        // 创建alloca存储参数
        llvm::AllocaInst* param_alloca = context_->createEntryBlockAlloca(
            closure_fn,
            param.name,
            args_iter->getType()
        );
        builder.CreateStore(&(*args_iter), param_alloca);
        context_->defineVariable(param.name, param_alloca);
        
        param_idx++;
    }
    
    // 3. 从闭包上下文提取捕获变量并绑定到符号表
    if (!captured_vars.empty()) {
        // 获取闭包上下文指针（第一个参数）
        llvm::Value* ctx_ptr = &(*closure_fn->arg_begin());
        ctx_ptr->setName("closure_ctx");
        
        // 从闭包结构体提取每个捕获变量
        for (size_t i = 0; i < captured_vars.size(); ++i) {
            const auto& captured = captured_vars[i];
            
            // 获取变量字段指针（字段0是函数指针，字段1+是捕获变量）
            llvm::Value* var_ptr = builder.CreateStructGEP(
                closure_struct_type,
                ctx_ptr,
                i + 1,  // +1跳过函数指针
                captured.name
            );
            
            // 将捕获变量添加到符号表（指向结构体内的值）
            context_->defineVariable(captured.name, var_ptr);
        }
    }
    
    // 4. 生成闭包body
    llvm::Value* body_result = nullptr;
    if (node->getBody()) {
        node->getBody()->accept(this);
        body_result = result_;  // 🔧 保存body的返回值
    }
    
    // 5. 如果没有终止指令，添加默认return
    if (!builder.GetInsertBlock()->getTerminator()) {
        if (return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else if (body_result) {
            // 🔧 如果body产生了值，返回它（隐式返回）
            builder.CreateRet(body_result);
        } else {
            llvm::Value* zero = llvm::Constant::getNullValue(return_type);
            builder.CreateRet(zero);
        }
    }
    
    // 6. 退出作用域
    context_->exitScope();
    
    // 恢复插入点
    builder.SetInsertPoint(saved_bb);
    
    // === Phase 3: 创建闭包对象 ===
    // 在堆上分配闭包结构体（因为闭包可能逃逸）
    llvm::Value* closure_size = llvm::ConstantExpr::getSizeOf(closure_struct_type);
    
    // 调用malloc分配内存
    llvm::FunctionType* malloc_type = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(builder.getContext()),
        {builder.getInt64Ty()},
        false
    );
    llvm::FunctionCallee malloc_fn = context_->getModule()->getOrInsertFunction(
        "malloc",
        malloc_type
    );
    
    llvm::Value* closure_ptr = builder.CreateCall(
        malloc_fn,
        {builder.CreateIntCast(closure_size, builder.getInt64Ty(), false)}
    );
    
    // Cast到闭包结构体指针
    llvm::Value* typed_closure_ptr = builder.CreateBitCast(
        closure_ptr,
        llvm::PointerType::getUnqual(closure_struct_type)
    );
    
    // 设置函数指针字段
    llvm::Value* fn_ptr_field = builder.CreateStructGEP(
        closure_struct_type,
        typed_closure_ptr,
        0,
        "fn_ptr"
    );
    builder.CreateStore(closure_fn, fn_ptr_field);
    
    // 捕获变量（从当前作用域）
    for (size_t i = 0; i < captured_vars.size(); ++i) {
        const auto& captured = captured_vars[i];
        
        // 查找变量
        llvm::Value* var_value = context_->lookupVariable(captured.name);
        if (!var_value || !captured.type) {
            continue;
        }
        
        // 获取变量字段指针
        llvm::Value* var_field = builder.CreateStructGEP(
            closure_struct_type,
            typed_closure_ptr,
            i + 1,  // +1因为第0个是函数指针
            "captured_" + captured.name
        );
        
        // 加载并存储值
        llvm::Value* loaded_value = builder.CreateLoad(
            context_->getLLVMType(captured.type),
            var_value,
            captured.name + "_val"
        );
        builder.CreateStore(loaded_value, var_field);
    }
    
    // 返回闭包指针
    result_ = typed_closure_ptr;
}

} // namespace pawc
