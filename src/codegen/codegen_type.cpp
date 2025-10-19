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
            
            // Check for generic parameters
            if (!named_type->generic_args.empty()) {
                // Generic struct: Box<i32>
                auto it = generic_structs_.find(named_type->name);
                if (it != generic_structs_.end()) {
                    return instantiateGenericStruct(named_type->name, named_type->generic_args);
                }
                // Generic enum: Option<i32>
                auto enum_it = generic_enums_.find(named_type->name);
                if (enum_it != generic_enums_.end()) {
                    return instantiateGenericEnum(named_type->name, named_type->generic_args);
                }
            }
            
            // First try to find enum type (enum as value type, check first)
            auto enum_type = getEnumType(named_type->name);
            if (enum_type) {
                return enum_type;  // enum as value type {i32, i64}
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

llvm::Type* CodeGenerator::convertPrimitiveType(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::I32: return llvm::Type::getInt32Ty(*context_);
        case PrimitiveType::I64: return llvm::Type::getInt64Ty(*context_);
        case PrimitiveType::F32: return llvm::Type::getFloatTy(*context_);
        case PrimitiveType::F64: return llvm::Type::getDoubleTy(*context_);
        case PrimitiveType::BOOL: return llvm::Type::getInt1Ty(*context_);
        case PrimitiveType::CHAR: return llvm::Type::getInt8Ty(*context_);
        case PrimitiveType::STRING: return llvm::PointerType::get(*context_, 0);
        case PrimitiveType::VOID: return llvm::Type::getVoidTy(*context_);
        default: return llvm::Type::getVoidTy(*context_);
    }
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
