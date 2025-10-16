// Type conversion and generic instantiation
#include "codegen.h"
#include <iostream>

namespace pawc {

llvm::Type* CodeGenerator::convertType(const Type* type) {
    if (!type) {
        return llvm::Type::getVoidTy(*context_);
    }
    
    switch (type->kind) {
        case Type::Kind::Primitive: {
            auto prim_type = static_cast<const PrimitiveTypeNode*>(type);
            return convertPrimitiveType(prim_type->prim_type);
        }
        case Type::Kind::Named: {
            auto named_type = static_cast<const NamedTypeNode*>(type);
            
            // 检查是否有泛型参数
            if (!named_type->generic_args.empty()) {
                // 泛型struct: Box<i32>
                auto it = generic_structs_.find(named_type->name);
                if (it != generic_structs_.end()) {
                    return instantiateGenericStruct(named_type->name, named_type->generic_args);
                }
                // 泛型enum: Option<i32>
                auto enum_it = generic_enums_.find(named_type->name);
                if (enum_it != generic_enums_.end()) {
                    return instantiateGenericEnum(named_type->name, named_type->generic_args);
                }
            }
            
            // 先尝试查找enum类型（enum作为值类型，优先检查）
            auto enum_type = getEnumType(named_type->name);
            if (enum_type) {
                return enum_type;  // enum作为值类型 {i32, i64}
            }
            
            // 如果没找到enum，尝试从符号表导入（跨模块类型）
            if (symbol_table_ && !enum_type) {
                // 尝试从所有模块导入此类型
                auto symbol = symbol_table_->lookup(named_type->name, module_name_);
                if (symbol && symbol->kind == SymbolTable::SymbolKind::Type) {
                    importTypeFromModule(named_type->name, symbol->module);
                    // 重新尝试查找
                    enum_type = getEnumType(named_type->name);
                    if (enum_type) {
                        return enum_type;
                    }
                }
            }
            
            // 再尝试查找struct类型（struct作为指针传递）
            auto struct_type = getOrCreateStructType(named_type->name);
            if (struct_type) {
                return llvm::PointerType::get(*context_, 0);  // struct作为指针传递
            }
            
            // 默认按i32处理
            return llvm::Type::getInt32Ty(*context_);
        }
        case Type::Kind::Array: {
            auto array_type = static_cast<const ArrayTypeNode*>(type);
            llvm::Type* elem_type = convertType(array_type->element_type.get());
            
            // 处理大小待推导的情况（size == -1）
            // 注意：在变量声明时，size=-1应该从初始化器推导，由外层处理
            // 只有在函数参数时才返回PointerType
            if (array_type->size == -1 || array_type->size < 0) {
                // 待推导大小的数组：
                // - 如果是函数参数：返回PointerType
                // - 如果是变量声明：外层会从初始化器推导，这里返回i32作为占位符
                // 使用一个小的默认大小作为占位（外层会覆盖）
                return llvm::ArrayType::get(elem_type, 1);  // 占位符，外层会重新设置
            }
            
            return llvm::ArrayType::get(elem_type, array_type->size);
        }
        case Type::Kind::Generic: {
            // 泛型参数在单态化后应该被替换
            // 这里暂时返回i32
            return llvm::Type::getInt32Ty(*context_);
        }
        case Type::Kind::Optional: {
            // Optional类型: T?
            // 内部表示为 enum { Value(T), Error(string) }
            // 简化为 {i32 tag, T value, ptr error_msg}
            auto opt_type = static_cast<const OptionalTypeNode*>(type);
            
            // 使用resolveGenericType而不是convertType，这样T可以被正确解析
            llvm::Type* inner_type = resolveGenericType(opt_type->inner_type.get());
            
            // 使用struct表示: { i32 tag, T value, ptr error_msg }
            std::vector<llvm::Type*> fields = {
                llvm::Type::getInt32Ty(*context_),           // tag: 0=Value, 1=Error
                inner_type,                                   // value
                llvm::PointerType::get(*context_, 0)         // error_msg
            };
            return llvm::StructType::get(*context_, fields);
        }
        case Type::Kind::SelfType: {
            // Self类型：在struct方法中表示当前struct类型
            if (current_struct_name_.empty()) {
                std::cerr << "Error: 'Self' can only be used in struct methods" << std::endl;
                return llvm::Type::getInt32Ty(*context_);
            }
            
            auto struct_type = getOrCreateStructType(current_struct_name_);
            if (!struct_type) {
                return llvm::Type::getInt32Ty(*context_);
            }
            
            // 区分关联函数和实例方法：
            // - 关联函数（如new）：返回值类型（struct本身）
            // - 实例方法（有self）：返回指针类型
            if (current_is_method_) {
                // 实例方法：返回指针
                return llvm::PointerType::get(*context_, 0);
            } else {
                // 关联函数：返回值类型
                return struct_type;
            }
        }
        default:
            return llvm::Type::getVoidTy(*context_);
    }
}

llvm::StructType* CodeGenerator::getOrCreateStructType(const std::string& name) {

    // 1. 先查本地缓存
    auto it = struct_types_.find(name);
    if (it != struct_types_.end()) {
        return it->second;
    }
    
    // 2. 查找本地struct定义
    auto def_it = struct_defs_.find(name);
    if (def_it != struct_defs_.end()) {
        // 创建struct类型
        auto def = def_it->second;
        std::vector<llvm::Type*> field_types;
        for (const auto& field : def->fields) {
            field_types.push_back(convertType(field.type.get()));
        }
        
        auto struct_type = llvm::StructType::create(*context_, field_types, name);
        struct_types_[name] = struct_type;
        return struct_type;
    }
    
    // 3. 查找跨模块的泛型struct实例（通过SymbolTable）
    if (symbol_table_) {
        // 使用通用lookup查找跨模块类型
        auto symbol = symbol_table_->lookup(name, module_name_);
        if (symbol && symbol->kind == SymbolTable::SymbolKind::Type && symbol->type) {
            // 找到了！从其他模块导入类型
            importTypeFromModule(name, symbol->module);
            
            // 再次检查是否成功导入
            auto imported_it = struct_types_.find(name);
            if (imported_it != struct_types_.end()) {
                return imported_it->second;
            }
        }
    }
    
    return nullptr;
}

    switch (type) {

        case PrimitiveType::VOID: return llvm::Type::getVoidTy(*context_);
        default: return llvm::Type::getVoidTy(*context_);
    }
}

// ============================================================================
// 第3部分：表达式生成（Expression Generation）
// ============================================================================

/**
 * 表达式生成入口
 * 根据表达式类型分发到具体的生成函数
 */
    // 如果是泛型函数，保存定义
    if (!stmt->generic_params.empty()) {
        generic_functions_[stmt->name] = stmt;
        
        // 如果是pub泛型函数，注册到符号表（保存AST定义）
        // 这样其他模块可以访问泛型函数的AST并实例化
        if (stmt->is_public && symbol_table_) {
            symbol_table_->registerGenericFunction(module_name_, stmt->name, stmt->is_public, stmt);
        }
        
        return;
    }
    
    // 设置当前是否为实例方法（用于Self类型解析）
    current_is_method_ = stmt->is_method;
    
    std::vector<llvm::Type*> param_types;

                
                llvm::Value* field_ptr = builder_->CreateStructGEP(struct_type, temp_alloca, i);
                builder_->CreateStore(field_val, field_ptr);
            }
        }
    }
    
    // 3. 分配堆内存
    const llvm::DataLayout& data_layout = module_->getDataLayout();
    uint64_t struct_size = data_layout.getTypeAllocSize(struct_type);
    llvm::Value* size_val = llvm::ConstantInt::get(*context_, llvm::APInt(64, struct_size));
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        std::cerr << "malloc not found!" << std::endl;
        return nullptr;
    }
    
    llvm::Value* heap_ptr = builder_->CreateCall(malloc_func, {size_val}, "struct_heap");
    
    // 4. 拷贝struct到堆
    llvm::Function* memcpy_func = module_->getFunction("memcpy");
    if (memcpy_func) {

    }
    
    // 5. 返回堆指针
    return heap_ptr;
}

llvm::Value* CodeGenerator::generateEnumVariantExpr(const EnumVariantExpr* expr) {
    // 尝试查找enum类型（可能是泛型实例化的mangled name）
    llvm::Type* enum_type = getEnumType(expr->enum_name);
    
    // 如果直接找不到，可能是泛型enum，需要从enum_defs_查找
    std::string enum_name = expr->enum_name;
    if (!enum_type) {
        // 检查enum_defs_中是否有这个类型
        auto it = enum_defs_.find(expr->enum_name);
        if (it != enum_defs_.end()) {
            // 找到了，构造类型
            std::vector<llvm::Type*> fields = {
                llvm::Type::getInt32Ty(*context_),  // tag
                llvm::Type::getInt64Ty(*context_)   // data
            };
            enum_type = llvm::StructType::get(*context_, fields);
        } else {

            return nullptr;
        }
    }
    
    // 分配enum实例
    llvm::Value* alloca = builder_->CreateAlloca(enum_type);
    
    // 设置tag（variant索引）
    auto def_it = enum_defs_.find(expr->enum_name);
    if (def_it != enum_defs_.end()) {
            }
            
            param_types.push_back(param_type);
        }
    }
    
    llvm::Type* return_type = generic_func->return_type ?
        resolveGenericType(generic_func->return_type.get()) :
        llvm::Type::getVoidTy(*context_);
    
    // 【新增】：对于struct返回值，使用指针类型
    if (return_type && llvm::isa<llvm::StructType>(return_type)) {
        return_type = llvm::PointerType::get(*context_, 0);
    }
    
    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        mangled_name,
        module_.get()
    );
    
    functions_[mangled_name] = func;
    
    // 预实例化返回类型中的泛型struct（如果有）
    if (generic_func->return_type && generic_func->return_type->kind == Type::Kind::Named) {
        const NamedTypeNode* named_ret = static_cast<const NamedTypeNode*>(generic_func->return_type.get());
        if (!named_ret->generic_args.empty()) {
            // 返回类型是泛型struct实例（如Pair<K, V>或Pair<V, K>）
            // 需要将泛型参数映射到具体类型
            
            std::vector<const Type*> concrete_args_raw;
            for (const auto& arg : named_ret->generic_args) {
                if (arg->kind == Type::Kind::Generic) {
                    // 找到这个泛型参数在函数generic_params中的索引
                    const GenericTypeNode* gen = static_cast<const GenericTypeNode*>(arg.get());
                    for (size_t i = 0; i < generic_func->generic_params.size(); i++) {
                        if (generic_func->generic_params[i].name == gen->name) {
                            // 使用对应的具体类型（原始指针）
                            if (i < type_args.size()) {
                                concrete_args_raw.push_back(type_args[i].get());
                            }
                            break;
                        }
                    }
                } else {
                    // 非泛型参数，直接使用（原始指针）
                    concrete_args_raw.push_back(arg.get());
                }
            }
            
            // 实例化struct（使用原始指针数组）
            if (!concrete_args_raw.empty()) {

                std::string struct_mangled = named_ret->name;
                for (const auto* arg_ptr : concrete_args_raw) {
                    struct_mangled += "_";
                    if (arg_ptr->kind == Type::Kind::Primitive) {
                        const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(arg_ptr);
                        switch (prim->prim_type) {
                            case PrimitiveType::I32: struct_mangled += "i32"; break;
                            case PrimitiveType::I64: struct_mangled += "i64"; break;
                            case PrimitiveType::STRING: struct_mangled += "string"; break;
                            default: struct_mangled += "T"; break;
                        }
                    } else if (arg_ptr->kind == Type::Kind::Named) {
                        const NamedTypeNode* named = static_cast<const NamedTypeNode*>(arg_ptr);
                        struct_mangled += named->name;
                    }
                }
                
                // 检查是否已实例化
                if (struct_types_.find(struct_mangled) == struct_types_.end()) {
                    // 手动实例化
                    auto gen_struct_it = generic_structs_.find(named_ret->name);

                    if (gen_struct_it == generic_structs_.end() && symbol_table_) {
                        auto symbol = symbol_table_->lookup(named_ret->name, module_name_);
                        if (symbol && symbol->ast_node) {
                            const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
                            if (struct_def && !struct_def->generic_params.empty()) {
                                generic_structs_[named_ret->name] = struct_def;
                                gen_struct_it = generic_structs_.find(named_ret->name);
                            }
                        }
                    }
                    
                    if (gen_struct_it != generic_structs_.end()) {
                        const StructStmt* gen_struct = gen_struct_it->second;
                        
                        // 建立临时type_param_map_
                        std::map<std::string, std::map<std::string, llvm::Type*>> temp_map;
                        for (size_t i = 0; i < concrete_args_raw.size() && i < gen_struct->generic_params.size(); i++) {
                            llvm::Type* concrete_type = convertType(concrete_args_raw[i]);
                            temp_map[gen_struct->generic_params[i].name][struct_mangled] = concrete_type;
                        }
                        
                        // 保存旧map
                        auto saved_map = type_param_map_;
                        type_param_map_ = temp_map;
                        
                        // 创建struct
                        llvm::StructType* st = llvm::StructType::create(*context_, struct_mangled);
                        struct_types_[struct_mangled] = st;
                        struct_defs_[struct_mangled] = gen_struct;
                        
                        std::vector<llvm::Type*> field_types;
                        for (const auto& field : gen_struct->fields) {
                            field_types.push_back(resolveGenericType(field.type.get()));
                        }
                        st->setBody(field_types);
                        
                        // 恢复map
                        type_param_map_ = saved_map;
                    }
                }
            }
        }
    }
    
    // 生成函数体
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*context_, "entry", func);
    auto old_insert_point = builder_->saveIP();
    builder_->SetInsertPoint(bb);
    
    // 保存旧的named_values
    auto old_named_values = named_values_;
    auto old_variable_types = variable_types_;
    auto old_array_element_types = array_element_types_;
    
    // 保存参数
    size_t idx = 0;
    for (auto& arg : func->args()) {
        const auto& param = generic_func->parameters[idx];
        
        // Check if it's数组参数
        if (param.type->kind == Type::Kind::Array) {
            // 数组参数：arg是ptr，直接创建ptr的alloca
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                llvm::PointerType::get(*context_, 0), nullptr, param.name
            );
            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            
            // 记录为指针类型（因为数组参数实际上是指针）
            variable_types_[param.name] = llvm::PointerType::get(*context_, 0);
            
            // 记录元素类型
            const ArrayTypeNode* array_type = static_cast<const ArrayTypeNode*>(param.type.get());
            llvm::Type* elem_type = resolveGenericType(array_type->element_type.get());
            array_element_types_[param.name] = elem_type;
        } else if (param.type->kind == Type::Kind::Optional) {
            // 【新增】：T?参数也是指针
            llvm::Type* param_type = resolveGenericType(param.type.get());
            // T?参数：arg是ptr
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                llvm::PointerType::get(*context_, 0), nullptr, param.name
            );
        // 检查是Struct还是Enum（通过尝试转换）
        const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
        const EnumStmt* enum_def = static_cast<const EnumStmt*>(symbol->ast_node);
        
        // 简单判断：如果type是StructType，则是Struct
        if (symbol->type && symbol->type->isStructTy()) {
            // 重建Struct类型
            llvm::StructType* original = llvm::cast<llvm::StructType>(symbol->type);
            
            std::vector<llvm::Type*> field_types;
            for (unsigned i = 0; i < original->getNumElements(); i++) {
                llvm::Type* field_type = original->getElementType(i);
                field_types.push_back(convertTypeToCurrentContext(field_type));
            }
            
            llvm::StructType* new_struct = llvm::StructType::create(*context_, field_types, type_name);
            struct_types_[type_name] = new_struct;
            
            // 保存定义（如果有）
            if (struct_def) {
                struct_defs_[type_name] = struct_def;
            }
        } else {
            // 重建Enum类型
            std::vector<llvm::Type*> enum_fields = {
                llvm::Type::getInt32Ty(*context_),  // tag
                llvm::Type::getInt64Ty(*context_)   // data
            };
            
            // Enum不需要创建命名类型，直接使用
            if (enum_def) {
                enum_defs_[type_name] = enum_def;
            }
        }
    }
}

/**
 * 标识符表达式生成
 * 支持：普通变量、self、数组、struct
 */
    if (memcpy_func) {
        builder_->CreateCall(memcpy_func, {heap_ptr, temp_result, size_val});
    }
    
    // 5. 返回堆指针（统一语义）
    return heap_ptr;
}

/**
 * @brief 实例化泛型struct的所有方法（单态化）
 * @param generic_struct 泛型struct定义
 * @param struct_mangled_name Struct修饰名称（如Pair_i32_string）
 * @param struct_type Struct的LLVM类型
 * @param type_args 类型参数列表
 * 
 * 当泛型struct被实例化时自动调用，生成所有方法的具体版本。
 * 
 * 处理流程：
 * 1. 遍历泛型struct的所有方法定义
 * 2. 为每个方法生成修饰名称（如new_i32_string）
 * 3. 构造参数类型列表：
 *    - self参数：struct指针（ptr）
 *    - 其他参数：解析泛型类型
 * 4. 确定返回类型（支持Self类型）
 * 5. 创建LLVM函数并注册
 * 6. 生成方法体：
 *    - 设置current_struct_上下文
 *    - 处理self参数
 *    - 生成方法体语句
 * 7. 恢复上下文
 * 
 * @note 支持静态方法和实例方法
 * @note 自动处理Self类型解析
 * @note 方法注册到struct_methods_映射
 */
void CodeGenerator::instantiateGenericStructMethods(
    const StructStmt* generic_struct,
    const std::string& struct_mangled_name,
    llvm::StructType* struct_type,
    const std::vector<TypePtr>& type_args) {
    
    if (!generic_struct || generic_struct->methods.empty()) {
        return;  // 没有方法，直接返回
    }
    
    // 【关键】保存并设置当前struct上下文，用于Self类型解析
    auto old_current_struct = current_struct_;
    auto old_current_struct_name = current_struct_name_;
    current_struct_ = generic_struct;
    current_struct_name_ = struct_mangled_name;
    
    // 遍历所有方法并实例化
    for (const auto& method : generic_struct->methods) {
        // 构造方法的mangled name
        // 例如：Pair_i32_i32 -> new_i32_i32, get_first_i32_i32
        std::string method_mangled = method->name + "_" + 
            struct_mangled_name.substr(generic_struct->name.length() + 1);
        
        // 检查是否已实例化
        if (functions_.find(method_mangled) != functions_.end()) {
            continue;  // 已存在，跳过
        }
        
        // 构造参数类型列表
        std::vector<llvm::Type*> param_types;
        
        for (const auto& param : method->parameters) {
            llvm::Type* param_type = nullptr;
            
            if (param.is_self) {
                // self参数是struct指针
                param_type = llvm::PointerType::get(*context_, 0);
            } else {
                // 普通参数，使用resolveGenericType解析
                param_type = resolveGenericType(param.type.get());
                
                // struct和数组参数传指针
                if (param_type && (llvm::isa<llvm::StructType>(param_type) || 
                    llvm::isa<llvm::ArrayType>(param_type))) {
                    param_type = llvm::PointerType::get(*context_, 0);
                }
            }
            
            if (param_type) {
                param_types.push_back(param_type);
            }
        }
        
        // 确定返回类型
        llvm::Type* return_type = nullptr;
        if (method->return_type) {
            return_type = resolveGenericType(method->return_type.get());
            
            // struct返回值使用指针
            if (return_type && llvm::isa<llvm::StructType>(return_type)) {
                return_type = llvm::PointerType::get(*context_, 0);
            }
        } else {
            return_type = llvm::Type::getVoidTy(*context_);
        }
        
        // 创建函数
        llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
        llvm::Function* func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            method_mangled,
            module_.get()
        );
        
        // 注册函数
        functions_[method_mangled] = func;
        struct_methods_[struct_mangled_name][method->name] = func;
        
        // 生成函数体
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*context_, "entry", func);
        auto old_insert_point = builder_->saveIP();
        builder_->SetInsertPoint(entry_bb);
        
        // 保存当前上下文
        auto old_named_values = named_values_;
        auto old_variable_types = variable_types_;
        auto old_current_function = current_function_;
        auto old_current_struct = current_struct_;
        auto old_current_struct_name = current_struct_name_;
        auto old_is_method = current_is_method_;
        
        current_function_ = func;
        current_struct_ = generic_struct;
        current_struct_name_ = struct_mangled_name;
        current_is_method_ = false;
        
        // 设置参数
        size_t arg_idx = 0;
        for (auto& arg : func->args()) {
            const auto& param = method->parameters[arg_idx];
            
            llvm::AllocaInst* alloca = builder_->CreateAlloca(arg.getType(), nullptr, param.name);
            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            
            // 对于self参数，记录实际的struct类型
            if (param.is_self) {
                variable_types_[param.name] = struct_type;
                current_is_method_ = true;

                variable_types_[param.name] = arg.getType();
            }
            
            arg_idx++;
        }
        
        // 生成方法体
        if (method->body) {
            generateStmt(method->body.get());
        }
        
        // 如果没有终止指令，添加默认返回
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (func->getReturnType()->isVoidTy()) {
                builder_->CreateRetVoid();
            } else {
                builder_->CreateRet(llvm::Constant::getNullValue(func->getReturnType()));
            }
        }
        
        // 恢复上下文
        named_values_ = old_named_values;
        variable_types_ = old_variable_types;
        current_function_ = old_current_function;
        current_struct_ = old_current_struct;
        current_struct_name_ = old_current_struct_name;
        current_is_method_ = old_is_method;
        builder_->restoreIP(old_insert_point);
    }
    
    // 恢复外层struct上下文
    current_struct_ = old_current_struct;
    current_struct_name_ = old_current_struct_name;
}

} // namespace pawc






} // namespace pawc
