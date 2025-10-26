// Struct and Enum code generation
#include "codegen.h"
#include "llvm/IR/Verifier.h"
#include <iostream>

namespace pawc {

llvm::Type* CodeGenerator::getEnumType(const std::string& name) {
    // Enum represented as tagged union: { i32 tag, [max_size] data }
    auto it = enum_defs_.find(name);
    if (it == enum_defs_.end()) {
        return nullptr;
    }
    
    const EnumStmt* enum_def = it->second;
    
    // 计算所有变体的最大类型大小（union表示）
    llvm::Type* max_type = nullptr;
    uint64_t max_size = 0;
    
    for (const auto& variant : enum_def->variants) {
        if (variant.associated_types.empty()) {
            // 无关联值的变体不占额外空间
            continue;
        }
        
        // 转换第一个关联值的类型
        llvm::Type* variant_type = convertType(variant.associated_types[0].get());
        if (!variant_type) continue;
        
        // 获取类型的分配大小
        uint64_t size = module_->getDataLayout().getTypeAllocSize(variant_type);
        
        if (size > max_size) {
            max_size = size;
            max_type = variant_type;
        }
    }
    
    // 如果所有变体都没有关联值，使用i32作为占位
    if (!max_type) {
        max_type = llvm::Type::getInt32Ty(*context_);
    }
    
    // 构建enum类型：{ i32 tag, T max_data }
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),  // tag
        max_type                             // union data (最大类型)
    };
    return llvm::StructType::get(*context_, fields);
}

llvm::Value* CodeGenerator::generateArrayLiteralExpr(const ArrayLiteralExpr* expr) {
    if (expr->elements.empty()) {
        return nullptr;
    }
    
    // Note: Array literals should use type from variable declaration, not infer from first element
    // generateArrayLiteralExpr should not independently infer types
    // This function is mainly used in special paths in generateLetStmt, where target type is known
    
    // Simplified implementation: return nullptr, let outer layer handle
    // Actual array initialization is done in generateLetStmt
    return nullptr;
}

/**
 * 生成元组字面量: (a, b, c)
 * 返回元组struct值
 */
llvm::Value* CodeGenerator::generateTupleLiteralExpr(const TupleLiteralExpr* expr) {
    // 生成每个元素的值
    std::vector<llvm::Value*> element_values;
    std::vector<llvm::Type*> element_types;
    
    for (const auto& elem : expr->elements) {
        llvm::Value* val = generateExpr(elem.get());
        if (!val) return nullptr;
        element_values.push_back(val);
        element_types.push_back(val->getType());
    }
    
    // 创建元组类型（struct）
    llvm::StructType* tuple_type = llvm::StructType::get(*context_, element_types);
    
    // 创建临时alloca存储元组
    llvm::AllocaInst* tuple_alloca = builder_->CreateAlloca(tuple_type, nullptr, "tuple_temp");
    
    // 初始化每个字段
    for (size_t i = 0; i < element_values.size(); i++) {
        llvm::Value* field_ptr = builder_->CreateStructGEP(tuple_type, tuple_alloca, i, "field_ptr");
        builder_->CreateStore(element_values[i], field_ptr);
    }
    
    // 加载并返回元组值
    return builder_->CreateLoad(tuple_type, tuple_alloca, "tuple");
}

llvm::Value* CodeGenerator::generateStructLiteralExpr(const StructLiteralExpr* expr) {
    // Resolve generic struct name (if in generic context)
    std::string resolved_name = resolveGenericStructName(expr->type_name);
    
    // First look up directly from struct_types_ (support generic instances like Box_i32)
    auto type_it = struct_types_.find(resolved_name);
    llvm::StructType* struct_type = nullptr;
    
    if (type_it != struct_types_.end()) {
        struct_type = type_it->second;
    } else {
        // If not found directly, try through getOrCreateStructType
        struct_type = getOrCreateStructType(resolved_name);
    }
    
    if (!struct_type) {
        std::cerr << "Unknown struct type: " << expr->type_name 
                  << " (resolved: " << resolved_name << ")" << std::endl;
        return nullptr;
    }
    
    // 1. Allocate temporary struct on stack
    llvm::Value* temp_alloca = builder_->CreateAlloca(struct_type, nullptr, "struct_temp");
    
    // 2. Initialize fields (using resolved_name)
    auto def_it = struct_defs_.find(resolved_name);
    if (def_it != struct_defs_.end()) {
        const StructStmt* struct_def = def_it->second;
        for (size_t i = 0; i < expr->fields.size() && i < struct_def->fields.size(); i++) {
            llvm::Value* field_val = generateExpr(expr->fields[i].value.get());
            if (field_val) {
                // Get target type of field
                llvm::Type* target_type = struct_type->getElementType(i);
                
                // 【全面修复】：处理struct类型字段
                if (target_type->isStructTy() && field_val->getType()->isPointerTy()) {
                    // struct字段：field_val是指针，需要load struct值
                    llvm::Value* struct_val = builder_->CreateLoad(target_type, field_val, "struct_field_val");
                    llvm::Value* field_ptr = builder_->CreateStructGEP(struct_type, temp_alloca, i);
                    builder_->CreateStore(struct_val, field_ptr);
                } else if (field_val->getType()->isIntegerTy() && target_type->isIntegerTy()) {
                    // Type conversion for integers
                    unsigned src_bits = field_val->getType()->getIntegerBitWidth();
                    unsigned dst_bits = target_type->getIntegerBitWidth();
                    
                    if (src_bits < dst_bits) {
                        field_val = builder_->CreateSExt(field_val, target_type, "field_sext");
                    } else if (src_bits > dst_bits) {
                        field_val = builder_->CreateTrunc(field_val, target_type, "field_trunc");
                    }
                    
                    llvm::Value* field_ptr = builder_->CreateStructGEP(struct_type, temp_alloca, i);
                    builder_->CreateStore(field_val, field_ptr);
                } else {
                    // Other types: direct store
                    llvm::Value* field_ptr = builder_->CreateStructGEP(struct_type, temp_alloca, i);
                    builder_->CreateStore(field_val, field_ptr);
                }
            }
        }
    }
    
    // 3. Allocate heap memory
    const llvm::DataLayout& data_layout = module_->getDataLayout();
    uint64_t struct_size = data_layout.getTypeAllocSize(struct_type);
    llvm::Value* size_val = llvm::ConstantInt::get(*context_, llvm::APInt(64, struct_size));
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        std::cerr << "malloc not found!" << std::endl;
        return nullptr;
    }
    
    llvm::Value* heap_ptr = builder_->CreateCall(malloc_func, {size_val}, "struct_heap");
    
    // 4. Copy struct to heap
    llvm::Function* memcpy_func = module_->getFunction("memcpy");
    if (memcpy_func) {
        builder_->CreateCall(memcpy_func, {heap_ptr, temp_alloca, size_val});
    }
    
    // 5. Return heap pointer
    return heap_ptr;
}

llvm::Value* CodeGenerator::generateEnumVariantExpr(const EnumVariantExpr* expr) {
    // 尝试查找enum类型（可能是泛型实例化的mangled name）
    llvm::Type* enum_type = getEnumType(expr->enum_name);
    
    // 如果直接找不到，尝试重新调用getEnumType()
    // （可能enum_defs_刚刚更新）
    std::string enum_name = expr->enum_name;
    if (!enum_type) {
        // 再次尝试，这次会使用新的union计算逻辑
        enum_type = getEnumType(expr->enum_name);
        
        if (!enum_type) {
            std::cerr << "Unknown enum type: " << expr->enum_name << std::endl;
            return nullptr;
        }
    }
    
    // 分配enum实例
    llvm::Value* alloca = builder_->CreateAlloca(enum_type);
    
    // 设置tag（variant索引）
    auto def_it = enum_defs_.find(expr->enum_name);
    if (def_it != enum_defs_.end()) {
        int tag = 0;
        for (const auto& variant : def_it->second->variants) {
            if (variant.name == expr->variant_name) break;
            tag++;
        }
        
        llvm::Value* tag_ptr = builder_->CreateStructGEP(
            static_cast<llvm::StructType*>(enum_type), alloca, 0
        );
        builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, tag)), tag_ptr);
        
        // 设置data（支持任意类型的关联值）
        if (!expr->values.empty()) {
            llvm::Value* val = generateExpr(expr->values[0].get());
            if (val) {
                llvm::Value* data_ptr = builder_->CreateStructGEP(
                    static_cast<llvm::StructType*>(enum_type), alloca, 1
                );
                
                // 获取目标data字段的类型
                llvm::Type* data_field_type = static_cast<llvm::StructType*>(enum_type)->getElementType(1);
                
                // 根据源类型和目标类型进行转换
                llvm::Value* converted_val = val;
                llvm::Type* src_type = val->getType();
                
                if (src_type != data_field_type) {
                    // 类型转换
                    if (src_type->isIntegerTy() && data_field_type->isIntegerTy()) {
                        // 整数 → 整数（扩展或截断）
                        converted_val = builder_->CreateIntCast(val, data_field_type, true, "int_cast");
                    } else if (src_type->isPointerTy() && data_field_type->isIntegerTy()) {
                        // 指针 → 整数
                        converted_val = builder_->CreatePtrToInt(val, data_field_type, "ptr_to_int");
                    } else if (src_type->isIntegerTy() && data_field_type->isPointerTy()) {
                        // 整数 → 指针
                        converted_val = builder_->CreateIntToPtr(val, data_field_type, "int_to_ptr");
                    } else if (src_type->isPointerTy() && data_field_type->isPointerTy()) {
                        // 指针 → 指针（不同类型）
                        // 直接存储，LLVM的opaque pointer会处理
                        converted_val = val;
                    } else if (src_type->isFloatingPointTy() && data_field_type->isIntegerTy()) {
                        // 浮点 → 整数（bitcast）
                        converted_val = builder_->CreateBitCast(val, data_field_type, "float_to_int");
                    } else {
                        // 其他情况：尝试bitcast
                        if (module_->getDataLayout().getTypeAllocSize(src_type) == 
                            module_->getDataLayout().getTypeAllocSize(data_field_type)) {
                            converted_val = builder_->CreateBitCast(val, data_field_type, "bitcast");
                        } else {
                            std::cerr << "Warning: Incompatible enum data type, using original value" << std::endl;
                        }
                    }
                }
                
                builder_->CreateStore(converted_val, data_ptr);
            }
        }
    }
    
    // 返回alloca指针（与struct行为一致）
    // 实际使用时会根据需要load值
    return alloca;
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

// 克隆AST类型节点（深拷贝）
TypePtr CodeGenerator::cloneType(const Type* type) {
    if (!type) return nullptr;
    
    switch (type->kind) {
        case Type::Kind::Primitive: {
            const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(type);
            return std::make_unique<PrimitiveTypeNode>(prim->prim_type, prim->location);
        }
        case Type::Kind::Named: {
            const NamedTypeNode* named = static_cast<const NamedTypeNode*>(type);
            // 递归克隆泛型参数
            std::vector<TypePtr> cloned_args;
            for (const auto& arg : named->generic_args) {
                cloned_args.push_back(cloneType(arg.get()));
            }
            return std::make_unique<NamedTypeNode>(named->name, std::move(cloned_args), named->location);
        }
        case Type::Kind::Generic: {
            const GenericTypeNode* gen = static_cast<const GenericTypeNode*>(type);
            return std::make_unique<GenericTypeNode>(gen->name, gen->location);
        }
        case Type::Kind::Array: {
            const ArrayTypeNode* arr = static_cast<const ArrayTypeNode*>(type);
            return std::make_unique<ArrayTypeNode>(cloneType(arr->element_type.get()), arr->size, arr->location);
        }
        case Type::Kind::Slice: {
            const SliceTypeNode* slice = static_cast<const SliceTypeNode*>(type);
            return std::make_unique<SliceTypeNode>(cloneType(slice->element_type.get()), slice->location);
        }
        case Type::Kind::Optional: {
            const OptionalTypeNode* opt = static_cast<const OptionalTypeNode*>(type);
            return std::make_unique<OptionalTypeNode>(cloneType(opt->inner_type.get()), opt->location);
        }
        default:
            // 其他类型暂不支持，返回i32
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I32, type->location);
    }
}

// 将LLVM类型转换回AST类型节点（辅助函数）
TypePtr CodeGenerator::llvmTypeToASTType(llvm::Type* llvm_type) {
    SourceLocation loc;  // 默认位置
    
    if (!llvm_type) {
        return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I32, loc);
    }
    
    // 整数类型
    if (llvm_type->isIntegerTy()) {
        unsigned bits = llvm_type->getIntegerBitWidth();
        if (bits == 32) {
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I32, loc);
        } else if (bits == 64) {
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I64, loc);
        } else if (bits == 8) {
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I8, loc);
        } else if (bits == 16) {
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I16, loc);
        } else if (bits == 1) {
            return std::make_unique<PrimitiveTypeNode>(PrimitiveType::BOOL, loc);
        }
        return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I32, loc);
    }
    
    // 浮点类型
    if (llvm_type->isFloatTy()) {
        return std::make_unique<PrimitiveTypeNode>(PrimitiveType::F32, loc);
    }
    if (llvm_type->isDoubleTy()) {
        return std::make_unique<PrimitiveTypeNode>(PrimitiveType::F64, loc);
    }
    
    // 指针类型（可能是string）
    if (llvm_type->isPointerTy()) {
        return std::make_unique<PrimitiveTypeNode>(PrimitiveType::STRING, loc);
    }
    
    // 结构体类型（尝试从名称反推）
    if (llvm::StructType* struct_type = llvm::dyn_cast<llvm::StructType>(llvm_type)) {
        if (struct_type->hasName()) {
            std::string name = struct_type->getName().str();
            // 简单情况：返回NamedType（没有泛型参数）
            std::vector<TypePtr> empty_args;
            return std::make_unique<NamedTypeNode>(name, std::move(empty_args), loc);
        }
    }
    
    // 默认返回i32
    return std::make_unique<PrimitiveTypeNode>(PrimitiveType::I32, loc);
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
    
    // 处理Named类型（可能包含泛型参数）
    if (type->kind == Type::Kind::Named) {
        const NamedTypeNode* named = static_cast<const NamedTypeNode*>(type);
        
        // 如果Named类型有泛型参数，需要递归解析这些参数
        if (!named->generic_args.empty()) {
            // 创建新的泛型参数列表，解析每个参数
            std::vector<TypePtr> resolved_args;
            for (const auto& arg : named->generic_args) {
                // 如果参数是泛型类型（如T），需要从type_param_map_中解析
                if (arg->kind == Type::Kind::Generic) {
                    const GenericTypeNode* gen_arg = static_cast<const GenericTypeNode*>(arg.get());
                    llvm::Type* concrete_type = nullptr;
                    
                    // 在type_param_map_中查找
                    for (const auto& entry : type_param_map_) {
                        if (entry.first == gen_arg->name && !entry.second.empty()) {
                            concrete_type = entry.second.begin()->second;
                            break;
                        }
                    }
                    
                    if (concrete_type) {
                        // 将LLVM类型转换回AST类型节点
                        TypePtr resolved = llvmTypeToASTType(concrete_type);
                        resolved_args.push_back(std::move(resolved));
                    } else {
                        // 未找到映射，克隆原参数
                        resolved_args.push_back(cloneType(arg.get()));
                    }
                } else {
                    // 非泛型参数，克隆后保持原样
                    resolved_args.push_back(cloneType(arg.get()));
                }
            }
            
            // Debug: 打印解析后的参数信息
            #ifdef DEBUG_NESTED_GENERIC
            std::cerr << "Resolved nested generic: " << named->name << " with " << resolved_args.size() << " args" << std::endl;
            #endif
            
            // 使用解析后的参数实例化泛型struct
            if (auto it = generic_structs_.find(named->name); it != generic_structs_.end()) {
                return instantiateGenericStruct(named->name, resolved_args);
            }
            
            // 尝试泛型enum
            if (auto enum_it = generic_enums_.find(named->name); enum_it != generic_enums_.end()) {
                return instantiateGenericEnum(named->name, resolved_args);
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
    
    // 生成函数体
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*context_, "entry", func);
    auto old_insert_point = builder_->saveIP();
    builder_->SetInsertPoint(bb);
    
    // 保存旧的named_values
    auto old_named_values = named_values_;
    auto old_variable_types = variable_types_;
    auto old_array_element_types = array_element_types_;
    auto old_array_param_sizes = array_param_sizes_;
    
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
            
            // 记录元素类型和数组大小
            const ArrayTypeNode* array_type = static_cast<const ArrayTypeNode*>(param.type.get());
            llvm::Type* elem_type = resolveGenericType(array_type->element_type.get());
            array_element_types_[param.name] = elem_type;
            
            // 【关键修复】：记录数组大小
            if (array_type->size >= 0) {
                array_param_sizes_[param.name] = array_type->size;
            }
        } else if (param.type->kind == Type::Kind::Optional) {
            // 【新增】：T?参数也是指针
            llvm::Type* param_type = resolveGenericType(param.type.get());
            // T?参数：arg是ptr
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                llvm::PointerType::get(*context_, 0), nullptr, param.name
            );
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
    array_param_sizes_ = old_array_param_sizes;
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
        // 重建struct或enum定义（根据AST节点重新生成类型）
        const Stmt* stmt = static_cast<const Stmt*>(symbol->ast_node);
        if (stmt->kind == Stmt::Kind::Struct) {
            generateStructStmt(static_cast<const StructStmt*>(stmt));
        } else if (stmt->kind == Stmt::Kind::Enum) {
            generateEnumStmt(static_cast<const EnumStmt*>(stmt));
        }
    }
}

} // namespace pawc
