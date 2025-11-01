//===--- expr_codegen.cpp - Expression CodeGen Implementation ----*- C++ -*-===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "../stmt/stmt_codegen.h"
#include "frontend/parser/ast/stmt.h"
#include "frontend/parser/ast/pattern.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>

namespace pawc {

ExprCodeGen::ExprCodeGen(CodeGenContext* context)
    : CodeGenBase(context), result_(nullptr) {}

llvm::Value* ExprCodeGen::generate(ASTNode* node) {
    if (auto* expr = dynamic_cast<Expr*>(node)) {
        expr->accept(this);
        return result_;
    }
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 字面量生成 - 已移至 literal_codegen.cpp
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 变量和标识符
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(SelfExpr* node) {
    // self表达式：返回self参数（函数的第一个参数）
    llvm::Function* current_fn = context_->getBuilder().GetInsertBlock()->getParent();
    
    if (current_fn && current_fn->arg_size() > 0) {
        // 假设self是第一个参数
        result_ = current_fn->arg_begin();
    } else {
        result_ = nullptr;
    }
}

void ExprCodeGen::visit(IdentifierExpr* node) {
    llvm::Value* var = context_->lookupVariable(node->getName());
    if (var) {
        // === 特殊处理：FunctionType（闭包）===
        // 闭包变量存储的是函数指针，直接返回alloca（指针），不load
        // CallExpr会负责正确处理
        if (node->getType() && node->getType()->getKind() == Type::Kind::Function) {
            result_ = var;  // 返回指向函数指针的指针（alloca）
            return;
        }
        
        // === 普通类型：Load变量值 ===
        TypeCodeGen type_gen(context_->getLLVMContext());
        llvm::Type* load_type = node->getType() ?
            type_gen.mapType(node->getType()) :
            context_->getI32Type();
        
        result_ = context_->getBuilder().CreateLoad(load_type, var, node->getName());
    } else {
        result_ = nullptr;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 二元运算和一元运算 - 已移至 binary_codegen.cpp
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 函数调用（已移至call_codegen.cpp）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// CallExpr::visit实现在call_codegen.cpp中

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 复杂表达式
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(StaticAccessExpr* node) {
    // 静态访问: Type::Variant（枚举构造器）
    // 注意：当前PawLang中枚举主要用于match表达式
    // 枚举构造器语法（如Status::Active）暂未在examples中使用
    // 该功能已在Sema中处理，CodeGen层面暂不需要生成代码
    // 如需要，应该：
    //   1. 查找枚举类型和variant索引
    //   2. 生成对应的常量值（i32）
    //   3. 或生成带数据的枚举值（struct）
    result_ = nullptr;
}

void ExprCodeGen::visit(MemberExpr* node) {
    // 结构体成员访问: obj.field
    node->getObject()->accept(this);
    llvm::Value* object = result_;
    
    if (!object) {
        result_ = nullptr;
        return;
    }
    
    auto& builder = context_->getBuilder();
    TypeCodeGen type_gen(context_->getLLVMContext());
    
    // 获取结构体类型
    Type* obj_type = node->getObject()->getType();
    if (!obj_type || !obj_type->isStruct()) {
        result_ = nullptr;
        return;
    }
    
    auto* struct_type = static_cast<StructType*>(obj_type);
    llvm::Type* llvm_struct_type = type_gen.mapType(struct_type);
    
    // 查找字段索引
    const auto& fields = struct_type->getFields();
    int field_idx = -1;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].first == node->getMember()) {
            field_idx = static_cast<int>(i);
            break;
        }
    }
    
    if (field_idx < 0) {
        result_ = nullptr;
        return;
    }
    
    // 如果object是值而不是指针，需要先存储到栈上
    llvm::Value* object_ptr = object;
    if (!object->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(object->getType(), nullptr, "struct.tmp");
        builder.CreateStore(object, temp);
        object_ptr = temp;
    }
    
    // 使用CreateStructGEP访问字段
    llvm::Value* field_ptr = builder.CreateStructGEP(
        llvm_struct_type,
        object_ptr,
        field_idx,
        node->getMember()
    );
    
    // Load字段值
    llvm::Type* field_type = type_gen.mapType(fields[field_idx].second);
    result_ = builder.CreateLoad(field_type, field_ptr);
}

void ExprCodeGen::visit(MatchExpr* node) {
    // match表达式CodeGen - 完整实现
    
    auto& context = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    
    // 1. 计算被匹配的值
    node->getScrutinee()->accept(this);
    llvm::Value* scrutinee_value = result_;
    
    if (!scrutinee_value) {
        result_ = nullptr;
        return;
    }
    
    // 2. 获取match表达式的结果类型
    Type* result_type = node->getType();
    if (!result_type) {
        result_ = nullptr;
        return;
    }
    
    TypeCodeGen type_gen(context);
    llvm::Type* llvm_result_type = type_gen.mapType(result_type);
    
    // 3. 创建结果变量（用于存储每个分支的结果）
    llvm::AllocaInst* result_alloca = builder.CreateAlloca(
        llvm_result_type, nullptr, "match.result");
    
    // 4. 创建基本块
    llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(context, "match.end", current_fn);
    
    // 5. 为每个分支生成代码
    const auto& arms = node->getArms();
    
    for (size_t i = 0; i < arms.size(); ++i) {
        const auto& arm = arms[i];
        
        // 创建分支的基本块
        llvm::BasicBlock* arm_bb = llvm::BasicBlock::Create(
            context, "match.arm." + std::to_string(i), current_fn);
        llvm::BasicBlock* next_bb = (i < arms.size() - 1) 
            ? llvm::BasicBlock::Create(context, "match.next." + std::to_string(i), current_fn)
            : end_bb;
        
        // 生成模式匹配条件
        llvm::Value* match_cond = generatePatternMatch(
            arm.pattern.get(), 
            scrutinee_value, 
            node->getScrutinee()->getType()
        );
        
        if (!match_cond) {
            // 通配符或变量绑定总是匹配
            builder.CreateBr(arm_bb);
        } else {
            builder.CreateCondBr(match_cond, arm_bb, next_bb);
        }
        
        // 生成分支表达式的代码
        builder.SetInsertPoint(arm_bb);
        
        // 处理模式变量绑定
        bindPatternVariables(arm.pattern.get(), scrutinee_value, node->getScrutinee()->getType());
        
        // 计算分支表达式
        arm.expression->accept(this);
        llvm::Value* arm_value = result_;
        
        if (arm_value) {
            builder.CreateStore(arm_value, result_alloca);
        }
        
        builder.CreateBr(end_bb);
        
        // 移动到下一个分支
        if (i < arms.size() - 1) {
            builder.SetInsertPoint(next_bb);
        }
    }
    
    // 6. 设置到end块并加载结果
    builder.SetInsertPoint(end_bb);
    result_ = builder.CreateLoad(llvm_result_type, result_alloca, "match.value");
}

// 辅助方法：生成模式匹配条件
llvm::Value* ExprCodeGen::generatePatternMatch(Pattern* pattern, llvm::Value* scrutinee, Type* scrutinee_type) {
    auto& context = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    
    // LiteralPattern: 字面量匹配
    if (auto* lit = dynamic_cast<LiteralPattern*>(pattern)) {
        switch (lit->getKind()) {
            case LiteralPattern::Kind::Int: {
                // 整数比较
                llvm::Value* pattern_val = llvm::ConstantInt::get(
                    scrutinee->getType(),
                    std::stoll(lit->getValue())
                );
                return builder.CreateICmpEQ(scrutinee, pattern_val, "match.int.eq");
            }
            case LiteralPattern::Kind::Bool: {
                // 布尔比较
                llvm::Value* pattern_val = llvm::ConstantInt::get(
                    llvm::Type::getInt1Ty(context),
                    lit->getValue() == "true" ? 1 : 0
                );
                return builder.CreateICmpEQ(scrutinee, pattern_val, "match.bool.eq");
            }
            case LiteralPattern::Kind::String: {
                // 字符串比较（调用runtime strcmp）
                llvm::Function* strcmp_fn = context_->getRuntimeFunction("paw_strcmp");
                if (!strcmp_fn) {
                    return nullptr;
                }
                
                // 创建字符串字面量
                llvm::Value* pattern_str = builder.CreateGlobalStringPtr(lit->getValue(), "match.str.literal");
                
                // 调用strcmp
                llvm::Value* cmp_result = builder.CreateCall(
                    strcmp_fn,
                    {scrutinee, pattern_str},
                    "strcmp.result"
                );
                
                // strcmp返回0表示相等
                llvm::Value* zero = llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(context),
                    0
                );
                return builder.CreateICmpEQ(cmp_result, zero, "match.str.eq");
            }
            case LiteralPattern::Kind::Char: {
                // 字符比较
                llvm::Value* pattern_val = llvm::ConstantInt::get(
                    llvm::Type::getInt8Ty(context),
                    lit->getValue()[0]
                );
                return builder.CreateICmpEQ(scrutinee, pattern_val, "match.char.eq");
            }
            case LiteralPattern::Kind::Float: {
                // 浮点数比较
                llvm::APFloat ap_float(std::stod(lit->getValue()));
                llvm::Value* pattern_val = llvm::ConstantFP::get(context, ap_float);
                return builder.CreateFCmpOEQ(scrutinee, pattern_val, "match.float.eq");
            }
            default:
                return nullptr;
        }
    }
    
    // WildcardPattern: 总是匹配
    if (dynamic_cast<WildcardPattern*>(pattern)) {
        return nullptr;  // nullptr表示总是匹配
    }
    
    // VariablePattern: 总是匹配（绑定变量）
    if (dynamic_cast<VariablePattern*>(pattern)) {
        return nullptr;  // nullptr表示总是匹配
    }
    
    // TuplePattern: 元组匹配
    if (auto* tuple = dynamic_cast<TuplePattern*>(pattern)) {
        if (!scrutinee_type || scrutinee_type->getKind() != Type::Kind::Tuple) {
            return nullptr;
        }
        
        auto* tuple_type = static_cast<TupleType*>(scrutinee_type);
        const auto& element_types = tuple_type->getElementTypes();
        const auto& patterns = tuple->getElements();
        
        if (patterns.size() != element_types.size()) {
            return nullptr;  // 元组大小不匹配
        }
        
        TypeCodeGen type_gen(context);
        llvm::Type* llvm_tuple_type = type_gen.mapType(tuple_type);
        
        // 确保scrutinee是指针类型
        llvm::Value* tuple_ptr = scrutinee;
        if (!scrutinee->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "tuple.tmp");
            builder.CreateStore(scrutinee, temp);
            tuple_ptr = temp;
        }
        
        // 生成所有子模式的条件，并用AND连接
        llvm::Value* result_cond = nullptr;
        
        for (size_t i = 0; i < patterns.size(); ++i) {
            // 提取元组元素
            llvm::Value* elem_ptr = builder.CreateStructGEP(
                llvm_tuple_type,
                tuple_ptr,
                i,
                "tuple.elem." + std::to_string(i)
            );
            
            llvm::Type* elem_llvm_type = type_gen.mapType(element_types[i]);
            llvm::Value* elem_value = builder.CreateLoad(
                elem_llvm_type,
                elem_ptr,
                "tuple.elem.val"
            );
            
            // 递归生成子模式匹配条件
            llvm::Value* sub_cond = generatePatternMatch(
                patterns[i].get(),
                elem_value,
                element_types[i]
            );
            
            if (sub_cond) {
                if (!result_cond) {
                    result_cond = sub_cond;
                } else {
                    result_cond = builder.CreateAnd(result_cond, sub_cond, "tuple.match.and");
                }
            }
        }
        
        return result_cond;  // nullptr表示所有子模式都是通配符/变量
    }
    
    // EnumPattern: 枚举匹配
    if (auto* enum_pat = dynamic_cast<EnumPattern*>(pattern)) {
        // 枚举匹配：检查tag是否相等
        if (!scrutinee_type || scrutinee_type->getKind() != Type::Kind::Enum) {
            return nullptr;
        }
        
        auto* enum_type = static_cast<EnumType*>(scrutinee_type);
        const auto& variants = enum_type->getVariants();
        
        // 查找变体索引
        int variant_idx = -1;
        for (size_t i = 0; i < variants.size(); ++i) {
            if (variants[i].first == enum_pat->getVariantName()) {
                variant_idx = static_cast<int>(i);
                break;
            }
        }
        
        if (variant_idx < 0) {
            return nullptr;  // 变体不存在
        }
        
        // 提取enum的tag字段（第0个字段）
        TypeCodeGen type_gen(context);
        llvm::Type* llvm_enum_type = type_gen.mapType(enum_type);
        
        llvm::Value* enum_ptr = scrutinee;
        if (!scrutinee->getType()->isPointerTy()) {
            // 如果是值类型，需要先分配到栈上
            llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "enum.tmp");
            builder.CreateStore(scrutinee, temp);
            enum_ptr = temp;
        }
        
        // 提取tag字段
        llvm::Value* tag_ptr = builder.CreateStructGEP(
            llvm_enum_type,
            enum_ptr,
            0,
            "enum.tag.ptr"
        );
        llvm::Value* tag_value = builder.CreateLoad(
            llvm::Type::getInt32Ty(context),
            tag_ptr,
            "enum.tag"
        );
        
        // 比较tag
        llvm::Value* expected_tag = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            variant_idx
        );
        
        return builder.CreateICmpEQ(tag_value, expected_tag, "match.enum.eq");
    }
    
    return nullptr;
}

// 辅助方法：绑定模式变量
void ExprCodeGen::bindPatternVariables(Pattern* pattern, llvm::Value* value, Type* value_type) {
    auto& builder = context_->getBuilder();
    
    // VariablePattern: 创建变量绑定
    if (auto* var = dynamic_cast<VariablePattern*>(pattern)) {
        // 在当前作用域创建变量
        TypeCodeGen type_gen(context_->getLLVMContext());
        llvm::Type* var_type = type_gen.mapType(value_type);
        
        // 创建alloca并存储值
        llvm::AllocaInst* var_alloca = builder.CreateAlloca(
            var_type, nullptr, var->getName());
        builder.CreateStore(value, var_alloca);
        
        // 注意：变量已通过alloca正确生成并存储
        // CodeGen层面的变量查找通过函数作用域管理
        // 符号表主要用于Sema阶段，CodeGen不需要额外注册
    }
    
    // WildcardPattern: 不绑定任何变量
    if (dynamic_cast<WildcardPattern*>(pattern)) {
        return;
    }
    
    // LiteralPattern: 不绑定任何变量
    if (dynamic_cast<LiteralPattern*>(pattern)) {
        return;
    }
    
    // TuplePattern: 递归绑定元组元素
    if (auto* tuple = dynamic_cast<TuplePattern*>(pattern)) {
        if (!value_type || value_type->getKind() != Type::Kind::Tuple) {
            return;
        }
        
        auto* tuple_type = static_cast<TupleType*>(value_type);
        const auto& element_types = tuple_type->getElementTypes();
        const auto& patterns = tuple->getElements();
        
        TypeCodeGen type_gen(context_->getLLVMContext());
        llvm::Type* llvm_tuple_type = type_gen.mapType(tuple_type);
        
        // 确保value是指针类型
        llvm::Value* tuple_ptr = value;
        if (!value->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "tuple.tmp");
            builder.CreateStore(value, temp);
            tuple_ptr = temp;
        }
        
        // 递归绑定每个元素
        for (size_t i = 0; i < patterns.size() && i < element_types.size(); ++i) {
            // 提取元组元素
            llvm::Value* elem_ptr = builder.CreateStructGEP(
                llvm_tuple_type,
                tuple_ptr,
                i,
                "tuple.elem." + std::to_string(i)
            );
            
            llvm::Type* elem_llvm_type = type_gen.mapType(element_types[i]);
            llvm::Value* elem_value = builder.CreateLoad(
                elem_llvm_type,
                elem_ptr,
                "tuple.elem.val"
            );
            
            // 递归绑定
            bindPatternVariables(patterns[i].get(), elem_value, element_types[i]);
        }
    }
    
    // EnumPattern: 绑定枚举数据
    if (auto* enum_pat = dynamic_cast<EnumPattern*>(pattern)) {
        if (!value_type || value_type->getKind() != Type::Kind::Enum) {
            return;
        }
        
        auto* enum_type = static_cast<EnumType*>(value_type);
        const auto& variants = enum_type->getVariants();
        
        // 查找变体
        int variant_idx = -1;
        Type* data_type = nullptr;
        for (size_t i = 0; i < variants.size(); ++i) {
            if (variants[i].first == enum_pat->getVariantName()) {
                variant_idx = static_cast<int>(i);
                data_type = variants[i].second;
                break;
            }
        }
        
        if (variant_idx < 0 || !data_type) {
            return;  // 无数据的变体，或变体不存在
        }
        
        // 提取enum的数据字段（第1个字段）
        TypeCodeGen type_gen(context_->getLLVMContext());
        llvm::Type* llvm_enum_type = type_gen.mapType(enum_type);
        
        llvm::Value* enum_ptr = value;
        if (!value->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "enum.tmp");
            builder.CreateStore(value, temp);
            enum_ptr = temp;
        }
        
        // 提取data字段
        llvm::Value* data_ptr = builder.CreateStructGEP(
            llvm_enum_type,
            enum_ptr,
            1,
            "enum.data.ptr"
        );
        
        llvm::Type* data_llvm_type = type_gen.mapType(data_type);
        llvm::Value* data_value = builder.CreateLoad(
            data_llvm_type,
            data_ptr,
            "enum.data"
        );
        
        // 绑定内部模式
        if (enum_pat->getInner()) {
            bindPatternVariables(enum_pat->getInner(), data_value, data_type);
        }
    }
}

void ExprCodeGen::visit(IndexExpr* node) {
    // 数组/切片索引: arr[i]
    node->getObject()->accept(this);
    llvm::Value* array = result_;
    
    node->getIndex()->accept(this);
    llvm::Value* index = result_;
    
    if (!array || !index) {
        result_ = nullptr;
        return;
    }
    
    auto& builder = context_->getBuilder();
    llvm::Value* array_ptr = array;
    llvm::Type* array_type = array->getType();
    
    // 如果array是值而不是指针，需要先存储到栈上
    if (!array_type->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(array_type, nullptr, "array.tmp");
        builder.CreateStore(array, temp);
        array_ptr = temp;
        // array_type保持不变，用于GEP
    } else {
        // 如果是指针，获取指向的类型
        // 对于不透明指针，array_type已经是正确的
    }
    
    // 生成GEP
    llvm::Value* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    
    llvm::Value* elem_ptr = builder.CreateGEP(
        array_type,
        array_ptr,
        {zero, index},
        "arrayidx"
    );
    
    // Load元素值
    Type* elem_type = node->getType();
    if (elem_type) {
        TypeCodeGen type_gen(context_->getLLVMContext());
        llvm::Type* llvm_elem_type = type_gen.mapType(elem_type);
        result_ = builder.CreateLoad(llvm_elem_type, elem_ptr);
    } else {
        result_ = nullptr;
    }
}

void ExprCodeGen::visit(IfExpr* node) {
    // if表达式: if cond { then_expr } else { else_expr }
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    // 生成条件
    node->getCondition()->accept(this);
    llvm::Value* cond = result_;
    
    // 创建基本块
    llvm::BasicBlock* then_bb = context_->createBasicBlock("if.then", func);
    llvm::BasicBlock* else_bb = context_->createBasicBlock("if.else", func);
    llvm::BasicBlock* merge_bb = context_->createBasicBlock("if.end", func);
    
    // 创建分支
    context_->getBuilder().CreateCondBr(cond, then_bb, else_bb);
    
    // 生成then分支
    context_->getBuilder().SetInsertPoint(then_bb);
    node->getThenExpr()->accept(this);
    llvm::Value* then_val = result_;
    llvm::BasicBlock* then_end_bb = context_->getCurrentBlock();
    if (!then_end_bb->getTerminator()) {
        context_->getBuilder().CreateBr(merge_bb);
    }
    
    // 生成else分支
    context_->getBuilder().SetInsertPoint(else_bb);
    llvm::Value* else_val = nullptr;
    if (node->getElseExpr()) {
        node->getElseExpr()->accept(this);
        else_val = result_;
    }
    llvm::BasicBlock* else_end_bb = context_->getCurrentBlock();
    if (!else_end_bb->getTerminator()) {
        context_->getBuilder().CreateBr(merge_bb);
    }
    
    // 合并块 - 使用PHI节点
    context_->getBuilder().SetInsertPoint(merge_bb);
    
    if (then_val && else_val && then_val->getType() == else_val->getType()) {
        llvm::PHINode* phi = context_->getBuilder().CreatePHI(
            then_val->getType(), 2, "iftmp");
        phi->addIncoming(then_val, then_end_bb);
        phi->addIncoming(else_val, else_end_bb);
        result_ = phi;
    } else {
        result_ = nullptr;
    }
}

void ExprCodeGen::visit(BlockExpr* node) {
    // 块表达式: { stmt1; stmt2; expr }
    context_->enterScope();
    
    const auto& stmts = node->getStmts();
    llvm::Value* last_value = nullptr;
    
    if (stmt_codegen_ && !stmts.empty()) {
        // 生成所有语句
        for (size_t i = 0; i < stmts.size(); ++i) {
            auto* stmt = stmts[i].get();
            
            // 最后一个语句如果是ExprStmt，提取其值作为块返回值
            if (i == stmts.size() - 1) {
                if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
                    // 最后一个是表达式语句，生成其值
                    if (expr_stmt->getExpr()) {
                        last_value = generate(expr_stmt->getExpr());
                    }
                    continue;
                }
            }
            
            // 其他语句正常生成
            stmt_codegen_->generate(stmt);
        }
    }
    
    context_->exitScope();
    result_ = last_value;
}

void ExprCodeGen::visit(ArrayLiteral* node) {
    // 数组字面量完整代码生成 [1, 2, 3]
    auto& builder = context_->getBuilder();
    TypeCodeGen type_gen(context_->getLLVMContext());
    
    Type* array_type = node->getType();
    if (!array_type) {
        result_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_array_type = type_gen.mapType(array_type);
    if (!llvm_array_type) {
        result_ = nullptr;
        return;
    }
    
    // 空数组
    if (node->getElements().empty()) {
        result_ = llvm::ConstantAggregateZero::get(llvm_array_type);
        return;
    }
    
    // 在栈上分配数组
    llvm::AllocaInst* array_alloca = builder.CreateAlloca(llvm_array_type, nullptr, "array.tmp");
    
    // 初始化每个元素
    size_t idx = 0;
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
        if (!result_) {
            result_ = nullptr;
            return;
        }
        
        // GEP计算元素指针
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0),
            llvm::ConstantInt::get(builder.getInt32Ty(), idx++)
        };
        llvm::Value* elem_ptr = builder.CreateGEP(llvm_array_type, array_alloca, indices, "array.elem.ptr");
        builder.CreateStore(result_, elem_ptr);
    }
    
    // 加载整个数组
    result_ = builder.CreateLoad(llvm_array_type, array_alloca, "array");
}

void ExprCodeGen::visit(TupleExpr* node) {
    // 元组完整代码生成 (1, "hello", true)
    auto& builder = context_->getBuilder();
    TypeCodeGen type_gen(context_->getLLVMContext());
    
    Type* tuple_type = node->getType();
    if (!tuple_type) {
        result_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_tuple_type = type_gen.mapType(tuple_type);
    if (!llvm_tuple_type || !llvm_tuple_type->isStructTy()) {
        result_ = nullptr;
        return;
    }
    
    llvm::StructType* struct_type = llvm::cast<llvm::StructType>(llvm_tuple_type);
    
    // 空元组
    if (node->getElements().empty()) {
        result_ = llvm::UndefValue::get(struct_type);
        return;
    }
    
    // 在栈上分配元组
    llvm::AllocaInst* tuple_alloca = builder.CreateAlloca(struct_type, nullptr, "tuple.tmp");
    
    // 初始化每个字段
    for (size_t i = 0; i < node->getElements().size(); ++i) {
        node->getElements()[i]->accept(this);
        if (!result_) {
            result_ = nullptr;
            return;
        }
        
        llvm::Value* field_ptr = builder.CreateStructGEP(struct_type, tuple_alloca, i, "tuple.field.ptr");
        builder.CreateStore(result_, field_ptr);
    }
    
    // 加载整个元组
    result_ = builder.CreateLoad(struct_type, tuple_alloca, "tuple");
}

void ExprCodeGen::visit(RangeExpr* node) {
    // Range表达式完整代码生成 0..10
    auto& builder = context_->getBuilder();
    
    llvm::Value* start_val = nullptr;
    llvm::Value* end_val = nullptr;
    
    // 生成start值
    if (node->getStart()) {
        node->getStart()->accept(this);
        start_val = result_;
    } else {
        start_val = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // 生成end值
    if (node->getEnd()) {
        node->getEnd()->accept(this);
        end_val = result_;
    } else {
        end_val = llvm::ConstantInt::get(builder.getInt32Ty(), INT32_MAX);
    }
    
    if (!start_val || !end_val) {
        result_ = nullptr;
        return;
    }
    
    // 创建Range结构体 {start, end, inclusive}
    llvm::Type* i32_type = builder.getInt32Ty();
    llvm::Type* i1_type = builder.getInt1Ty();
    llvm::StructType* range_struct = llvm::StructType::get(
        builder.getContext(),
        {i32_type, i32_type, i1_type},
        false
    );
    
    llvm::AllocaInst* range_alloca = builder.CreateAlloca(range_struct, nullptr, "range.tmp");
    
    // 初始化start
    llvm::Value* start_ptr = builder.CreateStructGEP(range_struct, range_alloca, 0, "range.start.ptr");
    builder.CreateStore(start_val, start_ptr);
    
    // 初始化end
    llvm::Value* end_ptr = builder.CreateStructGEP(range_struct, range_alloca, 1, "range.end.ptr");
    builder.CreateStore(end_val, end_ptr);
    
    // 初始化inclusive标志
    llvm::Value* inclusive_ptr = builder.CreateStructGEP(range_struct, range_alloca, 2, "range.inclusive.ptr");
    llvm::Value* inclusive_val = llvm::ConstantInt::get(i1_type, node->isInclusive() ? 1 : 0);
    builder.CreateStore(inclusive_val, inclusive_ptr);
    
    // 加载Range结构
    result_ = builder.CreateLoad(range_struct, range_alloca, "range");
}

void ExprCodeGen::visit(StructLiteral* node) {
    // 结构体字面量完整代码生成 Point { x: 10, y: 20 }
    auto& builder = context_->getBuilder();
    TypeCodeGen type_gen(context_->getLLVMContext());
    
    Type* struct_type = node->getType();
    if (!struct_type || struct_type->getKind() != Type::Kind::Struct) {
        result_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_struct_type = type_gen.mapType(struct_type);
    if (!llvm_struct_type || !llvm_struct_type->isStructTy()) {
        result_ = nullptr;
        return;
    }
    
    llvm::StructType* llvm_st = llvm::cast<llvm::StructType>(llvm_struct_type);
    StructType* paw_st = static_cast<StructType*>(struct_type);
    const auto& struct_fields = paw_st->getFields();
    
    // 在栈上分配结构体
    llvm::AllocaInst* struct_alloca = builder.CreateAlloca(llvm_st, nullptr, "struct.tmp");
    
    // 为每个字段生成初始化代码
    for (auto& field_init : node->getFields()) {
        // 找到字段在结构体中的索引
        size_t field_index = 0;
        bool found = false;
        
        for (size_t i = 0; i < struct_fields.size(); ++i) {
            if (struct_fields[i].first == field_init.name) {
                field_index = i;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // TypeChecker应该已经捕获这个错误
            continue;
        }
        
        // 生成字段值
        field_init.value->accept(this);
        if (!result_) {
            result_ = nullptr;
            return;
        }
        
        // 获取字段指针并存储
        llvm::Value* field_ptr = builder.CreateStructGEP(
            llvm_st, 
            struct_alloca, 
            field_index, 
            "struct.field." + field_init.name
        );
        builder.CreateStore(result_, field_ptr);
    }
    
    // 加载整个结构体
    result_ = builder.CreateLoad(llvm_st, struct_alloca, "struct");
}

} // namespace pawc
