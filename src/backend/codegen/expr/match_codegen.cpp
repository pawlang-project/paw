//===--- match_codegen.cpp - Match Expression CodeGen -----------*- C++ -*-===//
//
// Match表达式代码生成：模式匹配、分支生成
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "frontend/parser/ast/pattern.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/generic_types.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Match表达式主实现
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

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

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 模式匹配条件生成
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

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
        // 特殊处理Result类型
        if (scrutinee_type && scrutinee_type->getKind() == Type::Kind::Result) {
            return generateResultPatternMatch(enum_pat, scrutinee, static_cast<ResultType*>(scrutinee_type));
        }
        
        // 特殊处理Optional类型
        if (scrutinee_type && scrutinee_type->getKind() == Type::Kind::Optional) {
            return generateOptionalPatternMatch(enum_pat, scrutinee, static_cast<OptionalType*>(scrutinee_type));
        }
        
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
    
    // StructPattern: 结构体匹配（总是匹配，由TypeChecker保证正确性）
    if (dynamic_cast<StructPattern*>(pattern)) {
        return nullptr;  // 结构体模式总是匹配，类型检查阶段已验证
    }
    
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 模式变量绑定
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

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
        
        // **重要**: 必须注册到context，否则后续使用时找不到！
        context_->defineVariable(var->getName(), var_alloca);
        return;
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
        return;
    }
    
    // EnumPattern: 绑定枚举数据
    if (auto* enum_pat = dynamic_cast<EnumPattern*>(pattern)) {
        // 特殊处理Result类型
        if (value_type && value_type->getKind() == Type::Kind::Result) {
            bindResultPatternVariables(enum_pat, value, static_cast<ResultType*>(value_type));
            return;
        }
        
        // 特殊处理Optional类型
        if (value_type && value_type->getKind() == Type::Kind::Optional) {
            bindOptionalPatternVariables(enum_pat, value, static_cast<OptionalType*>(value_type));
            return;
        }
        
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
        return;
    }
    
    // StructPattern: 提取字段并绑定变量
    if (auto* struct_pat = dynamic_cast<StructPattern*>(pattern)) {
        if (value_type && value_type->getKind() == Type::Kind::Struct) {
            bindStructPatternVariables(struct_pat, value, static_cast<StructType*>(value_type));
        }
        return;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Result类型模式匹配
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Value* ExprCodeGen::generateResultPatternMatch(
    EnumPattern* pattern,
    llvm::Value* scrutinee,
    ResultType* result_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    
    // Result的内存布局: { i1 is_ok, T value, i8* error }
    // Ok => is_ok == true
    // Err => is_ok == false
    
    TypeCodeGen type_gen(ctx);
    llvm::Type* llvm_result_type = type_gen.mapType(result_type);
    
    // 如果scrutinee不是指针，先存到栈上
    llvm::Value* result_ptr = scrutinee;
    if (!scrutinee->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "result.tmp");
        builder.CreateStore(scrutinee, temp);
        result_ptr = temp;
    }
    
    // 提取is_ok字段（第0个字段）
    llvm::Value* is_ok_ptr = builder.CreateStructGEP(
        llvm_result_type,
        result_ptr,
        0,
        "result.is_ok.ptr"
    );
    llvm::Value* is_ok = builder.CreateLoad(
        llvm::Type::getInt1Ty(ctx),
        is_ok_ptr,
        "result.is_ok"
    );
    
    if (variant == "Ok") {
        // Ok变体：is_ok == true
        return is_ok;
    } else if (variant == "Err") {
        // Err变体：is_ok == false
        return builder.CreateNot(is_ok, "result.is_err");
    }
    
    return nullptr;
}

llvm::Value* ExprCodeGen::generateOptionalPatternMatch(
    EnumPattern* pattern,
    llvm::Value* scrutinee,
    OptionalType* opt_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    
    // Optional的内存布局: { i1 has_value, T value }
    // Some => has_value == true
    // None => has_value == false
    
    TypeCodeGen type_gen(ctx);
    llvm::Type* llvm_opt_type = type_gen.mapType(opt_type);
    
    llvm::Value* opt_ptr = scrutinee;
    if (!scrutinee->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "opt.tmp");
        builder.CreateStore(scrutinee, temp);
        opt_ptr = temp;
    }
    
    // 提取has_value字段（第0个字段）
    llvm::Value* has_value_ptr = builder.CreateStructGEP(
        llvm_opt_type,
        opt_ptr,
        0,
        "opt.has_value.ptr"
    );
    llvm::Value* has_value = builder.CreateLoad(
        llvm::Type::getInt1Ty(ctx),
        has_value_ptr,
        "opt.has_value"
    );
    
    if (variant == "Some") {
        return has_value;
    } else if (variant == "None") {
        return builder.CreateNot(has_value, "opt.is_none");
    }
    
    return nullptr;
}

void ExprCodeGen::bindResultPatternVariables(
    EnumPattern* pattern,
    llvm::Value* value,
    ResultType* result_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    auto& inner_patterns = pattern->getInnerPatterns();
    
    if (inner_patterns.empty()) {
        return;  // 没有变量需要绑定
    }
    
    TypeCodeGen type_gen(ctx);
    llvm::Type* llvm_result_type = type_gen.mapType(result_type);
    
    llvm::Value* result_ptr = value;
    if (!value->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "result.tmp");
        builder.CreateStore(value, temp);
        result_ptr = temp;
    }
    
    if (variant == "Ok") {
        // 提取value字段（第1个字段）
        llvm::Value* value_ptr = builder.CreateStructGEP(
            llvm_result_type,
            result_ptr,
            1,
            "result.value.ptr"
        );
        
        llvm::Type* ok_llvm_type = type_gen.mapType(result_type->getOkType());
        llvm::Value* ok_value = builder.CreateLoad(
            ok_llvm_type,
            value_ptr,
            "result.value"
        );
        
        // 递归绑定内部模式变量
        bindPatternVariables(inner_patterns[0].get(), ok_value, result_type->getOkType());
    }
    else if (variant == "Err") {
        // 提取error字段（第2个字段）
        llvm::Value* error_ptr = builder.CreateStructGEP(
            llvm_result_type,
            result_ptr,
            2,
            "result.error.ptr"
        );
        
        llvm::Type* string_llvm_type = context_->getStringType();
        llvm::Value* error_value = builder.CreateLoad(
            string_llvm_type,
            error_ptr,
            "result.error"
        );
        
        // 递归绑定内部模式变量（error是string类型）
        bindPatternVariables(inner_patterns[0].get(), error_value, nullptr);
    }
}

void ExprCodeGen::bindOptionalPatternVariables(
    EnumPattern* pattern,
    llvm::Value* value,
    OptionalType* opt_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    auto& inner_patterns = pattern->getInnerPatterns();
    
    if (inner_patterns.empty() || variant == "None") {
        return;  // None没有值需要绑定
    }
    
    TypeCodeGen type_gen(ctx);
    llvm::Type* llvm_opt_type = type_gen.mapType(opt_type);
    
    llvm::Value* opt_ptr = value;
    if (!value->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "opt.tmp");
        builder.CreateStore(value, temp);
        opt_ptr = temp;
    }
    
    if (variant == "Some") {
        // 提取value字段（第1个字段）
        llvm::Value* value_ptr = builder.CreateStructGEP(
            llvm_opt_type,
            opt_ptr,
            1,
            "opt.value.ptr"
        );
        
        llvm::Type* inner_llvm_type = type_gen.mapType(opt_type->getInnerType());
        llvm::Value* inner_value = builder.CreateLoad(
            inner_llvm_type,
            value_ptr,
            "opt.value"
        );
        
        // 递归绑定内部模式变量
        bindPatternVariables(inner_patterns[0].get(), inner_value, opt_type->getInnerType());
    }
}

void ExprCodeGen::bindStructPatternVariables(
    StructPattern* pattern,
    llvm::Value* value,
    StructType* struct_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    
    TypeCodeGen type_gen(ctx);
    llvm::Type* llvm_struct_type = type_gen.mapType(struct_type);
    
    // 确保value是指针类型
    llvm::Value* struct_ptr = value;
    if (!value->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "struct.tmp");
        builder.CreateStore(value, temp);
        struct_ptr = temp;
    }
    
    // 遍历每个字段模式
    for (const auto& field_pattern : pattern->getFields()) {
        const std::string& field_name = field_pattern.field_name;
        
        // 查找字段在结构体中的索引
        const auto& fields = struct_type->getFields();
        int field_idx = -1;
        Type* field_type = nullptr;
        
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].first == field_name) {
                field_idx = static_cast<int>(i);
                field_type = fields[i].second;
                break;
            }
        }
        
        if (field_idx < 0 || !field_type) {
            continue;  // 字段不存在（TypeChecker应该已经报错）
        }
        
        // 提取字段值
        llvm::Value* field_ptr = builder.CreateStructGEP(
            llvm_struct_type,
            struct_ptr,
            field_idx,
            "struct." + field_name + ".ptr"
        );
        
        llvm::Type* field_llvm_type = type_gen.mapType(field_type);
        llvm::Value* field_value = builder.CreateLoad(
            field_llvm_type,
            field_ptr,
            "struct." + field_name
        );
        
        // 递归绑定字段模式变量
        bindPatternVariables(field_pattern.pattern.get(), field_value, field_type);
    }
}

} // namespace pawc

