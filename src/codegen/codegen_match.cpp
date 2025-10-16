// 模式匹配代码生成

#include "codegen.h"
#include <iostream>

namespace pawc {

/**
 * @brief Match表达式生成
 * @param expr Match表达式AST节点
 * @return 匹配结果值
 * 
 * 生成完整的模式匹配表达式：
 * value is {
 *     Some(x) => x * 2,
 *     None() => 0,
 * }
 * 
 * 实现方式：
 * 1. 根据每个pattern的条件创建基本块
 * 2. 使用条件分支实现模式检查
 * 3. 在匹配的分支中绑定变量
 * 4. 使用PHI节点合并结果
 */
llvm::Value* CodeGenerator::generateMatchExpr(const MatchExpr* expr) {
    // 生成要匹配的值
    // 对于enum类型，我们需要指针而不是值
    llvm::Value* value_ptr = nullptr;
    
    if (expr->value->kind == Expr::Kind::Identifier) {
        // 直接获取alloca指针
        const IdentifierExpr* id_expr = 
            static_cast<const IdentifierExpr*>(expr->value.get());
        auto it = named_values_.find(id_expr->name);
        if (it != named_values_.end()) {
            value_ptr = it->second;
        }
    }
    
    if (!value_ptr) {
        std::cerr << "Match value must be an identifier" << std::endl;
        return nullptr;
    }
    
    // 获取当前函数
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    
    // 推断结果类型：默认使用i32，避免在类型推断阶段访问未绑定的变量
    // TODO: 从函数返回类型或显式类型注解推断更精确的类型
    llvm::Type* result_type = llvm::Type::getInt32Ty(*context_);
    
    // 创建结果变量
    llvm::AllocaInst* result_alloca = builder_->CreateAlloca(
        result_type, nullptr, "match_result"
    );
    
    // 创建合并块
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(
        *context_, "match_end", func
    );
    
    // 为每个arm创建基本块
    std::vector<llvm::BasicBlock*> arm_blocks;
    std::vector<llvm::BasicBlock*> next_blocks;
    for (size_t i = 0; i < expr->arms.size(); i++) {
        arm_blocks.push_back(llvm::BasicBlock::Create(
            *context_, "match_arm_" + std::to_string(i), func
        ));
        // 只为非最后一个arm创建next_block
        if (i + 1 < expr->arms.size()) {
            next_blocks.push_back(llvm::BasicBlock::Create(
                *context_, "match_next_" + std::to_string(i), func
            ));
        }
    }
    
    // 默认块（如果所有模式都不匹配）
    llvm::BasicBlock* default_bb = llvm::BasicBlock::Create(
        *context_, "match_default", func
    );
    
    // 生成模式检查链
    for (size_t i = 0; i < expr->arms.size(); i++) {
        const Pattern* pattern = expr->arms[i].pattern.get();
        
        // 设置插入点到当前检查块
        if (i > 0 && i - 1 < next_blocks.size()) {
            builder_->SetInsertPoint(next_blocks[i - 1]);
        }
        
        // 生成模式检查条件
        llvm::Value* match_cond = nullptr;
        
        if (pattern->kind == Pattern::Kind::Wildcard) {
            // 通配符总是匹配
            match_cond = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
        } else if (pattern->kind == Pattern::Kind::Identifier) {
            // 标识符总是匹配
            match_cond = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
        } else if (pattern->kind == Pattern::Kind::EnumVariant) {
            // 枚举变体：检查tag
            const EnumVariantPattern* enum_pattern = 
                static_cast<const EnumVariantPattern*>(pattern);
            
            // 查找enum定义
            for (const auto& [enum_name, enum_def] : enum_defs_) {
                int variant_tag = 0;
                for (const auto& variant : enum_def->variants) {
                    if (variant.name == enum_pattern->variant_name) {
                        // 获取enum类型
                        llvm::Type* enum_type = getEnumType(enum_name);
                        if (!enum_type || !enum_type->isStructTy()) {
                            break;
                        }
                        
                        llvm::StructType* struct_type = 
                            llvm::cast<llvm::StructType>(enum_type);
                        
                        // 提取tag
                        llvm::Value* tag_ptr = builder_->CreateStructGEP(
                            struct_type, value_ptr, 0, "tag_ptr"
                        );
                        llvm::Value* tag = builder_->CreateLoad(
                            llvm::Type::getInt32Ty(*context_),
                            tag_ptr, "tag"
                        );
                        
                        // 比较tag
                        llvm::Value* expected_tag = llvm::ConstantInt::get(
                            *context_, llvm::APInt(32, variant_tag)
                        );
                        match_cond = builder_->CreateICmpEQ(
                            tag, expected_tag, "tag_match"
                        );
                        break;
                    }
                    variant_tag++;
                }
                if (match_cond) break;
            }
        }
        
        if (!match_cond) {
            match_cond = llvm::ConstantInt::get(*context_, llvm::APInt(1, 0));
        }
        
        // 条件分支：如果匹配，跳转到arm块；否则跳转到下一个检查或default
        llvm::BasicBlock* next_dest = (i < next_blocks.size()) ? 
            next_blocks[i] : default_bb;
        builder_->CreateCondBr(match_cond, arm_blocks[i], next_dest);
        
        // 生成arm块代码
        builder_->SetInsertPoint(arm_blocks[i]);
        
        // 绑定变量
        std::map<std::string, llvm::Value*> bindings;
        matchPattern(value_ptr, pattern, bindings);
        
        // 生成arm表达式
        llvm::Value* arm_value = generateExpr(
            expr->arms[i].expression.get()
        );
        if (arm_value) {
            builder_->CreateStore(arm_value, result_alloca);
        }
        
        // 跳转到合并块
        builder_->CreateBr(merge_bb);
        
        // 清理绑定（从named_values_中移除）
        for (const auto& [name, _] : bindings) {
            named_values_.erase(name);
        }
    }
    
    // 默认块：返回零值
    builder_->SetInsertPoint(default_bb);
    builder_->CreateStore(
        llvm::Constant::getNullValue(result_type), result_alloca
    );
    builder_->CreateBr(merge_bb);
    
    // 合并块
    builder_->SetInsertPoint(merge_bb);
    return builder_->CreateLoad(result_type, result_alloca, "match_result");
}

/**
 * @brief Is表达式生成
 * @param expr Is表达式AST节点
 * @return 布尔比较结果
 * 
 * 生成模式匹配条件表达式：
 * value is Some(x)
 * 
 * 用于条件判断，支持变量绑定。
 * 注意：绑定的变量在is表达式外部不可见，需要在if语句中处理。
 */
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

/**
 * @brief 模式匹配辅助函数
 * @param value 要匹配的值
 * @param pattern 模式
 * @param bindings 绑定的变量映射
 * @return 是否匹配成功
 * 
 * 支持的模式类型：
 * - Wildcard: 通配符 _，总是匹配
 * - Identifier: 标识符 x，总是匹配并绑定变量
 * - EnumVariant: 枚举变体 Some(x)，检查tag并绑定关联值
 */
bool CodeGenerator::matchPattern(llvm::Value* value, const Pattern* pattern,
                                 std::map<std::string, llvm::Value*>& bindings) {
    switch (pattern->kind) {
        case Pattern::Kind::Wildcard:
            // 通配符总是匹配
            return true;
            
        case Pattern::Kind::Identifier: {
            // 标识符模式：总是匹配并绑定变量
            const IdentifierPattern* id_pattern = 
                static_cast<const IdentifierPattern*>(pattern);
            
            // 创建局部变量并存储值
            llvm::Type* value_type = value->getType();
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                value_type,
                nullptr,
                id_pattern->name
            );
            builder_->CreateStore(value, alloca);
            
            // 添加到绑定映射
            bindings[id_pattern->name] = alloca;
            named_values_[id_pattern->name] = alloca;
            
            return true;
        }
        
        case Pattern::Kind::EnumVariant: {
            // 枚举变体模式：检查tag并绑定关联值
            const EnumVariantPattern* enum_pattern = 
                static_cast<const EnumVariantPattern*>(pattern);
            
            // 查找enum定义并获取variant的tag值
            for (const auto& [enum_name, enum_def] : enum_defs_) {
                int variant_tag = 0;
                for (const auto& variant : enum_def->variants) {
                    if (variant.name == enum_pattern->variant_name) {
                        // 找到匹配的variant
                        
                        // 获取enum类型
                        llvm::Type* enum_type = getEnumType(enum_name);
                        if (!enum_type || !enum_type->isStructTy()) {
                            return false;
                        }
                        
                        llvm::StructType* struct_type = 
                            llvm::cast<llvm::StructType>(enum_type);
                        
                        // 提取tag字段
                        llvm::Value* tag_ptr = builder_->CreateStructGEP(
                            struct_type, value, 0, "tag_ptr"
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
                        
                        // 比较tag（这里简化处理，实际runtime会检查）
                        // 在matchPattern中，我们假设调用者已经通过is表达式检查了tag
                        
                        // 绑定关联值
                        if (!enum_pattern->bindings.empty() && 
                            !variant.associated_types.empty()) {
                            // 提取data字段
                            llvm::Value* data_ptr = builder_->CreateStructGEP(
                                struct_type, value, 1, "data_ptr"
                            );
                            
                            // 根据关联值类型进行转换
                            llvm::Type* data_type = struct_type->getElementType(1);
                            llvm::Value* data = builder_->CreateLoad(
                                data_type, data_ptr, "data"
                            );
                            
                            // 绑定到第一个变量（简化：只支持一个关联值）
                            if (enum_pattern->bindings[0]->kind == 
                                Pattern::Kind::Identifier) {
                                const IdentifierPattern* id_pattern = 
                                    static_cast<const IdentifierPattern*>(
                                        enum_pattern->bindings[0].get()
                                    );
                                
                                // 类型转换（如果需要）
                                llvm::Type* target_type = 
                                    convertType(variant.associated_types[0].get());
                                llvm::Value* bound_val = data;
                                
                                if (data_type != target_type) {
                                    // 整数类型转换
                                    if (data_type->isIntegerTy() && 
                                        target_type->isIntegerTy()) {
                                        unsigned src_bits = 
                                            data_type->getIntegerBitWidth();
                                        unsigned tgt_bits = 
                                            target_type->getIntegerBitWidth();
                                        
                                        if (src_bits > tgt_bits) {
                                            bound_val = builder_->CreateTrunc(
                                                data, target_type, "trunc"
                                            );
                                        } else if (src_bits < tgt_bits) {
                                            bound_val = builder_->CreateSExt(
                                                data, target_type, "sext"
                                            );
                                        }
                                    }
                                }
                                
                                // 创建局部变量
                                llvm::AllocaInst* alloca = 
                                    builder_->CreateAlloca(
                                        target_type,
                                        nullptr,
                                        id_pattern->name
                                    );
                                builder_->CreateStore(bound_val, alloca);
                                
                                // 添加到绑定
                                bindings[id_pattern->name] = alloca;
                                named_values_[id_pattern->name] = alloca;
                            }
                        }
                        
                        return true;
                    }
                    variant_tag++;
                }
            }
            
            return false;
        }
        
        case Pattern::Kind::Literal:
            // Literal模式（未实现）
            std::cerr << "Literal pattern not implemented" << std::endl;
            return false;
            
        case Pattern::Kind::Struct:
            // Struct模式（未实现）
            std::cerr << "Struct pattern not implemented" << std::endl;
            return false;
            
        default:
            return false;
    }
}

} // namespace pawc
