// Struct and Enum code generation
#include "codegen.h"
#include <iostream>

namespace pawc {

llvm::Type* CodeGenerator::getEnumType(const std::string& name) {
    // Enum表示为tagged union: { i32 tag, [max_size] data }
    auto it = enum_defs_.find(name);
    if (it == enum_defs_.end()) {
        return nullptr;
    }
    
    // 简化: 使用 {i32, i64} 表示enum
    // tag = variant索引, data = 关联值（简化为i64）
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),  // tag
        llvm::Type::getInt64Ty(*context_)   // data
    };
    return llvm::StructType::get(*context_, fields);
}

llvm::Type* CodeGenerator::convertPrimitiveType(PrimitiveType type) {

        int tag = 0;
        for (const auto& variant : def_it->second->variants) {
            if (variant.name == expr->variant_name) break;
            tag++;
        }
        
        llvm::Value* tag_ptr = builder_->CreateStructGEP(
            static_cast<llvm::StructType*>(enum_type), alloca, 0
        );
        builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, tag)), tag_ptr);
        
        // 设置data（简化: 只支持一个i32值）
        if (!expr->values.empty()) {
            llvm::Value* val = generateExpr(expr->values[0].get());
            if (val) {
                llvm::Value* data_ptr = builder_->CreateStructGEP(
                    static_cast<llvm::StructType*>(enum_type), alloca, 1
                );
                llvm::Value* extended = builder_->CreateSExtOrTrunc(val, llvm::Type::getInt64Ty(*context_));
                builder_->CreateStore(extended, data_ptr);
            }
        }
    }
    
    // 返回load的值而不是指针（enum是值类型）
    return builder_->CreateLoad(enum_type, alloca, "enum_val");
}



llvm::Value* CodeGenerator::generateArrayLiteralExpr(const ArrayLiteralExpr* expr) {
    if (expr->elements.empty()) {
        return nullptr;
    }
    
    // 注意：数组字面量应该使用变量声明中的类型，而不是从第一个元素推导
    // generateArrayLiteralExpr不应该独立推导类型
    // 这个函数主要在generateLetStmt中的特殊路径使用，那里已经知道目标类型
    
    // 简化实现：返回nullptr，让外层处理
    // 实际的数组初始化在generateLetStmt中完成
    return nullptr;
}

// ====== 泛型支持 ======

// 泛型名称修饰：Box<i32> → Box_i32
std::string CodeGenerator::mangleGenericName(const std::string& base_name, const std::vector<TypePtr>& type_args) {
    std::string mangled = base_name;
    for (const auto& arg : type_args) {
        mangled += "_";
        if (arg->kind == Type::Kind::Named) {
            const NamedTypeNode* named = static_cast<const NamedTypeNode*>(arg.get());
            mangled += named->name;
        } else if (arg->kind == Type::Kind::Primitive) {
            const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(arg.get());
            // 简化类型名
            switch (prim->prim_type) {
                case PrimitiveType::I32: mangled += "i32"; break;
                case PrimitiveType::I64: mangled += "i64"; break;
                case PrimitiveType::STRING: mangled += "string"; break;
                default: mangled += "T"; break;
            }
        }
    }
    return mangled;
}

// 解析泛型类型（替换类型参数）
llvm::Type* CodeGenerator::resolveGenericType(const Type* type) {
    // 处理Self类型（在泛型struct方法中）
    if (type->kind == Type::Kind::SelfType) {
        if (!current_struct_name_.empty()) {
            // 返回当前struct的指针类型（统一语义）
            return llvm::PointerType::get(*context_, 0);
        }
    }
    
    if (type->kind == Type::Kind::Generic) {
        const GenericTypeNode* gen = static_cast<const GenericTypeNode*>(type);
        
        // 在type_param_map_中查找当前类型参数的具体类型
        for (const auto& entry : type_param_map_) {
            if (entry.first == gen->name && !entry.second.empty()) {
                return entry.second.begin()->second;
            }
        }
    }
    return convertType(type);
}


// 解析泛型struct名称：将"Pair_K_V"解析为"Pair_i32_string"
std::string CodeGenerator::resolveGenericStructName(const std::string& mangled_name) {
    // 分析mangled_name，提取base_name和类型参数占位符
    // 例如："Pair_K_V" -> "Pair" + ["K", "V"]
    size_t first_underscore = mangled_name.find('_');
    if (first_underscore == std::string::npos) {
        return mangled_name;  // 没有下划线，不是泛型实例
    }
    
    std::string base_name = mangled_name.substr(0, first_underscore);
    std::string params_part = mangled_name.substr(first_underscore + 1);
    
    // 解析参数部分（简化：按_分割）
    std::vector<std::string> type_params;
    std::string current_param;
    for (char c : params_part) {
        if (c == '_') {
            if (!current_param.empty()) {
                type_params.push_back(current_param);
                current_param.clear();
            }
        } else {
            current_param += c;
        }
    }
    if (!current_param.empty()) {
        type_params.push_back(current_param);
    }
    
    // 解析每个类型参数
    std::vector<std::string> resolved_types;
    for (const auto& param : type_params) {
        // Check if it's泛型参数（单字母大写）
        if (param.length() == 1 && std::isupper(param[0])) {
            // 在type_param_map_中查找具体类型
            auto it = type_param_map_.find(param);
            if (it != type_param_map_.end() && !it->second.empty()) {
                llvm::Type* concrete_type = it->second.begin()->second;
                // 将LLVM Type转为字符串名称
                std::string type_str = "unknown";
                if (concrete_type->isIntegerTy()) {
                    unsigned bits = concrete_type->getIntegerBitWidth();
                    type_str = "i" + std::to_string(bits);
                } else if (concrete_type->isPointerTy()) {

                    type_str = "string";  // 假设指针是string
                }
                resolved_types.push_back(type_str);
            } else {
                resolved_types.push_back(param);  // 未找到，保持原样
            }
        } else {
            resolved_types.push_back(param);  // 非泛型参数，保持原样
        }
    }
    
    // 重新组装
    std::string result = base_name;
    for (const auto& t : resolved_types) {
        result += "_" + t;
    }
    
    return result;
}

// Check if it's泛型函数
bool CodeGenerator::isGenericFunction(const std::string& name) {
    return generic_functions_.find(name) != generic_functions_.end();
}

/**
 * @brief 实例化泛型函数（单态化）
 * @param name 泛型函数名
 * @param type_args 类型参数列表
 * @return llvm::Function* 实例化的函数，失败返回nullptr
 * 
 * 实现泛型函数的单态化（monomorphization）：
 * 1. 生成修饰名称（如sum_i32）
 * 2. 检查是否已实例化（缓存）
 * 3. 建立类型参数映射（T -> i32）
 * 4. 解析参数类型和返回类型
 * 5. 创建LLVM函数
 * 6. 生成函数体（使用type_param_map_）
 * 
 * @note 支持跨模块泛型函数调用
 * @note 自动处理数组参数的元素类型记录
 */
llvm::Function* CodeGenerator::instantiateGenericFunction(
    const std::string& name,
    const std::vector<TypePtr>& type_args) {
    
    auto it = generic_functions_.find(name);
    if (it == generic_functions_.end()) {
        return nullptr;
    }
    
    const FunctionStmt* generic_func = it->second;

    
    // 生成修饰后的名称
    std::string mangled_name = mangleGenericName(name, type_args);
    
    // 检查是否已经实例化
    auto func_it = functions_.find(mangled_name);
    if (func_it != functions_.end()) {
        return func_it->second;
    }
    
    // 建立类型参数映射
    std::map<std::string, std::map<std::string, llvm::Type*>> old_map = type_param_map_;
    for (size_t i = 0; i < generic_func->generic_params.size() && i < type_args.size(); i++) {
        const std::string& param_name = generic_func->generic_params[i].name;
        llvm::Type* concrete_type = convertType(type_args[i].get());
        type_param_map_[param_name][mangled_name] = concrete_type;
    }
    
    // 生成具体函数

    std::vector<llvm::Type*> param_types;
    for (const auto& param : generic_func->parameters) {
        if (!param.is_self) {
            llvm::Type* param_type = resolveGenericType(param.type.get());
            
            // 对于数组参数，使用指针类型（数组按指针传递）
            if (llvm::isa<llvm::ArrayType>(param_type)) {
                param_type = llvm::PointerType::get(*context_, 0);
            }
            
            // 【新增】：对于struct参数，使用指针类型（struct按指针传递）
            if (llvm::isa<llvm::StructType>(param_type)) {
                param_type = llvm::PointerType::get(*context_, 0);
            }
            
            // 【新增】：对于T?参数，使用指针类型（T?按指针传递）
            // 注意：resolveGenericType返回的已经是Optional<T> struct，我们需要指针
            if (param.type->kind == Type::Kind::Optional) {
                param_type = llvm::PointerType::get(*context_, 0);

            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            
            // 【关键】：记录具体的Optional<T>类型，而不是通用ptr
            // 这样is表达式时能找到正确的Optional定义
            variable_types_[param.name] = param_type;  // 记录Optional<T> StructType
        } else if (param.type->kind == Type::Kind::Named) {
            // 【新增】：struct参数也是指针
            llvm::Type* param_type = resolveGenericType(param.type.get());
            if (llvm::isa<llvm::StructType>(param_type)) {
                // struct参数：arg是ptr
                llvm::AllocaInst* alloca = builder_->CreateAlloca(
                    llvm::PointerType::get(*context_, 0), nullptr, param.name
                );
                builder_->CreateStore(&arg, alloca);
                named_values_[param.name] = alloca;
                
                // 【关键】：记录具体的struct类型，而不是通用ptr
                // 这样成员访问时能找到正确的struct定义
                variable_types_[param.name] = param_type;  // 记录StructType
            } else {
                // 非struct的Named类型（如enum）：正常处理
                llvm::AllocaInst* alloca = builder_->CreateAlloca(param_type, nullptr, param.name);
                builder_->CreateStore(&arg, alloca);
                named_values_[param.name] = alloca;
                variable_types_[param.name] = param_type;
            }
        } else {
            // 其他类型参数：正常处理
            llvm::Type* param_type = resolveGenericType(param.type.get());
            llvm::AllocaInst* alloca = builder_->CreateAlloca(param_type, nullptr, param.name);
            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            variable_types_[param.name] = param_type;
        }
        
        idx++;
    }
    
    // 生成函数体
    generateStmt(generic_func->body.get());
    
    // 如果没有terminator，添加默认返回
    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (return_type->isVoidTy()) {
            builder_->CreateRetVoid();
        }
    }
    
    // 恢复
    named_values_ = old_named_values;
    variable_types_ = old_variable_types;
    array_element_types_ = old_array_element_types;
    type_param_map_ = old_map;
    builder_->restoreIP(old_insert_point);
    
    llvm::verifyFunction(*func);
    
    return func;
}

// 实例化泛型struct
llvm::Type* CodeGenerator::instantiateGenericStruct(
    const std::string& name,
    const std::vector<TypePtr>& type_args) {
    
    const StructStmt* generic_struct = nullptr;
    
    // 1. 先查本地generic_structs_
    auto it = generic_structs_.find(name);
    if (it != generic_structs_.end()) {
        generic_struct = it->second;
    } else if (symbol_table_) {
        // 2. 查找跨模块的泛型struct定义
        auto symbol = symbol_table_->lookup(name, module_name_);
        if (symbol && symbol->kind == SymbolTable::SymbolKind::Type && symbol->ast_node) {
            // Check if it'sstruct定义（简化：假设有generic_params就是泛型struct）
            const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
            if (struct_def && !struct_def->generic_params.empty()) {
                generic_struct = struct_def;
                // 添加到本地缓存
                generic_structs_[name] = generic_struct;
            }
        }
    }
    
    if (!generic_struct) {
        return nullptr;
    }
    
    // 生成修饰后的名称
    std::string mangled_name = mangleGenericName(name, type_args);
    
    // 检查是否已经实例化
    auto type_it = struct_types_.find(mangled_name);
    if (type_it != struct_types_.end()) {
        return type_it->second;  // 返回struct类型本身
    }
    
    // 建立类型参数映射
    std::map<std::string, std::map<std::string, llvm::Type*>> old_map = type_param_map_;
    for (size_t i = 0; i < generic_struct->generic_params.size() && i < type_args.size(); i++) {
        const std::string& param_name = generic_struct->generic_params[i].name;
        llvm::Type* concrete_type = convertType(type_args[i].get());
        type_param_map_[param_name][mangled_name] = concrete_type;
    }
    
    // 先创建不透明struct类型，防止递归
    llvm::StructType* struct_type = llvm::StructType::create(*context_, mangled_name);
    struct_types_[mangled_name] = struct_type;
    struct_defs_[mangled_name] = generic_struct;  // 注册定义
    
    // 然后填充字段类型
    std::vector<llvm::Type*> field_types;
    for (const auto& field : generic_struct->fields) {
        field_types.push_back(resolveGenericType(field.type.get()));
    }
    struct_type->setBody(field_types);
    
    // 【新增】实例化泛型struct的所有方法
    instantiateGenericStructMethods(generic_struct, mangled_name, struct_type, type_args);
    
    // 恢复
    type_param_map_ = old_map;
    
    // 注册泛型struct实例到SymbolTable（如果有）
    if (symbol_table_ && generic_struct->is_public) {
        symbol_table_->registerGenericStructInstance(
            module_name_,
            mangled_name,
            name,
            true,  // 继承pub属性
            struct_type,
            static_cast<const void*>(generic_struct)
        );
    }
    
    return struct_type;  // 返回struct类型本身，不是指针

}

// 实例化泛型enum
llvm::Type* CodeGenerator::instantiateGenericEnum(
    const std::string& name,
    const std::vector<TypePtr>& type_args) {
    
    auto it = generic_enums_.find(name);
    if (it == generic_enums_.end()) {
        return nullptr;
    }
    
    const EnumStmt* generic_enum = it->second;
    
    // 生成修饰后的名称
    std::string mangled_name = mangleGenericName(name, type_args);
    
    // 检查是否已经实例化
    auto enum_it = enum_defs_.find(mangled_name);
    if (enum_it != enum_defs_.end()) {
        // 已经实例化，直接返回类型
        std::vector<llvm::Type*> fields = {
            llvm::Type::getInt32Ty(*context_),  // tag
            llvm::Type::getInt64Ty(*context_)   // data
        };
        return llvm::StructType::get(*context_, fields);
    }
    
    // 建立类型参数映射
    std::map<std::string, std::map<std::string, llvm::Type*>> old_map = type_param_map_;
    for (size_t i = 0; i < generic_enum->generic_params.size() && i < type_args.size(); i++) {
        const std::string& param_name = generic_enum->generic_params[i].name;
        llvm::Type* concrete_type = convertType(type_args[i].get());
        type_param_map_[param_name][mangled_name] = concrete_type;
    }
    
    // 注册实例化的enum定义（使用mangled_name）
    enum_defs_[mangled_name] = generic_enum;
    
    // 恢复
    type_param_map_ = old_map;
    
    // Enum表示为 {i32 tag, i64 data}（简化）
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),  // tag
        llvm::Type::getInt64Ty(*context_)   // data
    };
    
    return llvm::StructType::get(*context_, fields);
}


// ============================================================================
// 跨模块类型转换
// ============================================================================

/**
 * 将来自其他Context的类型转换为当前Context的类型
 * 用于跨模块函数调用时的类型匹配
 */
llvm::Type* CodeGenerator::convertTypeToCurrentContext(llvm::Type* type) {
    if (!type) return nullptr;
    
    // 基本类型直接映射到当前Context
    if (type->isVoidTy()) {
        return llvm::Type::getVoidTy(*context_);
    } else if (type->isIntegerTy()) {
        unsigned bits = type->getIntegerBitWidth();
        return llvm::Type::getIntNTy(*context_, bits);
    } else if (type->isFloatTy()) {
        return llvm::Type::getFloatTy(*context_);
    } else if (type->isDoubleTy()) {
        return llvm::Type::getDoubleTy(*context_);
    } else if (type->isPointerTy()) {
        // LLVM 21使用opaque pointers
        return llvm::PointerType::get(*context_, 0);
    } else if (type->isArrayTy()) {
        llvm::ArrayType* array_type = llvm::cast<llvm::ArrayType>(type);

        llvm::Type* elem_type = convertTypeToCurrentContext(array_type->getElementType());
        return llvm::ArrayType::get(elem_type, array_type->getNumElements());
    } else if (type->isStructTy()) {
        // 结构体类型 - 尝试通过名称查找或重建
        llvm::StructType* struct_type = llvm::cast<llvm::StructType>(type);
        
        // 如果有名字，尝试在当前模块中查找同名类型
        if (struct_type->hasName()) {
            std::string type_name = struct_type->getName().str();
            
            // 尝试从struct_types_中查找
            auto it = struct_types_.find(type_name);
            if (it != struct_types_.end()) {
                return it->second;
            }
            
            // 尝试从当前Context查找
            llvm::StructType* local_struct = llvm::StructType::getTypeByName(*context_, type_name);
            if (local_struct) {
                return local_struct;
            }
        }
        
        // 匿名结构体或无法重建的，创建等价结构
        std::vector<llvm::Type*> field_types;
        for (unsigned i = 0; i < struct_type->getNumElements(); i++) {
            llvm::Type* field_type = struct_type->getElementType(i);
            field_types.push_back(convertTypeToCurrentContext(field_type));
        }
        
        // 如果有名字，创建命名类型；否则创建匿名类型
        if (struct_type->hasName()) {
            std::string type_name = struct_type->getName().str();
            llvm::StructType* new_struct = llvm::StructType::create(*context_, field_types, type_name);
            struct_types_[type_name] = new_struct;
            return new_struct;
        } else {
            return llvm::StructType::get(*context_, field_types);
        }
    }
    
    // 默认返回i32
    return llvm::Type::getInt32Ty(*context_);
}

/**
 * 从其他模块导入类型定义
 * 在当前模块中重建Struct/Enum类型
 */
void CodeGenerator::importTypeFromModule(const std::string& type_name, const std::string& from_module) {
    if (!symbol_table_) return;
    
    // 查找类型符号
    auto symbol = symbol_table_->lookupInModule(from_module, type_name);
    if (!symbol || symbol->kind != SymbolTable::SymbolKind::Type) {
        return;
    }
    

    if (!symbol_table_->isAccessible(*symbol, module_name_)) {
        std::cerr << "Type " << type_name << " in module " << from_module 
                  << " is not accessible" << std::endl;
        return;
    }
    
    // 检查是否已经导入
    if (struct_types_.find(type_name) != struct_types_.end() ||
        enum_defs_.find(type_name) != enum_defs_.end()) {
        return;  // 已导入
    }
    
    // 根据AST节点重建类型
    if (symbol->ast_node) {


} // namespace pawc
