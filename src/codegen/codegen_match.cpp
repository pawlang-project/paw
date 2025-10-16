// 模式匹配代码生成

#include "codegen.h"
#include <iostream>

namespace pawc {

llvm::Value* CodeGenerator::generateMatchExpr(const MatchExpr* expr) {
    // 对于identifier，直接获取指针而不是load值
    llvm::Value* value_ptr = nullptr;
    
    if (expr->value->kind == Expr::Kind::Identifier) {
        const IdentifierExpr* id_expr = static_cast<const IdentifierExpr*>(expr->value.get());
        auto it = named_values_.find(id_expr->name);
        if (it != named_values_.end()) {
            value_ptr = it->second;  // 直接使用alloca指针
        }
    } else {
        // 其他表达式，生成值
        llvm::Value* value = generateExpr(expr->value.get());
        if (!value) return nullptr;
        value_ptr = value;
    }
    
    if (!value_ptr) return nullptr;
    
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    
    // 获取enum类型（简化：{i32, i64}）
    llvm::StructType* enum_struct_type = llvm::StructType::get(*context_, {
        llvm::Type::getInt32Ty(*context_),
        llvm::Type::getInt64Ty(*context_)
    });
    
    // 提取enum的tag
    llvm::Value* tag_ptr = builder_->CreateStructGEP(
        enum_struct_type,
        value_ptr,
        0,
        "tag_ptr"
    );
    llvm::Value* tag = builder_->CreateLoad(
        llvm::Type::getInt32Ty(*context_),
        tag_ptr,
        "tag"
    );
    
    // 创建结果变量
    llvm::Type* result_type = llvm::Type::getInt32Ty(*context_);
    llvm::AllocaInst* result_alloca = builder_->CreateAlloca(result_type, nullptr, "match_result");
    
    // 创建合并块
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "match_end", func);
    
    // 为每个arm创建块
    std::vector<llvm::BasicBlock*> arm_blocks;
    for (size_t i = 0; i < expr->arms.size(); i++) {
        arm_blocks.push_back(llvm::BasicBlock::Create(*context_, "match_arm", func));
    }
    llvm::BasicBlock* default_bb = llvm::BasicBlock::Create(*context_, "match_default", func);
    
    // 创建switch语句
    llvm::SwitchInst* switch_inst = builder_->CreateSwitch(tag, default_bb, expr->arms.size());
    
    // 生成每个分支
    for (size_t i = 0; i < expr->arms.size(); i++) {
        const Pattern* pattern = expr->arms[i].pattern.get();
        
        // 如果是enum变体模式
        if (pattern->kind == Pattern::Kind::EnumVariant) {
            const EnumVariantPattern* enum_pattern = 
                static_cast<const EnumVariantPattern*>(pattern);
            
            // 查找variant的tag值
            for (const auto& [enum_name, enum_def] : enum_defs_) {
                int variant_tag = 0;
                for (const auto& variant : enum_def->variants) {
                    if (variant.name == enum_pattern->variant_name) {
                        // 添加case
                        llvm::ConstantInt* case_val = llvm::ConstantInt::get(
                            *context_,
                            llvm::APInt(32, variant_tag)
                        );
                        switch_inst->addCase(case_val, arm_blocks[i]);
                        break;
                    }
                    variant_tag++;
                }
            }
        } else {
            // 其他模式（通配符、标识符）- 作为default
            switch_inst->setDefaultDest(arm_blocks[i]);
        }
        
        // 生成分支代码
        builder_->SetInsertPoint(arm_blocks[i]);
        
        // 绑定变量（简化：从enum中提取值）
        if (pattern->kind == Pattern::Kind::EnumVariant) {
            const EnumVariantPattern* enum_pattern = 
                static_cast<const EnumVariantPattern*>(pattern);
            
            // 如果有绑定变量，提取关联值
            if (!enum_pattern->bindings.empty()) {
                // 从enum的data字段提取值
                llvm::Value* data_ptr = builder_->CreateStructGEP(
                    enum_struct_type,
                    value_ptr,
                    1,
                    "data_ptr"
                );
                llvm::Value* data = builder_->CreateLoad(
                    llvm::Type::getInt64Ty(*context_),
                    data_ptr,
                    "data"
                );
                
                // 绑定到第一个变量（简化：只支持一个）
                if (enum_pattern->bindings[0]->kind == Pattern::Kind::Identifier) {
                    const IdentifierPattern* id_pattern = 
                        static_cast<const IdentifierPattern*>(enum_pattern->bindings[0].get());
                    
                    // 转换回i32
                    llvm::Value* bound_val = builder_->CreateTrunc(
                        data,
                        llvm::Type::getInt32Ty(*context_),
                        id_pattern->name
                    );
                    
                    // 创建局部变量
                    llvm::AllocaInst* alloca = builder_->CreateAlloca(
                        llvm::Type::getInt32Ty(*context_),
                        nullptr,
                        id_pattern->name
                    );
                    builder_->CreateStore(bound_val, alloca);
                    named_values_[id_pattern->name] = alloca;
                }
            }
        }
        
        // 生成分支表达式
        llvm::Value* arm_value = generateExpr(expr->arms[i].expression.get());
        if (arm_value) {
            builder_->CreateStore(arm_value, result_alloca);
        }
        builder_->CreateBr(merge_bb);
    }
    
    // Default分支
    builder_->SetInsertPoint(default_bb);
    builder_->CreateStore(llvm::ConstantInt::get(result_type, 0), result_alloca);
    builder_->CreateBr(merge_bb);
    
    // 合并块
    builder_->SetInsertPoint(merge_bb);
    return builder_->CreateLoad(result_type, result_alloca, "match_result");
}

llvm::Value* CodeGenerator::generateIsExpr(const IsExpr* expr) {
    llvm::Value* value = generateExpr(expr->value.get());
    if (!value) return nullptr;
    
    // 处理enum变体模式
    if (expr->pattern->kind == Pattern::Kind::EnumVariant) {
        const EnumVariantPattern* enum_pattern = 
            static_cast<const EnumVariantPattern*>(expr->pattern.get());
        
        // 查找enum定义，获取variant的tag值
        for (const auto& [enum_name, enum_def] : enum_defs_) {
            int variant_tag = 0;
            for (const auto& variant : enum_def->variants) {
                if (variant.name == enum_pattern->variant_name) {
                    // 找到匹配的variant！
                    // 判断是否是Optional类型
                    bool is_optional = (enum_name == "Optional");
                    
                    llvm::Type* enum_type;
                    llvm::Value* value_to_check = value;
                    
                    if (is_optional) {
                        // Optional类型：T?现在统一为指针
                        // value是ptr，需要找到它指向的Optional<T>类型
                        if (!value->getType()->isPointerTy()) {
                            std::cerr << "Error: T? must be a pointer" << std::endl;
                            return nullptr;
                        }
                        
                        // 从variable_types_获取实际Optional类型
                        // 策略1：直接从LoadInst -> AllocaInst获取名称
                        std::string var_name;
                        if (auto* load_inst = llvm::dyn_cast<llvm::LoadInst>(value)) {
                            auto* ptr_operand = load_inst->getPointerOperand();
                            if (auto* alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(ptr_operand)) {
                                var_name = alloca_inst->getName().str();
                            } else if (auto* inner_load = llvm::dyn_cast<llvm::LoadInst>(ptr_operand)) {
                                // 参数传递：r1 -> result -> load(result) -> load(load(result))
                                if (auto* inner_alloca = llvm::dyn_cast<llvm::AllocaInst>(inner_load->getPointerOperand())) {
                                    var_name = inner_alloca->getName().str();
                                }
                            }
                        }
                        
                        // 如果找到变量名，尝试从variable_types_获取
                        if (!var_name.empty() && variable_types_.count(var_name)) {
                            enum_type = variable_types_[var_name];
                        } else {
                            // 策略2：枚举所有已知的Optional<T>类型，找到第一个匹配的
                            // 这是一个fallback策略，适用于类型推断困难的情况
                            for (const auto& [name, type] : variable_types_) {
                                if (type->isStructTy()) {
                                    llvm::StructType* st = llvm::cast<llvm::StructType>(type);
                                    // 检查是否是Optional类型：{i32 tag, T value, ptr error}
                                    if (st->getNumElements() == 3) {
                                        // 可能是Optional<T>
                                        enum_type = type;
                                        break;
                                    }
                                }
                            }
                            
                            if (!enum_type) {
                                std::cerr << "Error: Cannot infer Optional type for var_name=" << var_name << std::endl;
                                return nullptr;
                            }
                        }
                        
                        // value是指向Optional的指针，需要直接使用
                        // 注意：在泛型函数中，参数是 alloca ptr，存储了指向heap的ptr
                        // 所以需要先load一次获取实际的heap指针
                        value_to_check = value;
                    } else {
                        enum_type = getEnumType(enum_name);
                        value_to_check = value;
                    }
                    
                    // 【关键修复】：对于T?，value可能是load(alloca ptr)的结果
                    // 这已经是heap指针了，可以直接用于GEP
                    // 但如果函数接收的是参数，value是指向指针的指针的load结果
                    // 无论哪种情况，value都应该是指向Optional struct的指针
                    
                    llvm::Value* tag_ptr = builder_->CreateStructGEP(
                        static_cast<llvm::StructType*>(enum_type),
                        value_to_check, 
                        0, 
                        "tag_ptr"
                    );
                    llvm::Value* tag = builder_->CreateLoad(
                        llvm::Type::getInt32Ty(*context_),
                        tag_ptr,
                        "tag"
                    );
                    
                    // 比较tag
                    llvm::Value* expected_tag = llvm::ConstantInt::get(
                        *context_, 
                        llvm::APInt(32, variant_tag)
                    );
                    llvm::Value* cmp = builder_->CreateICmpEQ(tag, expected_tag, "tag_match");
                    
                    // 如果需要绑定变量且是Optional类型
                    if (!enum_pattern->bindings.empty() && is_optional) {
                        // 为条件分支准备：在then块中绑定变量
                        // 注意：这个绑定需要在if语句的then块中生效
                        // 暂时先返回比较结果，绑定由IfStmt处理
                    }
                    
                    return cmp;
                }
                variant_tag++;
            }
        }
    }
    
    // 标识符模式：总是匹配（并绑定变量）
    if (expr->pattern->kind == Pattern::Kind::Identifier) {
        // TODO: 绑定值到变量
        return llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
    }
    
    // 通配符模式：总是匹配
    if (expr->pattern->kind == Pattern::Kind::Wildcard) {
        return llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
    }
    
    return llvm::ConstantInt::get(*context_, llvm::APInt(1, 0));
}

bool CodeGenerator::matchPattern(llvm::Value* value, const Pattern* pattern,
                                 std::map<std::string, llvm::Value*>& bindings) {
    // 模式匹配辅助函数
    // 简化实现
    return true;
}

} // namespace pawc
