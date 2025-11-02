//===--- stmt_codegen.cpp - Statement CodeGen Implementation -----*- C++ -*-===//

#include "stmt_codegen.h"
#include "../expr/expr_codegen.h"
#include "../type/type_codegen.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>

namespace pawc {

StmtCodeGen::StmtCodeGen(CodeGenContext* context, ExprCodeGen* expr_codegen)
    : CodeGenBase(context), expr_codegen_(expr_codegen), result_(nullptr) {
    // 设置ExprCodeGen的StmtCodeGen引用（用于BlockExpr）
    if (expr_codegen_) {
        expr_codegen_->setStmtCodeGen(this);
    }
}

llvm::Value* StmtCodeGen::generate(ASTNode* node) {
    if (auto* stmt = dynamic_cast<Stmt*>(node)) {
        stmt->accept(this);
        return result_;
    }
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 语句生成
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void StmtCodeGen::visit(ExprStmt* node) {
    result_ = expr_codegen_->generate(node->getExpr());
}

void StmtCodeGen::visit(VarDecl* node) {
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    auto& builder = context_->getBuilder();
    
    // === 特殊处理：FunctionType（闭包）===
    // 闭包类型不能直接alloca，需要存储闭包指针或闭包对象
    if (node->getType() && node->getType()->getKind() == Type::Kind::Function) {
        // 生成闭包初始化表达式
        if (node->getInit()) {
            llvm::Value* closure_value = expr_codegen_->generate(node->getInit());
            
            if (closure_value) {
                // 闭包值可能是：
                // 1. 函数指针（无捕获）
                // 2. 闭包结构体指针（有捕获）
                
                // 创建指针类型的alloca来存储闭包
                llvm::Type* closure_storage_type = closure_value->getType();
                
                llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
                    func,
                    node->getName(),
                    closure_storage_type
                );
                
                builder.CreateStore(closure_value, alloca);
                context_->defineVariable(node->getName(), alloca);
                result_ = alloca;
            }
        }
        return;
    }
    
    // === 普通类型处理 ===
    // 获取变量类型
    TypeCodeGen type_gen(context_->getLLVMContext());
    llvm::Type* var_type = node->getType() ?
        type_gen.mapType(node->getType()) :
        llvm::Type::getInt32Ty(context_->getLLVMContext());
    
    // 创建alloca
    llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
        func,
        node->getName(),
        var_type
    );
    
    // 如果有初始值，生成store
    if (node->getInit()) {
        llvm::Value* init_value = expr_codegen_->generate(node->getInit());
        if (init_value) {
            builder.CreateStore(init_value, alloca);
        }
    }
    
    // 注册变量
    context_->defineVariable(node->getName(), alloca);
    
    result_ = alloca;
}

void StmtCodeGen::visit(DestructuringDecl* node) {
    // 元组解构: let (a, b) = tuple_expr;
    auto& builder = context_->getBuilder();
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!node->getInit()) {
        result_ = nullptr;
        return;
    }
    
    // 生成元组表达式
    llvm::Value* tuple_value = expr_codegen_->generate(node->getInit());
    if (!tuple_value) {
        result_ = nullptr;
        return;
    }
    
    // 为每个变量创建alloca并提取对应的元组元素
    for (size_t i = 0; i < node->getNames().size(); ++i) {
        // 提取元组元素
        llvm::Value* element = builder.CreateExtractValue(tuple_value, i, "tuple.elem." + std::to_string(i));
        
        // 创建变量的alloca
        llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
            func,
            node->getNames()[i],
            element->getType()
        );
        
        // 存储元素值
        builder.CreateStore(element, alloca);
        
        // 注册变量
        context_->defineVariable(node->getNames()[i], alloca);
    }
    
    result_ = tuple_value;
}

void StmtCodeGen::visit(StructDestructuringDecl* node) {
    // 结构体解构: let Point { x, y } = p;
    auto& builder = context_->getBuilder();
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!node->getInit()) {
        result_ = nullptr;
        return;
    }
    
    // 生成结构体表达式
    llvm::Value* struct_value = expr_codegen_->generate(node->getInit());
    if (!struct_value) {
        result_ = nullptr;
        return;
    }
    
    // 为每个字段创建alloca并提取对应的结构体字段
    // 使用ExtractValue而不是GEP（假设struct_value是按值传递）
    for (size_t i = 0; i < node->getFieldNames().size(); ++i) {
        // 直接从结构体值中提取字段
        llvm::Value* field_value = builder.CreateExtractValue(
            struct_value,
            i,
            "struct.field." + node->getFieldNames()[i]
        );
        
        // 创建变量的alloca
        llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
            func,
            node->getFieldNames()[i],
            field_value->getType()
        );
        
        // 存储字段值
        builder.CreateStore(field_value, alloca);
        
        // 注册变量
        context_->defineVariable(node->getFieldNames()[i], alloca);
    }
    
    result_ = struct_value;
}

void StmtCodeGen::visit(FunctionDecl* node) {
    // 创建函数类型
    TypeCodeGen type_gen(context_->getLLVMContext());
    
    std::vector<llvm::Type*> param_types;
    for (const auto& param : node->getParams()) {
        param_types.push_back(type_gen.mapType(param.type));
    }
    
    llvm::Type* ret_type = type_gen.mapType(node->getReturnType());
    auto* func_type = llvm::FunctionType::get(ret_type, param_types, false);
    
    // 创建函数
    // 注意：如果是main函数，使用内部链接，稍后生成C ABI wrapper
    llvm::Function::LinkageTypes linkage = (node->getName() == "main") ?
        llvm::Function::InternalLinkage : llvm::Function::ExternalLinkage;
    
    std::string func_name = (node->getName() == "main") ? 
        "paw.main" : node->getName();
    
    llvm::Function* func = llvm::Function::Create(
        func_type,
        linkage,
        func_name,
        context_->getModule()
    );
    
    // 注册函数
    context_->registerFunction(node->getName(), func);
    
    // 创建entry基本块
    llvm::BasicBlock* entry = context_->createBasicBlock("entry", func);
    context_->getBuilder().SetInsertPoint(entry);
    
    // 进入函数作用域
    context_->enterScope();
    
    // 添加参数到符号表
    size_t i = 0;
    for (auto& arg : func->args()) {
        const auto& param = node->getParams()[i];
        arg.setName(param.name);
        
        // 创建alloca并存储参数值
        TypeCodeGen type_gen_param(context_->getLLVMContext());
        llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
            func,
            param.name,
            type_gen_param.mapType(param.type)
        );
        context_->getBuilder().CreateStore(&arg, alloca);
        context_->defineVariable(param.name, alloca);
        
        i++;
    }
    
    // 生成函数体
    if (node->getBody()) {
        node->getBody()->accept(this);
    }
    
    // 如果没有return，添加默认return
    if (!context_->getCurrentBlock()->getTerminator()) {
        if (func_type->getReturnType()->isVoidTy()) {
            context_->getBuilder().CreateRetVoid();
        }
    }
    
    // 退出函数作用域
    context_->exitScope();
    
    // 特殊处理：为所有main()生成C ABI wrapper
    if (node->getName() == "main") {
        generateMainWrapper(func, func_type->getReturnType());
    }
    
    result_ = func;
}

void StmtCodeGen::generateMainWrapper(llvm::Function* paw_main, llvm::Type* paw_return_type) {
    // 生成C ABI兼容的main函数: int main() { ... return 0/result; }
    auto& builder = context_->getBuilder();
    auto* i32_type = llvm::Type::getInt32Ty(context_->getLLVMContext());
    auto* main_type = llvm::FunctionType::get(i32_type, {}, false);
    
    llvm::Function* c_main = llvm::Function::Create(
        main_type,
        llvm::Function::ExternalLinkage,
        "main",
        context_->getModule()
    );
    
    // 创建entry块
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(
        context_->getLLVMContext(), "entry", c_main);
    
    // 保存当前插入点
    auto saved_block = builder.GetInsertBlock();
    
    // 切换到C main
    builder.SetInsertPoint(entry);
    
    // 调用PawLang的main
    llvm::Value* paw_result = builder.CreateCall(paw_main, {});
    
    // 根据返回类型处理返回值
    if (paw_return_type->isVoidTy()) {
        // void main() -> return 0
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
    } else if (paw_return_type->isIntegerTy(32)) {
        // i32 main() -> return result
        builder.CreateRet(paw_result);
    } else if (paw_return_type->isStructTy()) {
        // Result<i32> main() -> extract value and return
        // Result结构体: { i1 is_ok, T value, ptr error_message }
        // 如果is_ok，返回value；否则返回1（错误码）
        
        // === 关键修复：先alloca Result，再进行GEP ===
        // 创建临时alloca存储Result值
        llvm::AllocaInst* result_alloca = llvm::IRBuilder<>(
            &c_main->getEntryBlock(),
            c_main->getEntryBlock().begin()
        ).CreateAlloca(paw_return_type, nullptr, "result_tmp");
        
        // 存储Result值
        builder.CreateStore(paw_result, result_alloca);
        
        // 提取is_ok字段
        llvm::Value* is_ok_ptr = builder.CreateStructGEP(
            paw_return_type,
            result_alloca,  // 使用alloca指针
            0
        );
        llvm::Value* is_ok = builder.CreateLoad(
            builder.getInt1Ty(),
            is_ok_ptr,
            "is_ok"
        );
        
        // 创建条件分支
        llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "main.ok", c_main);
        llvm::BasicBlock* err_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "main.err", c_main);
        
        builder.CreateCondBr(is_ok, ok_bb, err_bb);
        
        // OK分支：提取value并返回
        builder.SetInsertPoint(ok_bb);
        llvm::Value* value_ptr = builder.CreateStructGEP(
            paw_return_type,
            result_alloca,  // 使用alloca指针
            1
        );
        llvm::Type* value_type = paw_return_type->getStructElementType(1);
        llvm::Value* value = builder.CreateLoad(value_type, value_ptr, "value");
        
        // 如果value是i32，直接返回；否则返回0
        if (value_type->isIntegerTy(32)) {
            builder.CreateRet(value);
        } else {
            builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
        }
        
        // ERR分支：返回1（错误码）
        builder.SetInsertPoint(err_bb);
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 1));
    } else {
        // 其他类型 -> return 0
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
    }
    
    // 恢复插入点
    if (saved_block) {
        builder.SetInsertPoint(saved_block);
    }
}

void StmtCodeGen::visit(ReturnStmt* node) {
    auto& builder = context_->getBuilder();
    
    if (node->getValue()) {
        llvm::Value* ret_val = expr_codegen_->generate(node->getValue());
        
        if (!ret_val) {
            result_ = nullptr;
            return;
        }
        
        // === 自动包装Optional和Result ===
        // 获取当前函数的返回类型
        llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
        llvm::Type* fn_ret_type = current_fn->getReturnType();
        
        // 检查是否需要包装为Optional或Result
        Type* ret_val_type = node->getValue()->getType();
        
        // 如果函数返回Optional<T>，且返回值是T（非Optional），自动包装
        if (fn_ret_type->isStructTy() && fn_ret_type->getStructNumElements() == 2) {
            // 可能是Optional<T> = { i1 has_value, T value }
            // 检查返回值类型是否匹配Optional的value类型
            llvm::Type* expected_value_type = fn_ret_type->getStructElementType(1);
            
            if (ret_val->getType() == expected_value_type) {
                // 自动包装为Optional: { true, value }
                llvm::Value* optional_value = llvm::UndefValue::get(fn_ret_type);
                
                // 设置has_value = true
                optional_value = builder.CreateInsertValue(optional_value,
                    llvm::ConstantInt::get(builder.getInt1Ty(), 1),  // true
                    0);
                
                // 设置value
                optional_value = builder.CreateInsertValue(optional_value, ret_val, 1);
                
                result_ = builder.CreateRet(optional_value);
                return;
            }
            
            // === 特殊处理：null返回（Optional<void> → Optional<T>） ===
            // 如果返回值是Optional类型（可能是null），且第一个字段类型匹配
            if (ret_val->getType()->isStructTy() &&
                ret_val->getType()->getStructNumElements() == 2 &&
                ret_val->getType()->getStructElementType(0)->isIntegerTy(1)) {
                
                // 这可能是null（Optional<void>），需要转换为Optional<T>
                // 提取has_value字段
                llvm::Value* has_value = builder.CreateExtractValue(ret_val, 0, "has_value");
                
                // 创建正确类型的Optional<T>
                llvm::Value* converted_optional = llvm::UndefValue::get(fn_ret_type);
                
                // 复制has_value（应该是false for null）
                converted_optional = builder.CreateInsertValue(converted_optional, has_value, 0);
                
                // value字段保持undef（因为has_value=false时不使用）
                
                result_ = builder.CreateRet(converted_optional);
                return;
            }
        }
        
        // 否则直接返回
        result_ = builder.CreateRet(ret_val);
    } else {
        result_ = builder.CreateRetVoid();
    }
}

void StmtCodeGen::visit(IfStmt* node) {
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    // 生成条件
    llvm::Value* cond = expr_codegen_->generate(node->getCondition());
    
    // 创建基本块
    llvm::BasicBlock* then_bb = context_->createBasicBlock("if.then", func);
    llvm::BasicBlock* else_bb = node->getElseStmt() ?
        context_->createBasicBlock("if.else", func) : nullptr;
    llvm::BasicBlock* merge_bb = context_->createBasicBlock("if.end", func);
    
    // 创建分支
    if (else_bb) {
        context_->getBuilder().CreateCondBr(cond, then_bb, else_bb);
    } else {
        context_->getBuilder().CreateCondBr(cond, then_bb, merge_bb);
    }
    
    // 生成then分支
    context_->getBuilder().SetInsertPoint(then_bb);
    node->getThenStmt()->accept(this);
    bool then_has_terminator = context_->getCurrentBlock()->getTerminator() != nullptr;
    if (!then_has_terminator) {
        context_->getBuilder().CreateBr(merge_bb);
    }
    
    // 生成else分支
    bool else_has_terminator = false;
    if (else_bb) {
        context_->getBuilder().SetInsertPoint(else_bb);
        node->getElseStmt()->accept(this);
        else_has_terminator = context_->getCurrentBlock()->getTerminator() != nullptr;
        if (!else_has_terminator) {
            context_->getBuilder().CreateBr(merge_bb);
        }
    }
    
    // 只有在merge块可达时才设置插入点
    // 如果两个分支都有terminator（都return），merge块不可达，需要添加unreachable
    if (then_has_terminator && else_has_terminator && else_bb) {
        // 两个分支都有terminator，merge块不可达
        context_->getBuilder().SetInsertPoint(merge_bb);
        context_->getBuilder().CreateUnreachable();
    } else {
        // merge块可达，正常设置为插入点
        context_->getBuilder().SetInsertPoint(merge_bb);
    }
    
    result_ = nullptr;
}

void StmtCodeGen::visit(LoopStmt* node) {
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    llvm::BasicBlock* loop_bb = context_->createBasicBlock("loop", func);
    llvm::BasicBlock* after_bb = context_->createBasicBlock("afterloop", func);
    
    // 保存break/continue目标
    llvm::BasicBlock* old_break = break_target_;
    llvm::BasicBlock* old_continue = continue_target_;
    break_target_ = after_bb;
    continue_target_ = loop_bb;
    
    // 跳转到loop
    context_->getBuilder().CreateBr(loop_bb);
    
    // 生成loop体
    context_->getBuilder().SetInsertPoint(loop_bb);
    node->getBody()->accept(this);
    
    // 如果没有terminator，继续循环
    if (!context_->getCurrentBlock()->getTerminator()) {
        context_->getBuilder().CreateBr(loop_bb);
    }
    
    // 恢复break/continue目标
    break_target_ = old_break;
    continue_target_ = old_continue;
    
    // 继续在after块
    context_->getBuilder().SetInsertPoint(after_bb);
    result_ = nullptr;
}

void StmtCodeGen::visit(WhileStmt* node) {
    // while等价于 loop { if !cond { break } ... }
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    llvm::BasicBlock* loop_bb = context_->createBasicBlock("while.loop", func);
    llvm::BasicBlock* body_bb = context_->createBasicBlock("while.body", func);
    llvm::BasicBlock* after_bb = context_->createBasicBlock("while.end", func);
    
    // 跳转到loop
    context_->getBuilder().CreateBr(loop_bb);
    
    // loop header: 检查条件
    context_->getBuilder().SetInsertPoint(loop_bb);
    llvm::Value* cond = expr_codegen_->generate(node->getCondition());
    context_->getBuilder().CreateCondBr(cond, body_bb, after_bb);
    
    // loop body
    context_->getBuilder().SetInsertPoint(body_bb);
    
    llvm::BasicBlock* old_break = break_target_;
    llvm::BasicBlock* old_continue = continue_target_;
    break_target_ = after_bb;
    continue_target_ = loop_bb;
    
    node->getBody()->accept(this);
    
    if (!context_->getCurrentBlock()->getTerminator()) {
        context_->getBuilder().CreateBr(loop_bb);
    }
    
    break_target_ = old_break;
    continue_target_ = old_continue;
    
    // 继续在after块
    context_->getBuilder().SetInsertPoint(after_bb);
    result_ = nullptr;
}

void StmtCodeGen::visit(BreakStmt* node) {
    if (break_target_) {
        result_ = context_->getBuilder().CreateBr(break_target_);
    }
}

void StmtCodeGen::visit(ContinueStmt* node) {
    if (continue_target_) {
        result_ = context_->getBuilder().CreateBr(continue_target_);
    }
}

void StmtCodeGen::visit(BlockStmt* node) {
    context_->enterScope();
    
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
        
        // 如果已经有terminator，停止生成后续代码
        if (context_->getCurrentBlock()->getTerminator()) {
            break;
        }
    }
    
    context_->exitScope();
    result_ = nullptr;
}

void StmtCodeGen::visit(ForStmt* node) {
    // for i in 0..10 { ... } 展开为传统循环
    
    auto& builder = context_->getBuilder();
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // 生成iterator表达式（应该是Range）
    llvm::Value* iterator = expr_codegen_->generate(node->getIterator());
    if (!iterator) {
        result_ = nullptr;
        return;
    }
    
    // 如果iterator是Range结构体 {start, end, inclusive}
    llvm::Type* iter_type = iterator->getType();
    
    if (iter_type->isStructTy()) {
        llvm::StructType* range_struct = llvm::cast<llvm::StructType>(iter_type);
        
        // 提取start, end, inclusive
        llvm::Value* start = builder.CreateExtractValue(iterator, 0, "range.start");
        llvm::Value* end = builder.CreateExtractValue(iterator, 1, "range.end");
        llvm::Value* inclusive = builder.CreateExtractValue(iterator, 2, "range.inclusive");
        
        // 创建循环变量
        llvm::AllocaInst* loop_var = context_->createEntryBlockAlloca(
            func,
            node->getVarName(),
            start->getType()
        );
        builder.CreateStore(start, loop_var);
        context_->defineVariable(node->getVarName(), loop_var);
        
        // 创建基本块
        llvm::BasicBlock* cond_bb = context_->createBasicBlock("for.cond", func);
        llvm::BasicBlock* body_bb = context_->createBasicBlock("for.body", func);
        llvm::BasicBlock* inc_bb = context_->createBasicBlock("for.inc", func);
        llvm::BasicBlock* end_bb = context_->createBasicBlock("for.end", func);
        
        // 跳转到条件检查
        builder.CreateBr(cond_bb);
        
        // 条件检查：i < end 或 i <= end
        builder.SetInsertPoint(cond_bb);
        llvm::Value* current = builder.CreateLoad(start->getType(), loop_var, "i");
        llvm::Value* cond;
        
        // 根据inclusive标志选择比较运算符
        llvm::BasicBlock* lt_bb = context_->createBasicBlock("for.lt", func);
        llvm::BasicBlock* le_bb = context_->createBasicBlock("for.le", func);
        llvm::BasicBlock* cmp_bb = context_->createBasicBlock("for.cmp", func);
        
        builder.CreateCondBr(inclusive, le_bb, lt_bb);
        
        // i < end
        builder.SetInsertPoint(lt_bb);
        llvm::Value* cond_lt = builder.CreateICmpSLT(current, end, "cond.lt");
        builder.CreateBr(cmp_bb);
        
        // i <= end
        builder.SetInsertPoint(le_bb);
        llvm::Value* cond_le = builder.CreateICmpSLE(current, end, "cond.le");
        builder.CreateBr(cmp_bb);
        
        // PHI节点选择条件
        builder.SetInsertPoint(cmp_bb);
        llvm::PHINode* phi = builder.CreatePHI(builder.getInt1Ty(), 2, "cond");
        phi->addIncoming(cond_lt, lt_bb);
        phi->addIncoming(cond_le, le_bb);
        
        builder.CreateCondBr(phi, body_bb, end_bb);
        
        // 循环体
        builder.SetInsertPoint(body_bb);
        
        llvm::BasicBlock* old_break = break_target_;
        llvm::BasicBlock* old_continue = continue_target_;
        break_target_ = end_bb;
        continue_target_ = inc_bb;
        
        node->getBody()->accept(this);
        
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(inc_bb);
        }
        
        break_target_ = old_break;
        continue_target_ = old_continue;
        
        // 递增：i = i + 1
        builder.SetInsertPoint(inc_bb);
        llvm::Value* current_inc = builder.CreateLoad(start->getType(), loop_var, "i.inc");
        llvm::Value* next = builder.CreateAdd(current_inc, 
                                               llvm::ConstantInt::get(start->getType(), 1), 
                                               "i.next");
        builder.CreateStore(next, loop_var);
        builder.CreateBr(cond_bb);
        
        // 继续在end块
        builder.SetInsertPoint(end_bb);
    }
    
    result_ = nullptr;
}

void StmtCodeGen::visit(StructDecl* node) {
    // 类型定义不需要生成LLVM IR
    // TypeSystem已在TypeChecker阶段注册类型
    result_ = nullptr;
}

void StmtCodeGen::visit(EnumDecl* node) {
    // 类型定义不需要生成LLVM IR
    // TypeSystem已在TypeChecker阶段注册类型
    result_ = nullptr;
}

void StmtCodeGen::visit(InterfaceDecl* node) {
    // 接口定义不需要生成LLVM IR
    // TypeSystem已在TypeChecker阶段注册类型
    result_ = nullptr;
}

void StmtCodeGen::visit(SupportDecl* node) {
    // support块：生成接口方法实现
    // 方法名使用 TypeName.MethodName 格式
    
    for (const auto& method : node->getMethods()) {
        // 保存原始函数名
        std::string original_name = method->getName();
        
        // 生成带类型前缀的方法名
        std::string mangled_name = node->getTypeName() + "." + original_name;
        
        // 临时修改函数名（用于CodeGen）
        // 注意：这是一个hack，理想情况应该在FunctionDecl中支持重命名
        
        // 生成方法（作为普通函数）
        method->accept(this);
        
        // 在method_table_中注册
        llvm::Function* method_fn = context_->lookupFunction(original_name);
        if (method_fn) {
            // 重命名函数（包含类型名，避免冲突）
            method_fn->setName(mangled_name);
            
            // 注意：方法已通过重命名机制正确生成
            // 接口调用通过函数名查找实现静态分发
            // 动态分发（vtable）可作为未来优化
        }
    }
    
    result_ = nullptr;
}

} // namespace pawc
