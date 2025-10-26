// Pattern matching code generation

#include "codegen.h"
#include <iostream>

namespace pawc {

/**
 * @brief Generate Match expression
 * @param expr Match expression AST node
 * @return Match result value
 * 
 * Generate complete pattern matching expression:
 * value is {
 *     Some(x) => x * 2,
 *     None() => 0,
 * }
 * 
 * Implementation:
 * 1. Create basic blocks for each pattern condition
 * 2. Use conditional branches for pattern checking
 * 3. Bind variables in matched branch
 * 4. Merge results using PHI nodes
 */
llvm::Value* CodeGenerator::generateMatchExpr(const MatchExpr* expr) {
    // Generate value to match
    // For enum types, we need pointer not value
    llvm::Value* value_ptr = nullptr;
    
    if (expr->value->kind == Expr::Kind::Identifier) {
        // Get alloca pointer directly
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
    
    // Get current function
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    
    // Infer result type: default to i32, avoid accessing unbound variables during type inference
    // TODO: Infer more precise type from function return type or explicit type annotation
    llvm::Type* result_type = llvm::Type::getInt32Ty(*context_);
    
    // Create result variable
    llvm::AllocaInst* result_alloca = builder_->CreateAlloca(
        result_type, nullptr, "match_result"
    );
    
    // Create merge block
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(
        *context_, "match_end", func
    );
    
    // Create basic blocks for each arm
    std::vector<llvm::BasicBlock*> arm_blocks;
    std::vector<llvm::BasicBlock*> next_blocks;
    for (size_t i = 0; i < expr->arms.size(); i++) {
        arm_blocks.push_back(llvm::BasicBlock::Create(
            *context_, "match_arm_" + std::to_string(i), func
        ));
        // Only create next_block for non-last arm
        if (i + 1 < expr->arms.size()) {
            next_blocks.push_back(llvm::BasicBlock::Create(
                *context_, "match_next_" + std::to_string(i), func
            ));
        }
    }
    
    // Default block (if all patterns fail to match)
    llvm::BasicBlock* default_bb = llvm::BasicBlock::Create(
        *context_, "match_default", func
    );
    
    // Generate pattern check chain
    for (size_t i = 0; i < expr->arms.size(); i++) {
        const Pattern* pattern = expr->arms[i].pattern.get();
        
        // Set insertion point to current check block
        if (i > 0 && i - 1 < next_blocks.size()) {
            builder_->SetInsertPoint(next_blocks[i - 1]);
        }
        
        // Generate pattern check condition
        llvm::Value* match_cond = nullptr;
        
        if (pattern->kind == Pattern::Kind::Wildcard) {
            // Wildcard always matches
            match_cond = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
        } else if (pattern->kind == Pattern::Kind::Identifier) {
            // Identifier always matches
            match_cond = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
        } else if (pattern->kind == Pattern::Kind::EnumVariant) {
            // Enum variant: check tag
            const EnumVariantPattern* enum_pattern = 
                static_cast<const EnumVariantPattern*>(pattern);
            
            // Find enum definition
            for (const auto& [enum_name, enum_def] : enum_defs_) {
                int variant_tag = 0;
                for (const auto& variant : enum_def->variants) {
                    if (variant.name == enum_pattern->variant_name) {
                        // Get enum type
                        llvm::Type* enum_type = getEnumType(enum_name);
                        if (!enum_type || !enum_type->isStructTy()) {
                            break;
                        }
                        
                        llvm::StructType* struct_type = 
                            llvm::cast<llvm::StructType>(enum_type);
                        
                        // Extract tag
                        llvm::Value* tag_ptr = builder_->CreateStructGEP(
                            struct_type, value_ptr, 0, "tag_ptr"
                        );
                        llvm::Value* tag = builder_->CreateLoad(
                            llvm::Type::getInt32Ty(*context_),
                            tag_ptr, "tag"
                        );
                        
                        // Compare tag
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
        
        // Conditional branch: if matched, jump to arm block; else jump to next check or default
        llvm::BasicBlock* next_dest = (i < next_blocks.size()) ? 
            next_blocks[i] : default_bb;
        builder_->CreateCondBr(match_cond, arm_blocks[i], next_dest);
        
        // Generate arm block code
        builder_->SetInsertPoint(arm_blocks[i]);
        
        // Bind variables
        std::map<std::string, llvm::Value*> bindings;
        matchPattern(value_ptr, pattern, bindings);
        
        // Generate arm expression
        llvm::Value* arm_value = generateExpr(
            expr->arms[i].expression.get()
        );
        if (arm_value) {
            builder_->CreateStore(arm_value, result_alloca);
        }
        
        // Jump to merge block
        builder_->CreateBr(merge_bb);
        
        // Cleanup bindings (remove from named_values_)
        for (const auto& [name, _] : bindings) {
            named_values_.erase(name);
        }
    }
    
    // Default block: return zero value
    builder_->SetInsertPoint(default_bb);
    builder_->CreateStore(
        llvm::Constant::getNullValue(result_type), result_alloca
    );
    builder_->CreateBr(merge_bb);
    
    // Merge block
    builder_->SetInsertPoint(merge_bb);
    return builder_->CreateLoad(result_type, result_alloca, "match_result");
}

/**
 * @brief Generate Is expression
 * @param expr Is expression AST node
 * @return Boolean comparison result
 * 
 * Generate pattern matching conditional expression:
 * value is Some(x)
 * 
 * Used for conditional checks, supports variable binding.
 * Note: Bound variables are not visible outside is expression, must be handled in if statement.
 */
llvm::Value* CodeGenerator::generateIsExpr(const IsExpr* expr) {
    // For enum matching, we need pointer not value
    llvm::Value* value_ptr = nullptr;
    
    if (expr->value->kind == Expr::Kind::Identifier) {
        // Get alloca pointer directly, don't load
        const IdentifierExpr* id_expr = 
            static_cast<const IdentifierExpr*>(expr->value.get());
        auto it = named_values_.find(id_expr->name);
        if (it != named_values_.end()) {
            value_ptr = it->second;
        }
    } else {
        // Other expressions, generate value
        llvm::Value* value = generateExpr(expr->value.get());
        if (!value) return nullptr;
        value_ptr = value;
    }
    
    if (!value_ptr) return nullptr;
    
    // Handle enum variant pattern
    if (expr->pattern->kind == Pattern::Kind::EnumVariant) {
        const EnumVariantPattern* enum_pattern = 
            static_cast<const EnumVariantPattern*>(expr->pattern.get());
        
        // Find enum definition and get variant's tag value
        // 如果pattern指定了enum_name，优先使用它
        std::vector<std::pair<std::string, const EnumStmt*>> enums_to_check;
        if (!enum_pattern->enum_name.empty()) {
            // 优先检查指定的enum
            auto it = enum_defs_.find(enum_pattern->enum_name);
            if (it != enum_defs_.end()) {
                enums_to_check.push_back(*it);
            }
        } else {
            // 没有指定，遍历所有enum
            for (const auto& pair : enum_defs_) {
                enums_to_check.push_back(pair);
            }
        }
        
        for (const auto& [enum_name, enum_def] : enums_to_check) {
            int variant_tag = 0;
            for (const auto& variant : enum_def->variants) {
                if (variant.name == enum_pattern->variant_name) {
                    // Found matching variant!
                    // Check if it's Optional type
                    bool is_optional = (enum_name == "Optional");
                    
                    // Unified enum handling: get type directly from variable_types_
                    llvm::Type* enum_type = getEnumType(enum_name);
                    llvm::Value* value_to_check = value_ptr;
                    
                    // 【关键修复】：检查value_to_check是值、指针还是指向指针的指针
                    if (value_to_check->getType() == enum_type) {
                        // 这是struct值，需要存储到临时alloca
                        llvm::AllocaInst* temp_alloca = builder_->CreateAlloca(
                            enum_type, nullptr, "enum_temp"
                        );
                        builder_->CreateStore(value_to_check, temp_alloca);
                        value_to_check = temp_alloca;
                    } else if (value_to_check->getType()->isPointerTy() && is_optional) {
                        // 【关键修复】：对于Optional类型，value_to_check可能是alloca（存储heap ptr）
                        // 需要load出heap指针
                        llvm::Value* heap_ptr = builder_->CreateLoad(
                            llvm::PointerType::get(*context_, 0),
                            value_to_check,
                            "optional_heap_ptr"
                        );
                        value_to_check = heap_ptr;
                    }
                    // 否则value_to_check已经是指针，可以直接使用
                    
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
                    
                    // Compare tag
                    llvm::Value* expected_tag = llvm::ConstantInt::get(
                        *context_, 
                        llvm::APInt(32, variant_tag)
                    );
                    llvm::Value* cmp = builder_->CreateICmpEQ(tag, expected_tag, "tag_match");
                    
                    // If need to bind variables and is Optional type
                    if (!enum_pattern->bindings.empty() && is_optional) {
                        // Prepare for conditional branch: bind variables in then block
                        // Note: This binding needs to take effect in if statement's then block
                        // For now return comparison result, binding handled by IfStmt
                    }
                    
                    return cmp;
                }
                variant_tag++;
            }
        }
    }
    
    // Identifier pattern: always matches (and binds variable)
    if (expr->pattern->kind == Pattern::Kind::Identifier) {
        // TODO: bind value to variable
        return llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
    }
    
    // Wildcard pattern: always matches
    if (expr->pattern->kind == Pattern::Kind::Wildcard) {
        return llvm::ConstantInt::get(*context_, llvm::APInt(1, 1));
    }
    
    return llvm::ConstantInt::get(*context_, llvm::APInt(1, 0));
}

/**
 * @brief Pattern matching helper function
 * @param value Value to match
 * @param pattern Pattern
 * @param bindings Bound variable mapping
 * @return Whether match succeeded
 * 
 * Supported pattern types:
 * - Wildcard: wildcard _, always matches
 * - Identifier: identifier x, always matches and binds variable
 * - EnumVariant: enum variant Some(x), check tag and bind associated value
 */
bool CodeGenerator::matchPattern(llvm::Value* value, const Pattern* pattern,
                                 std::map<std::string, llvm::Value*>& bindings) {
    switch (pattern->kind) {
        case Pattern::Kind::Wildcard:
            // Wildcard always matches
            return true;
            
        case Pattern::Kind::Identifier: {
            // Identifier pattern: always matches and binds variable
            const IdentifierPattern* id_pattern = 
                static_cast<const IdentifierPattern*>(pattern);
            
            // Create local variable and store value
            llvm::Type* value_type = value->getType();
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                value_type,
                nullptr,
                id_pattern->name
            );
            builder_->CreateStore(value, alloca);
            
            // Add to binding map
            bindings[id_pattern->name] = alloca;
            named_values_[id_pattern->name] = alloca;
            
            return true;
        }
        
        case Pattern::Kind::EnumVariant: {
            // Enum variant pattern: check tag and bind associated value
            const EnumVariantPattern* enum_pattern = 
                static_cast<const EnumVariantPattern*>(pattern);
            
            // Find enum definition and get variant's tag value
            for (const auto& [enum_name, enum_def] : enum_defs_) {
                int variant_tag = 0;
                for (const auto& variant : enum_def->variants) {
                    if (variant.name == enum_pattern->variant_name) {
                        // Found matching variant
                        
                        // Get enum type
                        llvm::Type* enum_type = getEnumType(enum_name);
                        if (!enum_type || !enum_type->isStructTy()) {
                            return false;
                        }
                        
                        llvm::StructType* struct_type = 
                            llvm::cast<llvm::StructType>(enum_type);
                        
                        // Extract tag field
                        llvm::Value* tag_ptr = builder_->CreateStructGEP(
                            struct_type, value, 0, "tag_ptr"
                        );
                        llvm::Value* tag = builder_->CreateLoad(
                            llvm::Type::getInt32Ty(*context_),
                            tag_ptr,
                            "tag"
                        );
                        
                        // Compare tag
                        llvm::Value* expected_tag = llvm::ConstantInt::get(
                            *context_,
                            llvm::APInt(32, variant_tag)
                        );
                        
                        // Compare tag (simplified here, actual runtime will check)
                        // In matchPattern, we assume caller already checked tag via is expression
                        
                        // Bind associated values
                        if (!enum_pattern->bindings.empty() && 
                            !variant.associated_types.empty()) {
                            // Extract data field
                            llvm::Value* data_ptr = builder_->CreateStructGEP(
                                struct_type, value, 1, "data_ptr"
                            );
                            
                            // Convert based on associated value type
                            llvm::Type* data_type = struct_type->getElementType(1);
                            llvm::Value* data = builder_->CreateLoad(
                                data_type, data_ptr, "data"
                            );
                            
                            // Bind to first variable (simplified: only support one associated value)
                            if (enum_pattern->bindings[0]->kind == 
                                Pattern::Kind::Identifier) {
                                const IdentifierPattern* id_pattern = 
                                    static_cast<const IdentifierPattern*>(
                                        enum_pattern->bindings[0].get()
                                    );
                                
                                // Type conversion (if needed)
                                llvm::Type* target_type = 
                                    convertType(variant.associated_types[0].get());
                                llvm::Value* bound_val = data;
                                
                                if (data_type != target_type) {
                                    // Integer type conversion
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
                                
                                // Create local variable
                                llvm::AllocaInst* alloca = 
                                    builder_->CreateAlloca(
                                        target_type,
                                        nullptr,
                                        id_pattern->name
                                    );
                                builder_->CreateStore(bound_val, alloca);
                                
                                // Add to bindings
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
            // Literal pattern (not implemented)
            std::cerr << "Literal pattern not implemented" << std::endl;
            return false;
            
        case Pattern::Kind::Struct:
            // Struct pattern (not implemented)
            std::cerr << "Struct pattern not implemented" << std::endl;
            return false;
            
        default:
            return false;
    }
}

} // namespace pawc
