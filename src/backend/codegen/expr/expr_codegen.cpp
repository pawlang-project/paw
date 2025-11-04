//===--- expr_codegen.cpp - Expression CodeGen Implementation ----*- C++ -*-===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "../stmt/stmt_codegen.h"
#include "frontend/parser/ast/stmt.h"
#include "frontend/parser/ast/pattern.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <iostream>

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
    // self表达式：在函数参数中查找self
    llvm::Value* self_var = context_->lookupVariable("self");
    if (self_var) {
        // self_var是alloca，存储的是指针类型（因为是引用参数）
        // Load它得到指针值（%Point*）
        result_ = context_->getBuilder().CreateLoad(
            context_->getVoidType()->getPointerTo(),
            self_var, 
            "self"
        );
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
        llvm::Type* load_type = node->getType() ?
            context_->getLLVMType(node->getType()) :
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
    // 🔧 M7: 静态访问enum variant（如Option::None）
    auto& builder = context_->getBuilder();
    Type* type = node->getType();
    
    // 检查是否是enum类型（无参数variant，如Option::None）
    if (type && type->isEnum()) {
        EnumType* enum_type = static_cast<EnumType*>(type);
        
        // 查找variant索引
        std::string variant_name = node->getMember();
        int variant_index = -1;
        
        const auto& variants = enum_type->getVariants();
        for (size_t i = 0; i < variants.size(); i++) {
            if (variants[i].first == variant_name) {
                variant_index = static_cast<int>(i);
                break;
            }
        }
        
        if (variant_index < 0) {
            result_ = nullptr;
            return;
        }
        
        // 获取enum的LLVM类型
        llvm::Type* enum_llvm_type = context_->getLLVMType(enum_type);
        
        // 创建enum struct: {i32 variant_index, data}
        llvm::Value* enum_value = llvm::UndefValue::get(enum_llvm_type);
        
        // 设置variant索引
        enum_value = builder.CreateInsertValue(
            enum_value,
            builder.getInt32(variant_index),
            {0}
        );
        
        // None variant没有数据，data字段保持undef
        
        result_ = enum_value;
        return;
    }
    
    // 其他静态访问（函数类型等）
    result_ = nullptr;
}

void ExprCodeGen::visit(MemberExpr* node) {
    // 成员访问: obj.field 或 tuple.0
    node->getObject()->accept(this);
    llvm::Value* object = result_;
    
    if (!object) {
        result_ = nullptr;
        return;
    }
    
    auto& builder = context_->getBuilder();
    Type* obj_type = node->getObject()->getType();
    if (!obj_type) {
        result_ = nullptr;
        return;
    }
    
    // 🔧 引用类型处理：如果对象是引用类型，获取pointee类型
    if (obj_type->isReference()) {
        auto* ref_type = static_cast<ReferenceType*>(obj_type);
        obj_type = ref_type->getPointeeType();
    }
    
    // 检查是否是元组字段访问（成员名是数字）
    const std::string& member = node->getMember();
    bool is_tuple_access = !member.empty() && std::isdigit(member[0]);
    
    if (is_tuple_access && obj_type->isTuple()) {
        // 元组字段访问: tuple.0, tuple.1 等
        auto* tuple_type = static_cast<TupleType*>(obj_type);
        
        // 解析字段索引
        int field_idx = std::stoi(member);
        
        // 验证索引有效性
        if (field_idx < 0 || field_idx >= static_cast<int>(tuple_type->getElementTypes().size())) {
            result_ = nullptr;
            return;
        }
        
        // 使用CreateExtractValue提取元组元素
        result_ = builder.CreateExtractValue(object, field_idx, "tuple.field." + member);
        
    } else if (obj_type->isStruct()) {
        // 结构体成员访问: struct.field_name
        auto* struct_type = static_cast<StructType*>(obj_type);
        llvm::Type* llvm_struct_type = context_->getLLVMType(struct_type);
        
        // 查找字段索引
        const auto& fields = struct_type->getFields();
        int field_idx = -1;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].first == member) {
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
            member
        );
        
        // Load字段值
        llvm::Type* field_type = context_->getLLVMType(fields[field_idx].second);
        result_ = builder.CreateLoad(field_type, field_ptr);
        
    } else {
        result_ = nullptr;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Match表达式和模式匹配 - 已移至 match_codegen.cpp
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// visit(MatchExpr*)、generatePatternMatch()、bindPatternVariables()
// 以及所有Pattern相关方法已移至match_codegen.cpp

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
        llvm::Type* llvm_elem_type = context_->getLLVMType(elem_type);
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
    
    Type* array_type = node->getType();
    if (!array_type) {
        result_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_array_type = context_->getLLVMType(array_type);
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
    
    Type* tuple_type = node->getType();
    if (!tuple_type) {
        result_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_tuple_type = context_->getLLVMType(tuple_type);
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
    
    Type* struct_type = node->getType();
    if (!struct_type || struct_type->getKind() != Type::Kind::Struct) {
        result_ = nullptr;
        return;
    }
    
    // 使用CodeGenContext的类型映射确保一致性
    llvm::Type* llvm_struct_type = context_->getLLVMType(struct_type);
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

void ExprCodeGen::visit(CastExpr* node) {
    // as类型转换: expr as TargetType
    node->getExpr()->accept(this);
    llvm::Value* source_value = result_;
    
    if (!source_value) {
        result_ = nullptr;
        return;
    }
    
    auto& builder = context_->getBuilder();
    
    Type* source_type = node->getExpr()->getType();
    Type* target_type = node->getTargetType();
    
    if (!source_type || !target_type) {
        result_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_target_type = context_->getLLVMType(target_type);
    
    // 数值类型转换
    if (source_type->isInteger() && target_type->isInteger()) {
        // 整数之间的转换
        llvm::Type* source_llvm = source_value->getType();
        unsigned source_bits = source_llvm->getIntegerBitWidth();
        unsigned target_bits = llvm_target_type->getIntegerBitWidth();
        
        if (source_bits < target_bits) {
            // 扩展
            if (source_type->isSignedInteger()) {
                result_ = builder.CreateSExt(source_value, llvm_target_type, "cast.sext");
            } else {
                result_ = builder.CreateZExt(source_value, llvm_target_type, "cast.zext");
            }
        } else if (source_bits > target_bits) {
            // 截断
            result_ = builder.CreateTrunc(source_value, llvm_target_type, "cast.trunc");
        } else {
            // 位数相同，直接使用
            result_ = source_value;
        }
    }
    else if (source_type->isInteger() && target_type->isFloat()) {
        // 整数到浮点
        if (source_type->isSignedInteger()) {
            result_ = builder.CreateSIToFP(source_value, llvm_target_type, "cast.sitofp");
        } else {
            result_ = builder.CreateUIToFP(source_value, llvm_target_type, "cast.uitofp");
        }
    }
    else if (source_type->isFloat() && target_type->isInteger()) {
        // 浮点到整数（截断）
        if (target_type->isSignedInteger()) {
            result_ = builder.CreateFPToSI(source_value, llvm_target_type, "cast.fptosi");
        } else {
            result_ = builder.CreateFPToUI(source_value, llvm_target_type, "cast.fptoui");
        }
    }
    else if (source_type->isFloat() && target_type->isFloat()) {
        // 浮点之间的转换
        unsigned source_bits = source_value->getType()->getScalarSizeInBits();
        unsigned target_bits = llvm_target_type->getScalarSizeInBits();
        
        if (source_bits < target_bits) {
            // 扩展
            result_ = builder.CreateFPExt(source_value, llvm_target_type, "cast.fpext");
        } else if (source_bits > target_bits) {
            // 截断
            result_ = builder.CreateFPTrunc(source_value, llvm_target_type, "cast.fptrunc");
        } else {
            result_ = source_value;
        }
    }
    else {
        // 不支持的类型转换
        result_ = nullptr;
    }
}

} // namespace pawc
