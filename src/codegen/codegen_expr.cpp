// Expression code generation
#include "codegen.h"
#include <iostream>

namespace pawc {

llvm::Value* CodeGenerator::generateExpr(const Expr* expr) {
    switch (expr->kind) {
        case Expr::Kind::Integer: {
            auto int_expr = static_cast<const IntegerExpr*>(expr);
            return llvm::ConstantInt::get(*context_, llvm::APInt(32, int_expr->value, true));
        }
        case Expr::Kind::Float: {
            auto float_expr = static_cast<const FloatExpr*>(expr);
            return llvm::ConstantFP::get(*context_, llvm::APFloat(float_expr->value));
        }
        case Expr::Kind::Boolean: {
            auto bool_expr = static_cast<const BooleanExpr*>(expr);
            return llvm::ConstantInt::get(*context_, llvm::APInt(1, bool_expr->value ? 1 : 0));
        }
        case Expr::Kind::String: {
            auto str_expr = static_cast<const StringExpr*>(expr);
            // Create global string and return pointer
            return builder_->CreateGlobalStringPtr(str_expr->value, "str");
        }
        case Expr::Kind::Identifier:
            return generateIdentifierExpr(static_cast<const IdentifierExpr*>(expr));
        case Expr::Kind::Binary:
            return generateBinaryExpr(static_cast<const BinaryExpr*>(expr));
        case Expr::Kind::Unary:
            return generateUnaryExpr(static_cast<const UnaryExpr*>(expr));
        case Expr::Kind::Call:
            return generateCallExpr(static_cast<const CallExpr*>(expr));
        case Expr::Kind::Assign:
            return generateAssignExpr(static_cast<const AssignExpr*>(expr));
        case Expr::Kind::ArrayLiteral:
            return generateArrayLiteralExpr(static_cast<const ArrayLiteralExpr*>(expr));
        case Expr::Kind::Index:
            return generateIndexExpr(static_cast<const IndexExpr*>(expr));
        case Expr::Kind::MemberAccess:
            return generateMemberAccessExpr(static_cast<const MemberAccessExpr*>(expr));
        case Expr::Kind::StructLiteral:
            return generateStructLiteralExpr(static_cast<const StructLiteralExpr*>(expr));
        case Expr::Kind::EnumVariant:
            return generateEnumVariantExpr(static_cast<const EnumVariantExpr*>(expr));
        case Expr::Kind::Match:
            return generateMatchExpr(static_cast<const MatchExpr*>(expr));
        case Expr::Kind::Is:
            return generateIsExpr(static_cast<const IsExpr*>(expr));
        case Expr::Kind::IfExpr:
            return generateIfExpr(static_cast<const IfExpr*>(expr));
        case Expr::Kind::Try:
            return generateTryExpr(static_cast<const TryExpr*>(expr));
        case Expr::Kind::Ok:
            return generateOkExpr(static_cast<const OkExpr*>(expr));
        case Expr::Kind::Err:
            return generateErrExpr(static_cast<const ErrExpr*>(expr));
        case Expr::Kind::Cast:
            return generateCastExpr(static_cast<const CastExpr*>(expr));
        default:
            return nullptr;
    }
}


llvm::Value* CodeGenerator::generateBinaryExpr(const BinaryExpr* expr) {
    llvm::Value* left = generateExpr(expr->left.get());
    llvm::Value* right = generateExpr(expr->right.get());
    
    if (!left || !right) return nullptr;
    
    // Check if it's a string operation
    bool is_ptr_left = left->getType()->isPointerTy();
    bool is_ptr_right = right->getType()->isPointerTy();
    
    if (expr->op == BinaryExpr::Op::Add && is_ptr_left && is_ptr_right) {
        // String concatenation: s1 + s2
        llvm::Function* strlen_func = module_->getFunction("strlen");
        llvm::Function* malloc_func = module_->getFunction("malloc");
        llvm::Function* strcpy_func = module_->getFunction("strcpy");
        llvm::Function* strcat_func = module_->getFunction("strcat");
        
        // Calculate length
        llvm::Value* len1 = builder_->CreateCall(strlen_func, {left}, "len1");
        llvm::Value* len2 = builder_->CreateCall(strlen_func, {right}, "len2");
        llvm::Value* total_len = builder_->CreateAdd(len1, len2, "total_len");
        llvm::Value* alloc_size = builder_->CreateAdd(
            total_len,
            llvm::ConstantInt::get(*context_, llvm::APInt(64, 1)),  // +1 for null
            "alloc_size"
        );
        
        // Allocate memory
        llvm::Value* result = builder_->CreateCall(malloc_func, {alloc_size}, "str_result");
        
        // Copy and concatenate
        builder_->CreateCall(strcpy_func, {result, left});
        builder_->CreateCall(strcat_func, {result, right});
        
        return result;
    }
    
    // Type matching check: integer types need unified bit width
    if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
        unsigned left_bits = left->getType()->getIntegerBitWidth();
        unsigned right_bits = right->getType()->getIntegerBitWidth();
        
        if (left_bits != right_bits) {
            // Promote to larger type
            if (left_bits < right_bits) {
                left = builder_->CreateSExt(left, right->getType(), "promote_left");
            } else {
                right = builder_->CreateSExt(right, left->getType(), "promote_right");
            }
        }
    }
    
    switch (expr->op) {
        case BinaryExpr::Op::Add: return builder_->CreateAdd(left, right, "addtmp");
        case BinaryExpr::Op::Sub: return builder_->CreateSub(left, right, "subtmp");
        case BinaryExpr::Op::Mul: return builder_->CreateMul(left, right, "multmp");
        case BinaryExpr::Op::Div: return builder_->CreateSDiv(left, right, "divtmp");
        case BinaryExpr::Op::Mod: return builder_->CreateSRem(left, right, "modtmp");
        case BinaryExpr::Op::Eq: return builder_->CreateICmpEQ(left, right, "eqtmp");
        case BinaryExpr::Op::Ne: return builder_->CreateICmpNE(left, right, "netmp");
        case BinaryExpr::Op::Lt: return builder_->CreateICmpSLT(left, right, "lttmp");
        case BinaryExpr::Op::Le: return builder_->CreateICmpSLE(left, right, "letmp");
        case BinaryExpr::Op::Gt: return builder_->CreateICmpSGT(left, right, "gttmp");
        case BinaryExpr::Op::Ge: return builder_->CreateICmpSGE(left, right, "getmp");
        case BinaryExpr::Op::And: return builder_->CreateAnd(left, right, "andtmp");
        case BinaryExpr::Op::Or: return builder_->CreateOr(left, right, "ortmp");
        default: return nullptr;
    }
}


llvm::Value* CodeGenerator::generateUnaryExpr(const UnaryExpr* expr) {
    llvm::Value* operand = generateExpr(expr->operand.get());
    if (!operand) return nullptr;
    
    switch (expr->op) {
        case UnaryExpr::Op::Neg:
            return builder_->CreateNeg(operand, "negtmp");
        case UnaryExpr::Op::Not:
            return builder_->CreateNot(operand, "nottmp");
        default:
            return nullptr;
    }
}


llvm::Value* CodeGenerator::generateCallExpr(const CallExpr* expr) {
    // Check if it's a method call: obj.method()
    if (expr->callee->kind == Expr::Kind::MemberAccess) {
        const MemberAccessExpr* member_expr = static_cast<const MemberAccessExpr*>(expr->callee.get());
        
        // Get object address (not value)
        llvm::Value* obj_ptr = nullptr;
        std::string obj_name;
        
        if (member_expr->object->kind == Expr::Kind::Identifier) {
            // Get pointer directly from named_values_ (alloca)
            obj_name = static_cast<const IdentifierExpr*>(member_expr->object.get())->name;
            auto it = named_values_.find(obj_name);
            if (it != named_values_.end()) {
                obj_ptr = it->second;  // 这已经是alloca指针
            }
        } else {
            // 其他情况（如临时值），需要生成表达式
            obj_ptr = generateExpr(member_expr->object.get());
        }
        
        if (!obj_ptr) return nullptr;
        
        // 查找方法 - 尝试所有struct
        for (const auto& [struct_name, methods] : struct_methods_) {
            auto method_it = methods.find(member_expr->member);
            if (method_it != methods.end()) {
                llvm::Function* method_func = method_it->second;
                
                // 构建参数列表：第一个参数是this指针
                std::vector<llvm::Value*> args;
                
                // 【关键修复】：在新的struct语义下，struct变量是alloca ptr
                // obj_ptr是alloca的地址，需要load获取实际的heap指针
                llvm::Value* actual_obj_ptr = obj_ptr;
                
                // 如果有obj_name并且在variable_types_中是StructType
                if (!obj_name.empty()) {
                    auto type_it = variable_types_.find(obj_name);
                    if (type_it != variable_types_.end() && type_it->second->isStructTy()) {
                        // obj_ptr是alloca ptr，load获取heap指针
                        actual_obj_ptr = builder_->CreateLoad(
                            llvm::PointerType::get(*context_, 0), 
                            obj_ptr, 
                            obj_name + "_heap_ptr"
                        );
                    }
                }
                
                args.push_back(actual_obj_ptr);  // this指针（heap指针）
                
                for (const auto& arg : expr->arguments) {
                    llvm::Value* arg_val = generateExpr(arg.get());
                    if (arg_val) args.push_back(arg_val);
                }
                
                // Call method
                if (method_func->getReturnType()->isVoidTy()) {
                    return builder_->CreateCall(method_func, args);
                }
                return builder_->CreateCall(method_func, args, "methodcall");
            }
        }
        
        std::cerr << "Method not found: " << member_expr->member << std::endl;
        return nullptr;
    }
    
    // 普通函数调用或关联函数
    if (expr->callee->kind != Expr::Kind::Identifier) {
        std::cerr << "Can only call functions by name" << std::endl;
        return nullptr;
    }
    
    auto callee_name = static_cast<const IdentifierExpr*>(expr->callee.get())->name;
    
    // 【新增】检查是否是泛型struct静态方法调用：Pair::new<K,V>()
    if (!expr->module_prefix.empty() && !expr->type_arguments.empty()) {
        // 检查module_prefix是否是泛型struct
        auto generic_struct_it = generic_structs_.find(expr->module_prefix);
        const StructStmt* generic_struct = nullptr;
        
        if (generic_struct_it != generic_structs_.end()) {
            generic_struct = generic_struct_it->second;
        } else if (symbol_table_) {
            // 跨模块查找泛型struct
            auto symbol = symbol_table_->lookup(expr->module_prefix, module_name_);
            if (symbol && symbol->kind == SymbolTable::SymbolKind::Type && symbol->ast_node) {
                const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
                if (struct_def && !struct_def->generic_params.empty()) {
                    generic_struct = struct_def;
                    generic_structs_[expr->module_prefix] = generic_struct;  // 缓存
                }
            }
        }
        
        if (generic_struct) {
            // 确认是泛型struct，实例化struct
            std::string struct_mangled = mangleGenericName(expr->module_prefix, expr->type_arguments);
            
            // 实例化struct（如果尚未实例化）
            llvm::Type* struct_type = instantiateGenericStruct(expr->module_prefix, expr->type_arguments);
            if (!struct_type) {
                std::cerr << "Failed to instantiate generic struct: " << expr->module_prefix << std::endl;
                return nullptr;
            }
            
            // 构造方法的mangled name
            // 例如：Pair::new + <i32, i32> -> new_i32_i32
            std::string method_mangled = callee_name + "_" + 
                struct_mangled.substr(expr->module_prefix.length() + 1);
            
            // 查找实例化的方法
            auto func_it = functions_.find(method_mangled);
            if (func_it == functions_.end()) {
                std::cerr << "Static method not found: " << expr->module_prefix 
                         << "::" << callee_name << std::endl;
                return nullptr;
            }
            
            llvm::Function* method_func = func_it->second;
            
            // Generate arguments
            std::vector<llvm::Value*> args;
            for (const auto& arg : expr->arguments) {
                llvm::Value* arg_val = generateExpr(arg.get());
                if (arg_val) args.push_back(arg_val);
            }
            
            // 调用静态方法
            if (method_func->getReturnType()->isVoidTy()) {
                return builder_->CreateCall(method_func, args);
            }
            return builder_->CreateCall(method_func, args, "static_method_call");
        }
    }
    
    // Check if it's跨模块调用 module::function()
    if (!expr->module_prefix.empty() && symbol_table_) {
        // Check if it's泛型调用
        if (!expr->type_arguments.empty()) {
            // 跨模块泛型调用：从符号表获取AST并在当前模块实例化
            auto symbol = symbol_table_->lookupInModule(expr->module_prefix, callee_name);
            if (!symbol) {
                std::cerr << "Function not found in module " << expr->module_prefix 
                         << ": " << callee_name << std::endl;
                return nullptr;
            }
            
            // Check if it's泛型函数
            if (symbol->kind != SymbolTable::SymbolKind::GenericFunction) {
                std::cerr << "Function " << callee_name << " is not a generic function" << std::endl;
                return nullptr;
            }
            
            // 从符号表获取AST定义
            const FunctionStmt* generic_func_ast = static_cast<const FunctionStmt*>(symbol->ast_node);
            if (!generic_func_ast) {
                std::cerr << "Generic function AST not found: " << callee_name << std::endl;
                return nullptr;
            }
            
            // 将泛型函数临时添加到当前CodeGenerator的generic_functions_
            generic_functions_[callee_name] = generic_func_ast;
            
            // 在当前模块实例化泛型函数
            llvm::Function* inst_func = instantiateGenericFunction(callee_name, expr->type_arguments);
            if (!inst_func) {
                std::cerr << "Failed to instantiate cross-module generic function: " << callee_name << std::endl;
                return nullptr;
            }
            
            // Generate arguments（使用辅助函数处理数组传递）
            std::vector<llvm::Value*> args;
            for (const auto& arg : expr->arguments) {
                llvm::Value* arg_val = generateArgumentValue(arg.get());
                if (arg_val) args.push_back(arg_val);
            }
            
            // 调用实例化的函数
            if (inst_func->getReturnType()->isVoidTy()) {
                return builder_->CreateCall(inst_func, args);
            }
            return builder_->CreateCall(inst_func, args, "cross_module_generic_call");
        }
        
        // 普通跨模块调用
        auto symbol = symbol_table_->lookupInModule(expr->module_prefix, callee_name);
        if (!symbol) {
            std::cerr << "Function not found in module " << expr->module_prefix 
                     << ": " << callee_name << std::endl;
            return nullptr;
        }
        
        if (!symbol_table_->isAccessible(*symbol, module_name_)) {
            std::cerr << "Function " << callee_name << " in module " 
                     << expr->module_prefix << " is not accessible" << std::endl;
            return nullptr;
        }
        
        // 重要：不能直接使用其他模块的llvm::Function*，因为它来自不同的Context
        // 需要在当前模块中声明这个函数
        llvm::Function* external_func_orig = llvm::cast<llvm::Function>(symbol->value);
        
        // 检查当前模块是否已有此函数声明
        llvm::Function* local_func = module_->getFunction(callee_name);
        if (!local_func) {
            // 在当前模块中创建函数声明（使用当前Context的类型）
            llvm::FunctionType* func_type = external_func_orig->getFunctionType();
            
            // 创建参数类型列表（使用当前Context）
            std::vector<llvm::Type*> param_types;
            for (unsigned i = 0; i < func_type->getNumParams(); i++) {
                llvm::Type* param_type = func_type->getParamType(i);
                // 转换类型到当前Context
                param_types.push_back(convertTypeToCurrentContext(param_type));
            }
            
            // 转换返回类型
            llvm::Type* return_type = convertTypeToCurrentContext(func_type->getReturnType());
            
            // 创建函数类型
            llvm::FunctionType* local_func_type = llvm::FunctionType::get(
                return_type, param_types, func_type->isVarArg()
            );
            
            // 创建函数声明（ExternalLinkage）
            local_func = llvm::Function::Create(
                local_func_type,
                llvm::Function::ExternalLinkage,
                callee_name,
                module_.get()
            );
        }
        
        // Generate arguments（使用辅助函数处理数组传递）
        std::vector<llvm::Value*> args;
        for (const auto& arg : expr->arguments) {
            llvm::Value* arg_val = generateArgumentValue(arg.get());
            if (arg_val) args.push_back(arg_val);
        }
        
        if (local_func->getReturnType()->isVoidTy()) {
            return builder_->CreateCall(local_func, args);
        }
        return builder_->CreateCall(local_func, args, "cross_module_call");
    }
    
    // Check if it's内置函数
    if (builtins_->isBuiltin(callee_name)) {
        return generateBuiltinCall(callee_name, expr->arguments);
    }
    
    // Check if it's泛型调用
    llvm::Function* callee = nullptr;
    if (!expr->type_arguments.empty()) {
        // 泛型函数调用：实例化
        callee = instantiateGenericFunction(callee_name, expr->type_arguments);
        if (!callee) {
            std::cerr << "Failed to instantiate generic function: " << callee_name << std::endl;
            return nullptr;
        }
    } else {
        // 查找用户定义的函数
        auto it = functions_.find(callee_name);
        if (it == functions_.end()) {
            // 可能是泛型函数但没有显式类型参数（暂不支持类型推导）
            if (isGenericFunction(callee_name)) {
                std::cerr << "Generic function requires explicit type arguments: " << callee_name << std::endl;
                return nullptr;
            }
            std::cerr << "Unknown function: " << callee_name << std::endl;
            return nullptr;
        }
        callee = it->second;
    }
    
    if (callee->arg_size() != expr->arguments.size()) {
        std::cerr << "Incorrect number of arguments" << std::endl;
        return nullptr;
    }
    
    std::vector<llvm::Value*> args;
    for (const auto& arg : expr->arguments) {
        llvm::Value* arg_val = generateArgumentValue(arg.get());
        if (!arg_val) return nullptr;
        args.push_back(arg_val);
    }
    
    // void函数不需要名字
    if (callee->getReturnType()->isVoidTy()) {
        return builder_->CreateCall(callee, args);
    }
    return builder_->CreateCall(callee, args, "calltmp");
}

// Helper function: generate function call arguments (handle array passing)

llvm::Value* CodeGenerator::generateArgumentValue(const Expr* arg) {
    // If argument is array variable, pass address not value
    if (arg->kind == Expr::Kind::Identifier) {
        std::string arg_name = static_cast<const IdentifierExpr*>(arg)->name;
        auto val_it = named_values_.find(arg_name);
        auto type_it = variable_types_.find(arg_name);
        
        if (val_it != named_values_.end() && type_it != variable_types_.end()) {
            // Check if it's数组类型
            if (llvm::isa<llvm::ArrayType>(type_it->second)) {
                // Array: pass alloca pointer directly
                return val_it->second;
            }
        }
    }
    
    // Non-array or other cases: generate normally
    return generateExpr(arg);
}

llvm::Value* CodeGenerator::generateBuiltinCall(const std::string& name, const std::vector<ExprPtr>& arguments) {

    llvm::Function* builtin_func = builtins_->getFunction(name);
    if (!builtin_func) {
        std::cerr << "Unknown builtin: " << name << std::endl;
        return nullptr;
    }
    
    // Check argument count
    if (builtin_func->arg_size() != arguments.size()) {
        std::cerr << "Incorrect number of arguments for " << name << std::endl;
        return nullptr;
    }
    
    // Generate arguments
    std::vector<llvm::Value*> args;
    for (const auto& arg : arguments) {
        args.push_back(generateExpr(arg.get()));
        if (!args.back()) return nullptr;
    }
    
    // Call built-in function
    if (builtin_func->getReturnType()->isVoidTy()) {
        return builder_->CreateCall(builtin_func, args);
    }
    return builder_->CreateCall(builtin_func, args, name + "_result");
}

llvm::Value* CodeGenerator::generateAssignExpr(const AssignExpr* expr) {
    llvm::Value* val = generateExpr(expr->value.get());
    if (!val) return nullptr;
    
    // 索引赋值：arr[i] = value 或 s[i] = value
    if (expr->target_expr && expr->target_expr->kind == Expr::Kind::Index) {
        const IndexExpr* index_expr = static_cast<const IndexExpr*>(expr->target_expr.get());
        
        // 生成索引值
        llvm::Value* index_val = generateExpr(index_expr->index.get());
        if (!index_val) return nullptr;
        
        // 获取数组/字符串的指针
        llvm::Value* array_ptr = nullptr;
        if (index_expr->array->kind == Expr::Kind::Identifier) {
            std::string array_name = static_cast<const IdentifierExpr*>(index_expr->array.get())->name;
            auto it = named_values_.find(array_name);
            if (it != named_values_.end()) {
                array_ptr = it->second;
            }
        }
        
        if (!array_ptr) {
            std::cerr << "Cannot access index of unknown array/string" << std::endl;
            return nullptr;
        }
        
        // 获取数组元素类型
        auto type_it = variable_types_.find(static_cast<const IdentifierExpr*>(index_expr->array.get())->name);
        if (type_it == variable_types_.end()) {
            std::cerr << "Unknown array/string type for index assignment" << std::endl;
            return nullptr;
        }
        
        llvm::Type* array_type = type_it->second;
        
        // Check if it's字符串类型（指针）
        if (array_type->isPointerTy()) {
            // 字符串索引写入：s[i] = 'A'
            llvm::Value* str_ptr = builder_->CreateLoad(
                llvm::PointerType::get(*context_, 0),
                array_ptr,
                "strload"
            );
            
            // GEP到指定字符
            llvm::Value* char_ptr = builder_->CreateGEP(
                llvm::Type::getInt8Ty(*context_),
                str_ptr,
                index_val,
                "stridx"
            );
            
            // 存储字符
            builder_->CreateStore(val, char_ptr);
            return val;
        } else if (array_type->isArrayTy()) {
            // 数组索引写入：arr[i] = value
            llvm::ArrayType* arr_type = llvm::cast<llvm::ArrayType>(array_type);
            
            // GEP到指定元素
            llvm::Value* elem_ptr = builder_->CreateGEP(
                arr_type,
                array_ptr,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0), index_val},
                "elem_ptr"
            );
            
            // 存储值
            builder_->CreateStore(val, elem_ptr);
            return val;
        } else {
            std::cerr << "Invalid type for index assignment" << std::endl;
            return nullptr;
        }
    }
    
    // 成员赋值：obj.field = value
    if (expr->target_expr && expr->target_expr->kind == Expr::Kind::MemberAccess) {
        const MemberAccessExpr* member_expr = static_cast<const MemberAccessExpr*>(expr->target_expr.get());
        
        // 获取对象指针
        llvm::Value* obj_ptr = nullptr;
        llvm::Type* struct_value_type = nullptr;
        
        if (member_expr->object->kind == Expr::Kind::Identifier) {
            std::string obj_name = static_cast<const IdentifierExpr*>(member_expr->object.get())->name;
            
            // 处理self
            if (obj_name == "self" && !current_struct_name_.empty()) {
                auto self_it = named_values_.find("self");
                if (self_it != named_values_.end()) {
                    llvm::Value* self_alloca = self_it->second;
                    obj_ptr = builder_->CreateLoad(
                        llvm::PointerType::get(*context_, 0),
                        self_alloca,
                        "self"
                    );
                    auto struct_type = getOrCreateStructType(current_struct_name_);
                    struct_value_type = struct_type;
                }
            } else {
                // 普通变量
                auto it = named_values_.find(obj_name);
                if (it != named_values_.end()) {
                    obj_ptr = it->second;
                    auto type_it = variable_types_.find(obj_name);
                    if (type_it != variable_types_.end()) {
                        struct_value_type = type_it->second;
                    }
                }
            }
        }
        
        if (!obj_ptr) {
            std::cerr << "Cannot access member of unknown object" << std::endl;
            return nullptr;
        }
        
        // 找到字段并生成GEP
        for (const auto& [struct_name, struct_def] : struct_defs_) {
            auto struct_type = getOrCreateStructType(struct_name);
            
            if (struct_value_type && struct_type != struct_value_type) {
                continue;
            }
            
            int field_idx = 0;
            for (const auto& field : struct_def->fields) {
                if (field.name == member_expr->member) {
                    llvm::Value* field_ptr = builder_->CreateStructGEP(
                        struct_type, obj_ptr, field_idx, "field_ptr"
                    );
                    builder_->CreateStore(val, field_ptr);
                    return val;
                }
                field_idx++;
            }
        }
        
        std::cerr << "Unknown field: " << member_expr->member << std::endl;
        return nullptr;
    }
    
    // 普通变量赋值：x = value
    auto it = named_values_.find(expr->target);
    if (it != named_values_.end()) {
        builder_->CreateStore(val, it->second);
        return val;
    }
    
    std::cerr << "Unknown variable: " << expr->target << std::endl;
    return nullptr;
}

/**
 * 成员访问表达式
 * 支持：struct.field访问
 */
llvm::Value* CodeGenerator::generateMemberAccessExpr(const MemberAccessExpr* expr) {
    // 获取对象指针（必须是指针，用于GEP）
    llvm::Value* obj_ptr = nullptr;
    llvm::Type* struct_value_type = nullptr;
    
    // Check if it's标识符（变量名）
    if (expr->object->kind == Expr::Kind::Identifier) {
        std::string obj_name = static_cast<const IdentifierExpr*>(expr->object.get())->name;
        
        // 如果是self，并且在方法中
        if (obj_name == "self" && !current_struct_name_.empty()) {
            auto self_it = named_values_.find("self");
            if (self_it != named_values_.end()) {
                llvm::Value* self_alloca = self_it->second;
                
                // self_alloca存储的是指针，需要load
                obj_ptr = builder_->CreateLoad(
                    llvm::PointerType::get(*context_, 0),
                    self_alloca,
                    "self"
                );
                
                // 获取struct类型
                auto struct_type = getOrCreateStructType(current_struct_name_);
                struct_value_type = struct_type;
            }
        } else {
            // 普通变量：obj_ptr应该是alloca（指向struct值的指针）
            auto it = named_values_.find(obj_name);
            if (it != named_values_.end()) {
                // 从variable_types_获取实际的struct值类型
                auto type_it = variable_types_.find(obj_name);
                if (type_it != variable_types_.end()) {
                    struct_value_type = type_it->second;
                    
                    // 【新逻辑】：处理不同的类型
                    if (struct_value_type->isPointerTy()) {
                        // struct变量现在存储ptr：需要load得到实际的struct指针
                        obj_ptr = builder_->CreateLoad(
                            llvm::PointerType::get(*context_, 0),
                            it->second,
                            obj_name + "_ptr"
                        );
                        // 注意：variable_types_可能存储的是StructType（参数），需要保留
                    } else if (struct_value_type->isStructTy()) {
                        // struct参数传递：variable_types_存储StructType，alloca存储ptr
                        // 需要load得到实际struct指针
                        obj_ptr = builder_->CreateLoad(
                            llvm::PointerType::get(*context_, 0),
                            it->second,
                            obj_name + "_loaded"
                        );
                    } else {
                        // 非指针（旧语义）：直接使用alloca
                        obj_ptr = it->second;
                    }
                } else {
                    obj_ptr = it->second;  // fallback
                }
            }
        }
    } else {
        // 复杂表达式：生成表达式
        llvm::Value* obj = generateExpr(expr->object.get());
        if (!obj) return nullptr;
        
        // 如果是指针，直接使用；如果是值，需要创建临时alloca
        if (obj->getType()->isPointerTy()) {
            obj_ptr = obj;
        } else if (obj->getType()->isStructTy()) {
            // 创建临时alloca存储struct值
            llvm::AllocaInst* temp = builder_->CreateAlloca(obj->getType(), nullptr, "temp_struct");
            builder_->CreateStore(obj, temp);
            obj_ptr = temp;
            struct_value_type = obj->getType();
        } else {
            return nullptr;
        }
    }
    
    if (!obj_ptr) return nullptr;
    
    // 尝试所有struct定义，找到匹配的字段
    // 【优先使用精确类型】：如果struct_value_type是StructType，直接使用它
    if (struct_value_type && struct_value_type->isStructTy()) {
        // 直接使用精确的struct类型
        llvm::StructType* exact_struct = llvm::cast<llvm::StructType>(struct_value_type);
        
        // 找到对应的struct定义
        for (const auto& [struct_name, struct_def] : struct_defs_) {
            auto struct_type = getOrCreateStructType(struct_name);
            if (struct_type == exact_struct) {
                // 找到匹配的定义
                int field_idx = 0;
                for (const auto& field : struct_def->fields) {
                    if (field.name == expr->member) {
                        // 找到字段！
                        llvm::Value* field_ptr = builder_->CreateStructGEP(
                            struct_type, obj_ptr, field_idx, "field_ptr"
                        );
                        
                        // 加载字段值
                        llvm::Type* field_type = convertType(field.type.get());
                        return builder_->CreateLoad(field_type, field_ptr, expr->member);
                    }
                    field_idx++;
                }
            }
        }
    }
    
    // 如果精确类型匹配失败，尝试所有struct定义（兜底）
    for (const auto& [struct_name, struct_def] : struct_defs_) {
        auto struct_type = getOrCreateStructType(struct_name);
        
        int field_idx = 0;
        for (const auto& field : struct_def->fields) {
            if (field.name == expr->member) {
                llvm::Value* field_ptr = builder_->CreateStructGEP(
                    struct_type, obj_ptr, field_idx, "field_ptr"
                );
                llvm::Type* field_type = convertType(field.type.get());
                return builder_->CreateLoad(field_type, field_ptr, expr->member);
            }
            field_idx++;
        }
    }
    
    std::cerr << "Unknown field: " << expr->member << std::endl;
    return nullptr;
}

llvm::Value* CodeGenerator::generateIndexExpr(const IndexExpr* expr) {
    // 获取数组/字符串变量名和类型
    llvm::Value* array_ptr = nullptr;
    llvm::Type* array_type = nullptr;
    
    if (expr->array->kind == Expr::Kind::Identifier) {
        std::string array_name = static_cast<const IdentifierExpr*>(expr->array.get())->name;
        auto ptr_it = named_values_.find(array_name);
        auto type_it = variable_types_.find(array_name);
        
        if (ptr_it != named_values_.end() && type_it != variable_types_.end()) {
            array_ptr = ptr_it->second;   // alloca指针
            array_type = type_it->second; // 数组/字符串类型
            
            // Check if it's数组参数（在array_element_types_中有记录）
            auto elem_it = array_element_types_.find(array_name);
            if (elem_it != array_element_types_.end()) {
                // 这是数组参数：arr[i] -> T
                llvm::Type* elem_type = elem_it->second;
                llvm::Value* index_val = generateExpr(expr->index.get());
                if (!index_val) return nullptr;
                
                // 加载数组指针
                llvm::Value* arr_ptr = builder_->CreateLoad(
                    llvm::PointerType::get(*context_, 0),
                    array_ptr,
                    "arrload"
                );
                
                // GEP到指定元素
                llvm::Value* elem_ptr = builder_->CreateGEP(
                    elem_type,
                    arr_ptr,
                    index_val,
                    "elemptr"
                );
                
                // 加载元素
                return builder_->CreateLoad(elem_type, elem_ptr, "elemload");
            }
            
            // Check if it's字符串类型
            if (array_type->isPointerTy() && !array_type->isArrayTy()) {
                // 这是字符串：s[i] -> char
                llvm::Value* index_val = generateExpr(expr->index.get());
                if (!index_val) return nullptr;
                
                // 加载字符串指针
                llvm::Value* str_ptr = builder_->CreateLoad(
                    llvm::PointerType::get(*context_, 0),
                    array_ptr,
                    "strload"
                );
                
                // GEP到指定字符
                llvm::Value* char_ptr = builder_->CreateGEP(
                    llvm::Type::getInt8Ty(*context_),
                    str_ptr,
                    index_val,
                    "stridx"
                );
                
                // 加载字符
                return builder_->CreateLoad(
                    llvm::Type::getInt8Ty(*context_),
                    char_ptr,
                    "charload"
                );
            }
        }
    } else if (expr->array->kind == Expr::Kind::Index) {
        // 多维数组：递归处理但返回指针
        const IndexExpr* inner = static_cast<const IndexExpr*>(expr->array.get());
        
        // 获取基础数组
        if (inner->array->kind != Expr::Kind::Identifier) {
            std::cerr << "Nested index only supports identifiers" << std::endl;
            return nullptr;
        }
        
        std::string base_name = static_cast<const IdentifierExpr*>(inner->array.get())->name;
        auto ptr_it = named_values_.find(base_name);
        auto type_it = variable_types_.find(base_name);
        
        if (ptr_it == named_values_.end() || type_it == variable_types_.end()) {
            std::cerr << "Unknown array: " << base_name << std::endl;
            return nullptr;
        }
        
        llvm::Value* base_ptr = ptr_it->second;
        llvm::Type* base_type = type_it->second;
        
        if (!llvm::isa<llvm::ArrayType>(base_type)) {
            std::cerr << "Base is not an array" << std::endl;
            return nullptr;
        }
        
        // 第一级索引
        llvm::Value* first_idx = generateExpr(inner->index.get());
        if (!first_idx) return nullptr;
        
        // GEP到第一级元素（指针）
        llvm::Value* first_ptr = builder_->CreateInBoundsGEP(
            base_type, base_ptr,
            {llvm::ConstantInt::get(*context_, llvm::APInt(64, 0)), first_idx},
            "first_ptr"
        );
        
        // 元素类型
        llvm::Type* first_elem = llvm::cast<llvm::ArrayType>(base_type)->getElementType();
        
        array_ptr = first_ptr;
        array_type = first_elem;
        
    } else {
        std::cerr << "Unsupported array expression" << std::endl;
        return nullptr;
    }
    
    if (!array_ptr || !array_type) return nullptr;
    
    llvm::Value* index = generateExpr(expr->index.get());
    if (!index) return nullptr;
    
    // 获取元素类型
    llvm::Type* elem_type = nullptr;
    if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(array_type)) {
        elem_type = arr_ty->getElementType();
        
        // 计算GEP（数组索引）
        llvm::Value* elem_ptr = builder_->CreateGEP(
            array_type, array_ptr,
            {llvm::ConstantInt::get(*context_, llvm::APInt(64, 0)), index},
            "elemptr"
        );
        
        return builder_->CreateLoad(elem_type, elem_ptr, "elemval");
    }
    
    return nullptr;
}

llvm::Value* CodeGenerator::generateIdentifierExpr(const IdentifierExpr* expr) {
    auto it = named_values_.find(expr->name);
    if (it != named_values_.end()) {
        // 获取变量类型
        llvm::Type* var_type = nullptr;
        auto type_it = variable_types_.find(expr->name);
        if (type_it != variable_types_.end()) {
            var_type = type_it->second;
        }
        
        // 特殊处理self：在方法中，self存储为alloca(ptr)，load得到指向struct的指针
        if (expr->name == "self" && !current_struct_name_.empty()) {
            return builder_->CreateLoad(llvm::PointerType::get(*context_, 0), it->second, "self");
        }
        
        // 数组类型返回指针（alloca）
        if (var_type && var_type->isArrayTy()) {
            return it->second;  // 返回数组的指针
        }
        
        // struct类型：需要区分两种情况
        // 1. 本地struct变量：alloca ptr，存储heap指针，需要load
        // 2. 函数参数struct：alloca ptr，存储heap指针，需要load
        // 【重要】：在新语义下，所有struct都是alloca ptr，需要load来获取实际heap指针
        if (var_type && var_type->isStructTy()) {
            // 检查alloca的类型：如果是ptr，说明需要load
            llvm::Type* alloca_type = it->second->getType();
            if (alloca_type->isPointerTy()) {
                // alloca ptr -> load -> heap指针
                return builder_->CreateLoad(llvm::PointerType::get(*context_, 0), it->second, expr->name);
            } else {
                // 旧逻辑：alloca struct -> 返回指针
                return it->second;
            }
        }
        
        // 加载其他类型的值
        if (var_type) {
            return builder_->CreateLoad(var_type, it->second, expr->name);
        }
        
        return builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), it->second, expr->name);
    }
    
    std::cerr << "Unknown variable: " << expr->name << std::endl;
    return nullptr;
}

/**
 * If表达式生成
 * 语法: if condition { then_expr } else { else_expr }
 * 使用LLVM的select指令或PHI节点
 */
llvm::Value* CodeGenerator::generateIfExpr(const IfExpr* expr) {

    // 生成条件
    llvm::Value* cond = generateExpr(expr->condition.get());
    if (!cond) return nullptr;
    
    // 创建基本块
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context_, "if_then", func);
    llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*context_, "if_else", func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "if_merge", func);
    
    // 条件分支
    builder_->CreateCondBr(cond, then_bb, else_bb);
    
    // Then分支
    builder_->SetInsertPoint(then_bb);
    llvm::Value* then_val = generateExpr(expr->then_expr.get());
    if (!then_val) return nullptr;
    builder_->CreateBr(merge_bb);
    then_bb = builder_->GetInsertBlock();  // 更新（可能被表达式修改）
    
    // Else分支
    builder_->SetInsertPoint(else_bb);
    llvm::Value* else_val = generateExpr(expr->else_expr.get());
    if (!else_val) return nullptr;
    builder_->CreateBr(merge_bb);
    else_bb = builder_->GetInsertBlock();  // 更新（可能被表达式修改）
    
    // Merge分支（使用PHI节点）
    builder_->SetInsertPoint(merge_bb);
    
    // 确保两个分支的类型一致
    llvm::Type* then_type = then_val->getType();
    llvm::Type* else_type = else_val->getType();
    
    if (then_type != else_type) {
        // 类型不匹配时报错
        std::cerr << "Error: if expression branches must have the same type" << std::endl;
        return nullptr;
    }
    
    llvm::PHINode* phi = builder_->CreatePHI(then_type, 2, "if_result");
    phi->addIncoming(then_val, then_bb);
    phi->addIncoming(else_val, else_bb);
    
    return phi;
}

/**
 * Cast表达式生成（类型转换）
 * 语法: expr as target_type
 */
llvm::Value* CodeGenerator::generateCastExpr(const CastExpr* expr) {

    llvm::Value* val = generateExpr(expr->expression.get());
    if (!val) return nullptr;
    
    llvm::Type* target_type = resolveGenericType(expr->target_type.get());
    if (!target_type) return nullptr;
    
    llvm::Type* source_type = val->getType();
    
    // 如果类型相同，直接返回
    if (source_type == target_type) {
        return val;
    }
    
    // 整数 → 整数（扩展或截断）
    if (source_type->isIntegerTy() && target_type->isIntegerTy()) {
        unsigned src_bits = source_type->getIntegerBitWidth();
        unsigned tgt_bits = target_type->getIntegerBitWidth();
        
        if (src_bits < tgt_bits) {
            return builder_->CreateSExt(val, target_type, "sext");
        } else if (src_bits > tgt_bits) {
            return builder_->CreateTrunc(val, target_type, "trunc");
        }
        return val;
    }
    
    // 整数 → 浮点
    if (source_type->isIntegerTy() && target_type->isFloatingPointTy()) {
        return builder_->CreateSIToFP(val, target_type, "sitofp");
    }
    
    // 浮点 → 整数
    if (source_type->isFloatingPointTy() && target_type->isIntegerTy()) {
        return builder_->CreateFPToSI(val, target_type, "fptosi");
    }
    
    // 浮点 → 浮点
    if (source_type->isFloatingPointTy() && target_type->isFloatingPointTy()) {
        unsigned src_bits = source_type->getScalarSizeInBits();
        unsigned tgt_bits = target_type->getScalarSizeInBits();
        
        if (src_bits < tgt_bits) {
            return builder_->CreateFPExt(val, target_type, "fpext");
        } else if (src_bits > tgt_bits) {
            return builder_->CreateFPTrunc(val, target_type, "fptrunc");
        }
        return val;
    }
    
    return val;
}

/**
 * 创建Optional类型结构: {i32 tag, T value, ptr error_msg}
 * tag: 0 = Value, 1 = Error
 */
llvm::StructType* CodeGenerator::createOptionalType(llvm::Type* value_type) {
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),           // tag
        value_type,                                   // value
        llvm::PointerType::get(*context_, 0)         // error_msg (i8*)
    };
    return llvm::StructType::get(*context_, fields);
}

/**
 * 确保T?类型有对应的enum定义，用于模式匹配
 * 创建虚拟的 enum { Value(T), Error(string) }
 */
void CodeGenerator::ensureOptionalEnumDef(llvm::Type* value_type, const std::string& type_name) {
    // 检查是否已经创建
    if (enum_defs_.find(type_name) != enum_defs_.end()) {
        return;
    }
    
    // 创建variants
    std::vector<EnumVariant> variants;
    
    // Value变体（泛型，关联值类型稍后处理）
    EnumVariant value_variant;
    value_variant.name = "Value";
    value_variant.location = SourceLocation();
    // 注意：不添加associated_types，模式匹配时会动态处理
    variants.push_back(std::move(value_variant));
    
    // Error变体（关联string）
    EnumVariant error_variant;
    error_variant.name = "Error";
    error_variant.location = SourceLocation();
    error_variant.associated_types.push_back(
        std::make_unique<PrimitiveTypeNode>(PrimitiveType::STRING, SourceLocation())
    );
    variants.push_back(std::move(error_variant));
    
    // 创建EnumStmt（使用构造函数）
    EnumStmt* optional_enum = new EnumStmt(
        type_name,
        std::vector<GenericParam>(),  // 无泛型参数
        std::move(variants),
        true,  // is_public
        SourceLocation()
    );
    
    // 注册到enum_defs_
    enum_defs_[type_name] = optional_enum;
}

/**
 * Try表达式生成: expr?
 * 如果expr是Error，立即返回Error；否则提取Value
 */
llvm::Value* CodeGenerator::generateTryExpr(const TryExpr* expr) {

    llvm::Value* val = generateExpr(expr->expression.get());
    if (!val) return nullptr;
    
    // 假设val是Optional类型: {i32 tag, T value, ptr error_msg}
    llvm::Type* optional_type = val->getType();
    if (!optional_type->isStructTy()) {
        std::cerr << "Error: ? operator can only be used on Optional types" << std::endl;
        return nullptr;
    }
    
    llvm::StructType* struct_type = llvm::cast<llvm::StructType>(optional_type);
    if (struct_type->getNumElements() != 3) {
        std::cerr << "Error: Invalid Optional type structure" << std::endl;
        return nullptr;
    }
    
    // 如果val是struct值（而不是指针），先存储到alloca
    llvm::Value* opt_ptr = val;
    if (!val->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder_->CreateAlloca(struct_type, nullptr, "opt_temp");
        builder_->CreateStore(val, temp);
        opt_ptr = temp;
    }
    
    // 创建基本块
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* value_bb = llvm::BasicBlock::Create(*context_, "try_value", func);
    llvm::BasicBlock* error_bb = llvm::BasicBlock::Create(*context_, "try_error", func);
    
    // 加载tag字段（索引0）
    llvm::Value* tag_ptr = builder_->CreateStructGEP(struct_type, opt_ptr, 0, "tag_ptr");
    llvm::Value* tag = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), tag_ptr, "tag");
    
    // 检查tag: 0=Value, 1=Error
    llvm::Value* is_error = builder_->CreateICmpEQ(
        tag,
        llvm::ConstantInt::get(*context_, llvm::APInt(32, 1)),
        "is_error"
    );
    builder_->CreateCondBr(is_error, error_bb, value_bb);
    
    // Error分支：返回Error
    builder_->SetInsertPoint(error_bb);
    llvm::Value* error_val = builder_->CreateLoad(struct_type, opt_ptr, "error_val");
    builder_->CreateRet(error_val);  // 返回Optional（包含错误）
    
    // Value分支：提取值
    builder_->SetInsertPoint(value_bb);
    llvm::Value* value_ptr = builder_->CreateStructGEP(struct_type, opt_ptr, 1, "value_ptr");
    llvm::Type* value_type = struct_type->getElementType(1);
    llvm::Value* extracted_value = builder_->CreateLoad(value_type, value_ptr, "extracted");
    
    return extracted_value;
}

/**
 * Ok表达式生成: ok(value)
 * 创建Optional类型的Value变体，返回struct值（不是指针）
 */
llvm::Value* CodeGenerator::generateOkExpr(const OkExpr* expr) {

    llvm::Value* val = generateExpr(expr->value.get());
    if (!val) return nullptr;
    
    // 创建Optional类型
    llvm::StructType* optional_type = createOptionalType(val->getType());
    
    // 为T?类型创建enum定义（用于模式匹配）
    // 使用类型的字符串表示作为名称
    std::string type_name = "Optional";  // 简化：所有T?使用相同的enum名称
    ensureOptionalEnumDef(val->getType(), type_name);
    
    // 1. 在栈上创建临时Optional
    llvm::AllocaInst* temp_result = builder_->CreateAlloca(optional_type, nullptr, "ok_temp");
    
    // 2. 初始化字段
    // 设置tag = 0 (Value)
    llvm::Value* tag_ptr = builder_->CreateStructGEP(optional_type, temp_result, 0, "tag_ptr");
    builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, 0)), tag_ptr);
    
    // 设置value
    llvm::Value* value_ptr = builder_->CreateStructGEP(optional_type, temp_result, 1, "value_ptr");
    builder_->CreateStore(val, value_ptr);
    
    // 设置error_msg = null
    llvm::Value* error_ptr = builder_->CreateStructGEP(optional_type, temp_result, 2, "error_ptr");
    builder_->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(*context_, 0)), error_ptr);
    
    // 3. 分配堆内存
    const llvm::DataLayout& data_layout = module_->getDataLayout();
    uint64_t optional_size = data_layout.getTypeAllocSize(optional_type);
    llvm::Value* size_val = llvm::ConstantInt::get(*context_, llvm::APInt(64, optional_size));
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        std::cerr << "malloc not found!" << std::endl;
        return nullptr;
    }
    
    llvm::Value* heap_ptr = builder_->CreateCall(malloc_func, {size_val}, "ok_heap");
    
    // 4. 拷贝到堆
    llvm::Function* memcpy_func = module_->getFunction("memcpy");
    if (memcpy_func) {
        builder_->CreateCall(memcpy_func, {heap_ptr, temp_result, size_val});
    }
    
    // 5. 返回堆指针（统一语义）
    return heap_ptr;
}

/**
 * Err表达式生成: err(message)
 * 创建Optional类型的Error变体，需要从函数返回类型推导T
 */
llvm::Value* CodeGenerator::generateErrExpr(const ErrExpr* expr) {

    llvm::Value* msg = generateExpr(expr->message.get());
    if (!msg) return nullptr;
    
    // 从函数返回类型推导T类型
    llvm::Type* value_type = llvm::Type::getInt32Ty(*context_);  // 默认i32
    
    if (current_function_return_type_) {
        // 如果返回类型是T?，提取T
        if (auto opt_type = dynamic_cast<const OptionalTypeNode*>(current_function_return_type_)) {
            value_type = convertType(opt_type->inner_type.get());
        } else {
            // 不是Optional类型，使用函数返回类型本身
            value_type = convertType(current_function_return_type_);
        }
    }
    
    // 创建Optional类型: {i32 tag, T value, ptr error_msg}
    llvm::StructType* optional_type = createOptionalType(value_type);
    
    // 为T?类型创建enum定义（用于模式匹配）
    std::string type_name = "Optional";
    ensureOptionalEnumDef(value_type, type_name);
    
    // 1. 在栈上创建临时Optional
    llvm::AllocaInst* temp_result = builder_->CreateAlloca(optional_type, nullptr, "err_temp");
    
    // 2. 初始化字段
    // 设置tag = 1 (Error)
    llvm::Value* tag_ptr = builder_->CreateStructGEP(optional_type, temp_result, 0, "tag_ptr");
    builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, 1)), tag_ptr);
    
    // 设置value = 0 (零值，类型匹配T)
    llvm::Value* value_ptr = builder_->CreateStructGEP(optional_type, temp_result, 1, "value_ptr");
    llvm::Value* zero_value = llvm::Constant::getNullValue(value_type);
    builder_->CreateStore(zero_value, value_ptr);
    
    // 设置error_msg
    llvm::Value* error_ptr = builder_->CreateStructGEP(optional_type, temp_result, 2, "error_ptr");
    builder_->CreateStore(msg, error_ptr);
    
    // 3. 分配堆内存
    const llvm::DataLayout& data_layout = module_->getDataLayout();
    uint64_t optional_size = data_layout.getTypeAllocSize(optional_type);
    llvm::Value* size_val = llvm::ConstantInt::get(*context_, llvm::APInt(64, optional_size));
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        std::cerr << "malloc not found!" << std::endl;
        return nullptr;
    }
    
    llvm::Value* heap_ptr = builder_->CreateCall(malloc_func, {size_val}, "err_heap");
    
    // 4. 拷贝到堆
    llvm::Function* memcpy_func = module_->getFunction("memcpy");
    if (memcpy_func) {
        builder_->CreateCall(memcpy_func, {heap_ptr, temp_result, size_val});
    }
    
    // 5. 返回堆指针（统一语义）
    return heap_ptr;
}

} // namespace pawc
