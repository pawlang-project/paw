// 语句代码生成

#include "codegen.h"
#include <iostream>

namespace pawc {

void CodeGenerator::generateStmt(const Stmt* stmt) {
    switch (stmt->kind) {
        case Stmt::Kind::Function:
            generateFunctionStmt(static_cast<const FunctionStmt*>(stmt));
            break;
        case Stmt::Kind::Let:
            generateLetStmt(static_cast<const LetStmt*>(stmt));
            break;
        case Stmt::Kind::Return:
            generateReturnStmt(static_cast<const ReturnStmt*>(stmt));
            break;
        case Stmt::Kind::If:
            generateIfStmt(static_cast<const IfStmt*>(stmt));
            break;
        case Stmt::Kind::Loop:
            generateLoopStmt(static_cast<const LoopStmt*>(stmt));
            break;
        case Stmt::Kind::Break:
            generateBreakStmt(static_cast<const BreakStmt*>(stmt));
            break;
        case Stmt::Kind::Continue:
            generateContinueStmt(static_cast<const ContinueStmt*>(stmt));
            break;
        case Stmt::Kind::Block:
            generateBlockStmt(static_cast<const BlockStmt*>(stmt));
            break;
        case Stmt::Kind::Expression:
            generateExprStmt(static_cast<const ExprStmt*>(stmt));
            break;
        case Stmt::Kind::Struct:
            generateStructStmt(static_cast<const StructStmt*>(stmt));
            break;
        case Stmt::Kind::Enum:
            generateEnumStmt(static_cast<const EnumStmt*>(stmt));
            break;
        case Stmt::Kind::TypeAlias:
            // type别名只是包装，实际生成在内部的struct/enum
            if (auto alias = static_cast<const TypeAliasStmt*>(stmt)) {
                generateStmt(alias->definition.get());
            }
            break;
        case Stmt::Kind::Impl:
            generateImplStmt(static_cast<const ImplStmt*>(stmt));
            break;
        case Stmt::Kind::Import:
            // import语句：跳过（将来实现跨文件模块系统）
            break;
        case Stmt::Kind::Extern:
            generateExternStmt(static_cast<const ExternStmt*>(stmt));
            break;
    }
}


void CodeGenerator::generateLetStmt(const LetStmt* stmt) {
    llvm::Type* type = nullptr;
    llvm::Type* actual_type = nullptr;  // 实际存储类型（可能是struct值）
    
    // 如果有类型声明
    if (stmt->type) {
        // 使用resolveGenericType而不是convertType，这样可以正确处理泛型类型T
        type = resolveGenericType(stmt->type.get());
        
        // 特殊处理：如果类型是struct名称，存储指针而不是值（新语义）
        if (stmt->type->kind == Type::Kind::Named) {
            const NamedTypeNode* named = static_cast<const NamedTypeNode*>(stmt->type.get());
            
            // 构造完整类型名（包含泛型参数）
            std::string full_type_name = named->name;
            if (!named->generic_args.empty()) {
                // 泛型实例化类型：生成mangled name
                full_type_name = mangleGenericName(named->name, named->generic_args);
            }
            
            auto struct_type = getOrCreateStructType(full_type_name);
            if (struct_type) {
                // 【重要改动】：struct变量存储指针，不是值
                actual_type = llvm::PointerType::get(*context_, 0);
            } else {
                actual_type = type;
            }
        } else if (stmt->type->kind == Type::Kind::Optional) {
            // 【新增】：T?类型也存储指针（统一语义）
            // Optional<T> = { i32 tag, T value, ptr error }
            // 变量存储：ptr to Optional
            actual_type = llvm::PointerType::get(*context_, 0);
        } else {
            actual_type = type;
        }
        
        // 如果是大小待推导的数组类型，从初始化器推断
        if (stmt->type->kind == Type::Kind::Array) {
            const ArrayTypeNode* array_type = static_cast<const ArrayTypeNode*>(stmt->type.get());
            if (array_type->size == -1 && stmt->initializer) {
                // 检查初始化器是否是数组字面量
                if (stmt->initializer->kind == Expr::Kind::ArrayLiteral) {
                    const ArrayLiteralExpr* array_lit = 
                        static_cast<const ArrayLiteralExpr*>(stmt->initializer.get());
                    
                    // 从字面量推导大小（使用resolveGenericType处理泛型）
                    llvm::Type* elem_type = resolveGenericType(array_type->element_type.get());
                    llvm::Type* inferred_type = llvm::ArrayType::get(elem_type, array_lit->elements.size());
                    type = inferred_type;
                    actual_type = inferred_type;  // 关键：同时更新actual_type！
                }
            }
        }
    } else if (stmt->initializer) {
        // 没有类型声明，从初始化器推导
        // 检查是否是struct literal
        if (stmt->initializer->kind == Expr::Kind::StructLiteral) {
            const StructLiteralExpr* struct_lit = static_cast<const StructLiteralExpr*>(stmt->initializer.get());
            std::string struct_type_name = struct_lit->type_name;
            
            // 生成struct literal（返回heap ptr）
            llvm::Value* init_val = generateExpr(stmt->initializer.get());
            if (!init_val) return;
            
            // alloca存储ptr
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                llvm::PointerType::get(*context_, 0), nullptr, stmt->name
            );
            named_values_[stmt->name] = alloca;
            builder_->CreateStore(init_val, alloca);
            
            // 【关键】：记录实际的StructType，而不是ptr
            auto struct_type = getOrCreateStructType(struct_type_name);
            if (struct_type) {
                variable_types_[stmt->name] = struct_type;
            } else {
                variable_types_[stmt->name] = llvm::PointerType::get(*context_, 0);
            }
            
            return;  // 提前返回
        }
        
        // 检查是否是函数调用返回struct
        if (stmt->initializer->kind == Expr::Kind::Call) {
            const CallExpr* call_expr = static_cast<const CallExpr*>(stmt->initializer.get());
            
            // 生成调用
            llvm::Value* init_val = generateExpr(stmt->initializer.get());
            if (!init_val) return;
            
            // 检查返回值是否是指针（可能是struct）
            if (init_val->getType()->isPointerTy()) {
                // 尝试从所有struct的方法中查找
                std::string found_struct_name;
                
                if (call_expr->callee->kind == Expr::Kind::Identifier) {
                    const IdentifierExpr* id = static_cast<const IdentifierExpr*>(call_expr->callee.get());
                    std::string func_name = id->name;
                    
                    // 在所有struct_methods_中查找这个函数
                    for (const auto& [struct_name, methods] : struct_methods_) {
                        if (methods.find(func_name) != methods.end()) {
                            found_struct_name = struct_name;
                            break;
                        }
                    }
                }
                
                if (!found_struct_name.empty()) {
                    // 找到了！这是struct的静态方法
                    auto struct_type = getOrCreateStructType(found_struct_name);
                    if (struct_type) {
                        // alloca存储ptr
                        llvm::AllocaInst* alloca = builder_->CreateAlloca(
                            llvm::PointerType::get(*context_, 0), nullptr, stmt->name
                        );
                        named_values_[stmt->name] = alloca;
                        builder_->CreateStore(init_val, alloca);
                        
                        // 记录实际的StructType
                        variable_types_[stmt->name] = struct_type;
                        
                        return;  // 提前返回
                    }
                }
            }
        }
        
        // 其他类型的初始化器：正常推导
        llvm::Value* init_val = generateExpr(stmt->initializer.get());
        if (init_val) {
            type = init_val->getType();
        } else {
            type = llvm::Type::getInt32Ty(*context_);
        }
        
        // 类型推导时，直接使用生成的值
        llvm::AllocaInst* alloca = builder_->CreateAlloca(type, nullptr, stmt->name);
        named_values_[stmt->name] = alloca;
        variable_types_[stmt->name] = type;
        
        if (init_val) {
            builder_->CreateStore(init_val, alloca);
        }
        return;  // 提前返回，避免重复生成
    } else {
        // 既没有类型也没有初始化器，默认i32
        type = llvm::Type::getInt32Ty(*context_);
        actual_type = type;
    }
    
    // 使用actual_type分配空间（可能是struct值类型）
    llvm::Type* alloc_type = actual_type ? actual_type : type;
    llvm::AllocaInst* alloca = builder_->CreateAlloca(alloc_type, nullptr, stmt->name);
    named_values_[stmt->name] = alloca;
    
    // 【重要】：保存变量的实际类型
    // 对于T?和struct类型，alloc_type是ptr，但需要记录实际的类型
    if (stmt->type && stmt->type->kind == Type::Kind::Optional) {
        // 获取实际的Optional<T>类型
        llvm::Type* optional_type = resolveGenericType(stmt->type.get());
        variable_types_[stmt->name] = optional_type;
    } else if (stmt->type && stmt->type->kind == Type::Kind::Named) {
        // 对于struct类型，记录实际的StructType而不是ptr
        const NamedTypeNode* named = static_cast<const NamedTypeNode*>(stmt->type.get());
        std::string full_type_name = named->name;
        if (!named->generic_args.empty()) {
            full_type_name = mangleGenericName(named->name, named->generic_args);
        }
        
        auto struct_type = getOrCreateStructType(full_type_name);
        if (struct_type) {
            // 记录实际的StructType
            variable_types_[stmt->name] = struct_type;
        } else {
            variable_types_[stmt->name] = alloc_type;
        }
    } else {
        variable_types_[stmt->name] = alloc_type;  // 其他情况正常保存
    }
    
    if (stmt->initializer) {
        // 特殊处理：数组字面量 - 直接在目标alloca上初始化
        if (stmt->initializer->kind == Expr::Kind::ArrayLiteral && alloc_type->isArrayTy()) {
            const ArrayLiteralExpr* array_lit = static_cast<const ArrayLiteralExpr*>(stmt->initializer.get());
            llvm::ArrayType* array_type = llvm::cast<llvm::ArrayType>(alloc_type);
            llvm::Type* elem_type = array_type->getElementType();
            
            // 直接在alloca上初始化元素
            for (size_t i = 0; i < array_lit->elements.size(); i++) {
                llvm::Value* elem_val = generateExpr(array_lit->elements[i].get());
                if (!elem_val) continue;
                
                // 类型转换：确保元素值匹配数组元素类型
                if (elem_val->getType() != elem_type) {
                    // 整数类型转换
                    if (elem_val->getType()->isIntegerTy() && elem_type->isIntegerTy()) {
                        unsigned src_bits = elem_val->getType()->getIntegerBitWidth();
                        unsigned tgt_bits = elem_type->getIntegerBitWidth();
                        if (src_bits < tgt_bits) {
                            elem_val = builder_->CreateSExt(elem_val, elem_type, "elem_sext");
                        } else if (src_bits > tgt_bits) {
                            elem_val = builder_->CreateTrunc(elem_val, elem_type, "elem_trunc");
                        }
                    }
                }
                
                // 获取元素指针: GEP alloca, 0, i
                llvm::Value* elem_ptr = builder_->CreateInBoundsGEP(
                    array_type,
                    alloca,
                    {llvm::ConstantInt::get(*context_, llvm::APInt(64, 0)),
                     llvm::ConstantInt::get(*context_, llvm::APInt(64, i))},
                    "elem_ptr"
                );
                builder_->CreateStore(elem_val, elem_ptr);
            }
        } else {
            // 其他情况：正常生成初始化器表达式
            llvm::Value* init_val = generateExpr(stmt->initializer.get());
            if (init_val) {
                
                // 检查类型匹配：如果init_val是struct值但alloca需要指针（或反之）
                llvm::Type* init_type = init_val->getType();
                
                // 如果init_val是struct值，alloc_type也是struct，直接存储
                if (init_type == alloc_type) {
                    builder_->CreateStore(init_val, alloca);
                }
                // 如果init_val是指针，alloc_type是struct，需要load
                else if (init_type->isPointerTy() && alloc_type->isStructTy()) {
                    llvm::Value* loaded = builder_->CreateLoad(alloc_type, init_val, "struct_val");
                    builder_->CreateStore(loaded, alloca);
                }
                // 如果init_val是struct值，alloc_type是指针（不应该发生）
                else if (init_type->isStructTy() && alloc_type->isPointerTy()) {
                    // 创建临时alloca并存储值，然后使用指针
                    llvm::AllocaInst* temp = builder_->CreateAlloca(init_type, nullptr, "temp_struct");
                    builder_->CreateStore(init_val, temp);
                    builder_->CreateStore(temp, alloca);
                }
                // 如果类型不完全匹配但都是整数，需要转换
                else if (init_type->isIntegerTy() && alloc_type->isIntegerTy()) {
                    unsigned src_bits = init_type->getIntegerBitWidth();
                    unsigned tgt_bits = alloc_type->getIntegerBitWidth();
                    if (src_bits != tgt_bits) {
                        // 需要类型转换
                        llvm::Value* converted;
                        if (src_bits < tgt_bits) {
                            converted = builder_->CreateSExt(init_val, alloc_type, "sext");
                        } else {
                            converted = builder_->CreateTrunc(init_val, alloc_type, "trunc");
                        }
                        builder_->CreateStore(converted, alloca);
                    } else {
                        builder_->CreateStore(init_val, alloca);
                    }
                }
                else {
                    builder_->CreateStore(init_val, alloca);
                }
            }
        }
    }
}


void CodeGenerator::generateReturnStmt(const ReturnStmt* stmt) {
    if (stmt->value) {
        llvm::Value* ret_val = generateExpr(stmt->value.get());
        if (ret_val) {
            // 检查函数返回类型
            llvm::Type* func_return_type = current_function_->getReturnType();
            
            // 如果函数返回struct值类型，但ret_val是指针，需要load
            if (func_return_type->isStructTy() && ret_val->getType()->isPointerTy()) {
                ret_val = builder_->CreateLoad(func_return_type, ret_val, "ret_struct_val");
            }
            
            builder_->CreateRet(ret_val);
        }
    } else {
        builder_->CreateRetVoid();
    }
}


void CodeGenerator::generateIfStmt(const IfStmt* stmt) {
    // 检查条件是否是IsExpr（用于变量绑定）
    const IsExpr* is_expr = nullptr;
    llvm::Value* is_value = nullptr;
    std::string binding_var_name;
    std::string variant_name;
    
    if (stmt->condition->kind == Expr::Kind::Is) {
        is_expr = static_cast<const IsExpr*>(stmt->condition.get());
        
        // 检查是否有变量绑定
        if (is_expr->pattern->kind == Pattern::Kind::EnumVariant) {
            const EnumVariantPattern* pattern = 
                static_cast<const EnumVariantPattern*>(is_expr->pattern.get());
            
            if (!pattern->bindings.empty() && 
                pattern->bindings[0]->kind == Pattern::Kind::Identifier) {
                // 有变量绑定！保存信息
                const IdentifierPattern* id_pattern = 
                    static_cast<const IdentifierPattern*>(pattern->bindings[0].get());
                binding_var_name = id_pattern->name;
                variant_name = pattern->variant_name;
                
                // 生成被检查的值
                is_value = generateExpr(is_expr->value.get());
            }
        }
    }
    
    llvm::Value* cond = generateExpr(stmt->condition.get());
    if (!cond) return;
    
    cond = builder_->CreateICmpNE(cond, llvm::ConstantInt::get(*context_, llvm::APInt(1, 0)), "ifcond");
    
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context_, "then", func);
    llvm::BasicBlock* else_bb = nullptr;
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "ifcont", func);
    
    if (stmt->else_branch) {
        else_bb = llvm::BasicBlock::Create(*context_, "else", func);
        builder_->CreateCondBr(cond, then_bb, else_bb);
    } else {
        builder_->CreateCondBr(cond, then_bb, merge_bb);
    }
    
    builder_->SetInsertPoint(then_bb);
    
    // 如果有变量绑定，在then块中绑定变量
    if (!binding_var_name.empty() && is_value) {
        // 确保is_value是指针类型
        llvm::Value* value_ptr = is_value;
        if (!is_value->getType()->isPointerTy()) {
            // 如果是struct值，存储到临时变量
            llvm::AllocaInst* temp = builder_->CreateAlloca(
                is_value->getType(), nullptr, "is_temp"
            );
            builder_->CreateStore(is_value, temp);
            value_ptr = temp;
        }
        
        // 获取Optional类型结构
        llvm::StructType* opt_type = nullptr;
        if (value_ptr->getType()->isPointerTy()) {
            // 从variable_types_中获取实际类型，或使用is_value的类型
            opt_type = static_cast<llvm::StructType*>(is_value->getType());
        } else {
            opt_type = static_cast<llvm::StructType*>(is_value->getType());
        }
        
        // 根据变体类型提取值
        if (variant_name == "Value") {
            // 提取value字段（索引1）
            llvm::Value* value_field_ptr = builder_->CreateStructGEP(
                opt_type, value_ptr, 1, "value_field_ptr"
            );
            llvm::Type* value_type = opt_type->getElementType(1);
            llvm::Value* extracted_value = builder_->CreateLoad(
                value_type, value_field_ptr, "extracted_value"
            );
            
            // 创建局部变量并绑定
            llvm::AllocaInst* var_alloca = builder_->CreateAlloca(
                value_type, nullptr, binding_var_name
            );
            builder_->CreateStore(extracted_value, var_alloca);
            named_values_[binding_var_name] = var_alloca;
            variable_types_[binding_var_name] = value_type;
        } else if (variant_name == "Error") {
            // 提取error_msg字段（索引2）
            llvm::Value* error_field_ptr = builder_->CreateStructGEP(
                opt_type, value_ptr, 2, "error_field_ptr"
            );
            llvm::Value* error_msg = builder_->CreateLoad(
                llvm::PointerType::get(*context_, 0), 
                error_field_ptr, 
                "error_msg"
            );
            
            // 创建局部变量并绑定
            llvm::AllocaInst* var_alloca = builder_->CreateAlloca(
                llvm::PointerType::get(*context_, 0), 
                nullptr, 
                binding_var_name
            );
            builder_->CreateStore(error_msg, var_alloca);
            named_values_[binding_var_name] = var_alloca;
            variable_types_[binding_var_name] = llvm::PointerType::get(*context_, 0);
        }
    }
    
    generateStmt(stmt->then_branch.get());
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(merge_bb);
    }
    
    // 清理then块中的变量绑定
    if (!binding_var_name.empty()) {
        named_values_.erase(binding_var_name);
        variable_types_.erase(binding_var_name);
    }
    
    if (stmt->else_branch) {
        builder_->SetInsertPoint(else_bb);
        generateStmt(stmt->else_branch.get());
        if (!builder_->GetInsertBlock()->getTerminator()) {
            builder_->CreateBr(merge_bb);
        }
    }
    
    builder_->SetInsertPoint(merge_bb);
}


void CodeGenerator::generateLoopStmt(const LoopStmt* stmt) {
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    
    switch (stmt->loop_kind) {
        case LoopStmt::LoopKind::Condition:
        case LoopStmt::LoopKind::Infinite: {
            // 条件循环或无限循环
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*context_, "loop", func);
            llvm::BasicBlock* after_bb = llvm::BasicBlock::Create(*context_, "afterloop", func);
            
            // 将循环标签推入栈 (continue → loop_bb, break → after_bb)
            loop_stack_.push_back({loop_bb, after_bb});
            
            builder_->CreateBr(loop_bb);
            builder_->SetInsertPoint(loop_bb);
            
            if (stmt->condition) {
                llvm::Value* cond = generateExpr(stmt->condition.get());
                if (cond) {
                    cond = builder_->CreateICmpNE(cond, llvm::ConstantInt::get(*context_, llvm::APInt(1, 0)), "loopcond");
                    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "loopbody", func);
                    builder_->CreateCondBr(cond, body_bb, after_bb);
                    builder_->SetInsertPoint(body_bb);
                }
            }
            
            generateStmt(stmt->body.get());
            if (!builder_->GetInsertBlock()->getTerminator()) {
                builder_->CreateBr(loop_bb);
            }
            
            // 弹出循环标签
            loop_stack_.pop_back();
            
            builder_->SetInsertPoint(after_bb);
            break;
        }
        
        case LoopStmt::LoopKind::Range: {
            // 范围循环: loop x in start..end {}
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*context_, "rangeloop", func);
            llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "rangebody", func);
            llvm::BasicBlock* after_bb = llvm::BasicBlock::Create(*context_, "afterrange", func);
            
            // 生成起始值和结束值
            llvm::Value* start_val = generateExpr(stmt->range_start.get());
            llvm::Value* end_val = generateExpr(stmt->range_end.get());
            
            // 创建迭代器变量
            llvm::AllocaInst* iter_var = builder_->CreateAlloca(
                llvm::Type::getInt32Ty(*context_), nullptr, stmt->iterator_var
            );
            builder_->CreateStore(start_val, iter_var);
            
            // 保存到named_values_供循环体使用
            auto old_value = named_values_[stmt->iterator_var];
            named_values_[stmt->iterator_var] = iter_var;
            variable_types_[stmt->iterator_var] = llvm::Type::getInt32Ty(*context_);
            
            // 将循环标签推入栈
            loop_stack_.push_back({loop_bb, after_bb});
            
            // 跳转到循环头
            builder_->CreateBr(loop_bb);
            builder_->SetInsertPoint(loop_bb);
            
            // 检查条件: iter < end
            llvm::Value* iter_val = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), iter_var, "iter");
            llvm::Value* cond = builder_->CreateICmpSLT(iter_val, end_val, "rangecond");
            builder_->CreateCondBr(cond, body_bb, after_bb);
            
            // 循环体
            builder_->SetInsertPoint(body_bb);
            generateStmt(stmt->body.get());
            
            // 递增迭代器
            if (!builder_->GetInsertBlock()->getTerminator()) {
                llvm::Value* next_val = builder_->CreateAdd(
                    builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), iter_var),
                    llvm::ConstantInt::get(*context_, llvm::APInt(32, 1)),
                    "nextiter"
                );
                builder_->CreateStore(next_val, iter_var);
                builder_->CreateBr(loop_bb);
            }
            
            // 弹出循环标签
            loop_stack_.pop_back();
            
            // 恢复旧值
            builder_->SetInsertPoint(after_bb);
            if (old_value) {
                named_values_[stmt->iterator_var] = old_value;
            } else {
                named_values_.erase(stmt->iterator_var);
                variable_types_.erase(stmt->iterator_var);
            }
            break;
        }
        
        case LoopStmt::LoopKind::Iterator: {
            // 迭代器循环: loop item in array {}
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*context_, "iterloop", func);
            llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "iterbody", func);
            llvm::BasicBlock* after_bb = llvm::BasicBlock::Create(*context_, "afteriter", func);
            
            // 获取数组
            std::string array_name;
            if (stmt->iterable->kind == Expr::Kind::Identifier) {
                array_name = static_cast<const IdentifierExpr*>(stmt->iterable.get())->name;
            } else {
                std::cerr << "Iterator loop only supports identifiers for now" << std::endl;
                return;
            }
            
            auto arr_it = named_values_.find(array_name);
            auto type_it = variable_types_.find(array_name);
            if (arr_it == named_values_.end() || type_it == variable_types_.end()) {
                std::cerr << "Unknown array: " << array_name << std::endl;
                return;
            }
            
            llvm::Value* array_ptr = arr_it->second;
            llvm::Type* array_type = type_it->second;
            
            if (!llvm::isa<llvm::ArrayType>(array_type)) {
                std::cerr << "Iterator loop requires an array" << std::endl;
                return;
            }
            
            llvm::ArrayType* arr_ty = llvm::cast<llvm::ArrayType>(array_type);
            uint64_t array_len = arr_ty->getNumElements();
            llvm::Type* elem_type = arr_ty->getElementType();
            
            // 创建索引变量
            llvm::AllocaInst* index_var = builder_->CreateAlloca(
                llvm::Type::getInt32Ty(*context_), nullptr, "index"
            );
            builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, 0)), index_var);
            
            // 创建迭代器变量（当前元素）
            llvm::AllocaInst* iter_elem = builder_->CreateAlloca(elem_type, nullptr, stmt->iterator_var);
            
            auto old_value = named_values_[stmt->iterator_var];
            named_values_[stmt->iterator_var] = iter_elem;
            variable_types_[stmt->iterator_var] = elem_type;
            
            // 将循环标签推入栈
            loop_stack_.push_back({loop_bb, after_bb});
            
            // 跳转到循环头
            builder_->CreateBr(loop_bb);
            builder_->SetInsertPoint(loop_bb);
            
            // 检查条件: index < array_len
            llvm::Value* idx = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), index_var, "idx");
            llvm::Value* cond = builder_->CreateICmpSLT(
                idx, 
                llvm::ConstantInt::get(*context_, llvm::APInt(32, array_len)),
                "itercond"
            );
            builder_->CreateCondBr(cond, body_bb, after_bb);
            
            // 循环体
            builder_->SetInsertPoint(body_bb);
            
            // 加载当前元素
            llvm::Value* elem_ptr = builder_->CreateInBoundsGEP(
                array_type, array_ptr, {
                    llvm::ConstantInt::get(*context_, llvm::APInt(64, 0)),
                    builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), index_var)
                }
            );
            llvm::Value* elem_val = builder_->CreateLoad(elem_type, elem_ptr, "elem");
            builder_->CreateStore(elem_val, iter_elem);
            
            generateStmt(stmt->body.get());
            
            // 递增索引
            if (!builder_->GetInsertBlock()->getTerminator()) {
                llvm::Value* next_idx = builder_->CreateAdd(
                    builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), index_var),
                    llvm::ConstantInt::get(*context_, llvm::APInt(32, 1)),
                    "nextidx"
                );
                builder_->CreateStore(next_idx, index_var);
                builder_->CreateBr(loop_bb);
            }
            
            // 弹出循环标签
            loop_stack_.pop_back();
            
            // 恢复
            builder_->SetInsertPoint(after_bb);
            if (old_value) {
                named_values_[stmt->iterator_var] = old_value;
            } else {
                named_values_.erase(stmt->iterator_var);
                variable_types_.erase(stmt->iterator_var);
            }
            break;
        }
    }
}


void CodeGenerator::generateBreakStmt(const BreakStmt* stmt) {
    if (loop_stack_.empty()) {
        std::cerr << "break statement outside of loop" << std::endl;
        return;
    }
    
    // 跳转到当前循环的 break_target
    llvm::BasicBlock* break_target = loop_stack_.back().second;
    builder_->CreateBr(break_target);
}


void CodeGenerator::generateContinueStmt(const ContinueStmt* stmt) {
    if (loop_stack_.empty()) {
        std::cerr << "continue statement outside of loop" << std::endl;
        return;
    }
    
    // 跳转到当前循环的 continue_target
    llvm::BasicBlock* continue_target = loop_stack_.back().first;
    builder_->CreateBr(continue_target);
}


void CodeGenerator::generateBlockStmt(const BlockStmt* stmt) {
    for (const auto& s : stmt->statements) {
        generateStmt(s.get());
    }
}


void CodeGenerator::generateExprStmt(const ExprStmt* stmt) {
    generateExpr(stmt->expression.get());
}

} // namespace pawc
