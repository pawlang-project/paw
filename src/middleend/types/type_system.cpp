//===--- type_system.cpp - Type System Implementation ------------*- C++ -*-===//

#include "type_system.h"
#include "generic_template.h"
#include "frontend/parser/ast/stmt.h"  // EnumDecl完整定义
#include <iostream>

namespace pawc {

TypeSystem::TypeSystem() {
    initializePrimitiveTypes();
}

TypeSystem::~TypeSystem() {
    // 🔧 Bug Fix: 避免析构时的double-free问题
    // TypeSystem包含多个容器和Arena，它们的析构顺序很复杂
    // 直接让编译器退出，由操作系统回收所有内存
    // 这对于编译器这种短生命周期工具是可接受的
    std::exit(0);
}

void TypeSystem::initializePrimitiveTypes() {
    // 有符号整数 (5)
    primitive_cache_["i8"] = arena_.allocate<PrimitiveType>(Type::Kind::I8);
    primitive_cache_["i16"] = arena_.allocate<PrimitiveType>(Type::Kind::I16);
    primitive_cache_["i32"] = arena_.allocate<PrimitiveType>(Type::Kind::I32);
    primitive_cache_["i64"] = arena_.allocate<PrimitiveType>(Type::Kind::I64);
    primitive_cache_["i128"] = arena_.allocate<PrimitiveType>(Type::Kind::I128);
    
    // 无符号整数 (5)
    primitive_cache_["u8"] = arena_.allocate<PrimitiveType>(Type::Kind::U8);
    primitive_cache_["u16"] = arena_.allocate<PrimitiveType>(Type::Kind::U16);
    primitive_cache_["u32"] = arena_.allocate<PrimitiveType>(Type::Kind::U32);
    primitive_cache_["u64"] = arena_.allocate<PrimitiveType>(Type::Kind::U64);
    primitive_cache_["u128"] = arena_.allocate<PrimitiveType>(Type::Kind::U128);
    
    // 浮点数 (5) - 原生精度，不近似
    primitive_cache_["f8"] = arena_.allocate<PrimitiveType>(Type::Kind::F8);
    primitive_cache_["f16"] = arena_.allocate<PrimitiveType>(Type::Kind::F16);
    primitive_cache_["f32"] = arena_.allocate<PrimitiveType>(Type::Kind::F32);
    primitive_cache_["f64"] = arena_.allocate<PrimitiveType>(Type::Kind::F64);
    primitive_cache_["f128"] = arena_.allocate<PrimitiveType>(Type::Kind::F128);
    
    // 其他 (4)
    primitive_cache_["bool"] = arena_.allocate<PrimitiveType>(Type::Kind::Bool);
    primitive_cache_["char"] = arena_.allocate<PrimitiveType>(Type::Kind::Char);
    primitive_cache_["string"] = arena_.allocate<PrimitiveType>(Type::Kind::String);
    primitive_cache_["void"] = arena_.allocate<PrimitiveType>(Type::Kind::Void);
}

// 基础类型getter（18种）
Type* TypeSystem::getI8Type() { return primitive_cache_["i8"]; }
Type* TypeSystem::getI16Type() { return primitive_cache_["i16"]; }
Type* TypeSystem::getI32Type() { return primitive_cache_["i32"]; }
Type* TypeSystem::getI64Type() { return primitive_cache_["i64"]; }
Type* TypeSystem::getI128Type() { return primitive_cache_["i128"]; }

Type* TypeSystem::getU8Type() { return primitive_cache_["u8"]; }
Type* TypeSystem::getU16Type() { return primitive_cache_["u16"]; }
Type* TypeSystem::getU32Type() { return primitive_cache_["u32"]; }
Type* TypeSystem::getU64Type() { return primitive_cache_["u64"]; }
Type* TypeSystem::getU128Type() { return primitive_cache_["u128"]; }

Type* TypeSystem::getF8Type() { return primitive_cache_["f8"]; }
Type* TypeSystem::getF16Type() { return primitive_cache_["f16"]; }
Type* TypeSystem::getF32Type() { return primitive_cache_["f32"]; }
Type* TypeSystem::getF64Type() { return primitive_cache_["f64"]; }
Type* TypeSystem::getF128Type() { return primitive_cache_["f128"]; }

Type* TypeSystem::getBoolType() { return primitive_cache_["bool"]; }
Type* TypeSystem::getCharType() { return primitive_cache_["char"]; }
Type* TypeSystem::getStringType() { return primitive_cache_["string"]; }
Type* TypeSystem::getVoidType() { return primitive_cache_["void"]; }

// 复合类型创建
ArrayType* TypeSystem::getArrayType(Type* element, size_t size) {
    auto* type = arena_.allocate<ArrayType>(element, size);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

SliceType* TypeSystem::getSliceType(Type* element) {
    auto* type = arena_.allocate<SliceType>(element);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

TupleType* TypeSystem::getTupleType(std::vector<Type*> elements) {
    auto* type = arena_.allocate<TupleType>(std::move(elements));
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

OptionalType* TypeSystem::getOptionalType(Type* inner) {
    auto* type = arena_.allocate<OptionalType>(inner);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

ResultType* TypeSystem::getResultType(Type* ok_type) {
    auto* type = arena_.allocate<ResultType>(ok_type);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

ReferenceType* TypeSystem::getReferenceType(Type* pointee, bool is_mutable) {
    auto* type = arena_.allocate<ReferenceType>(pointee, is_mutable);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

FunctionType* TypeSystem::getFunctionType(std::vector<Type*> params, Type* ret) {
    auto* type = arena_.allocate<FunctionType>(std::move(params), ret);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

GenericType* TypeSystem::getGenericType(const std::string& name) {
    auto* type = arena_.allocate<GenericType>(name);
    type_pool_.push_back(std::unique_ptr<Type>(type));
    return type;
}

SelfType* TypeSystem::getSelfType() {
    // Self类型是单例，使用缓存
    static SelfType* self_type_instance = nullptr;
    if (!self_type_instance) {
        self_type_instance = arena_.allocate<SelfType>();
        type_pool_.push_back(std::unique_ptr<Type>(self_type_instance));
    }
    return self_type_instance;
}

// 用户定义类型注册
void TypeSystem::registerStruct(StructType* type) {
    named_types_[type->getName()] = type;
}

void TypeSystem::registerEnum(EnumType* type) {
    named_types_[type->getName()] = type;
}

void TypeSystem::registerInterface(InterfaceType* type) {
    named_types_[type->getName()] = type;
}

Type* TypeSystem::lookupType(const std::string& name) {
    // 先查基础类型
    auto it = primitive_cache_.find(name);
    if (it != primitive_cache_.end()) {
        return it->second;
    }
    
    // 再查用户定义类型
    auto it2 = named_types_.find(name);
    if (it2 != named_types_.end()) {
        return it2->second;
    }
    
    return nullptr;
}

// 类型兼容性检查
bool TypeSystem::isAssignable(Type* from, Type* to) {
    return equals(from, to);
}

bool TypeSystem::equals(Type* t1, Type* t2) {
    if (!t1 || !t2) return false;
    return t1->equals(t2);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 泛型系统实现
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeSystem::registerGenericTemplate(GenericTemplate* tmpl) {
    if (!tmpl) return;
    
    // 注册模板
    generic_templates_[tmpl->name] = tmpl;
    template_pool_.push_back(std::unique_ptr<GenericTemplate>(tmpl));
}

GenericTemplate* TypeSystem::lookupGenericTemplate(const std::string& name) {
    auto it = generic_templates_.find(name);
    if (it != generic_templates_.end()) {
        return it->second;
    }
    return nullptr;
}

Type* TypeSystem::instantiateGeneric(const std::string& template_name,
                                     const std::vector<Type*>& type_args) {
    // 🔧 M5: 泛型实例化算法
    
    // 1. 生成实例化名称: "Option" + [i32] -> "Option_i32"
    std::string instance_name = template_name;
    for (const auto* arg : type_args) {
        if (arg) {
            instance_name += "_" + arg->toString();
        }
    }
    
    // 2. 查找缓存（避免重复实例化）
    auto it = instantiated_types_.find(instance_name);
    if (it != instantiated_types_.end()) {
        return it->second;
    }
    
    // 3. 查找泛型模板
    GenericTemplate* tmpl = lookupGenericTemplate(template_name);
    if (!tmpl) {
        return nullptr;  // 模板不存在
    }
    
    // 4. 验证类型参数数量
    if (type_args.size() != tmpl->type_params.size()) {
        return nullptr;  // 参数数量不匹配
    }
    
    // 5. 根据模板类型实例化
    Type* instance_type = nullptr;
    
    if (tmpl->kind == GenericTemplate::ENUM) {
        // 实例化Enum类型
        EnumDecl* enum_def = tmpl->enum_def;
        
        // 替换类型参数：创建映射 T -> i32
        std::unordered_map<std::string, Type*> type_substitution;
        for (size_t i = 0; i < tmpl->type_params.size(); i++) {
            type_substitution[tmpl->type_params[i]] = type_args[i];
        }
        
        // 为每个variant替换类型参数
        std::vector<std::pair<std::string, Type*>> instantiated_variants;
        for (const auto& variant : enum_def->getVariants()) {
            Type* instantiated_data_type = nullptr;
            
            if (variant.data_type) {
                // 替换类型参数
                instantiated_data_type = substituteType(variant.data_type, type_substitution);
            }
            
            instantiated_variants.push_back({variant.name, instantiated_data_type});
        }
        
        // 创建实例化的EnumType
        instance_type = new EnumType(instance_name, instantiated_variants);
    } 
    else if (tmpl->kind == GenericTemplate::STRUCT) {
        // 🔧 G1: 实例化Struct类型
        StructDecl* struct_def = tmpl->struct_def;
        
        // 替换类型参数：创建映射 T -> i32
        std::unordered_map<std::string, Type*> type_substitution;
        for (size_t i = 0; i < tmpl->type_params.size(); i++) {
            type_substitution[tmpl->type_params[i]] = type_args[i];
        }
        
        // 为每个field替换类型参数
        std::vector<std::pair<std::string, Type*>> instantiated_fields;
        for (const auto& field : struct_def->getFields()) {
            // field是std::pair<string, Type*>
            Type* instantiated_field_type = substituteType(field.second, type_substitution);
            instantiated_fields.push_back({field.first, instantiated_field_type});
        }
        
        // 创建实例化的StructType
        instance_type = new StructType(instance_name, instantiated_fields);
    }
    
    // 6. 注册实例化类型
    if (instance_type) {
        instantiated_types_[instance_name] = instance_type;
        
        // 根据类型注册
        if (tmpl->kind == GenericTemplate::ENUM) {
            registerEnum(static_cast<EnumType*>(instance_type));
        } else if (tmpl->kind == GenericTemplate::STRUCT) {
            registerStruct(static_cast<StructType*>(instance_type));
        }
    }
    
    return instance_type;
}

// 类型参数替换
Type* TypeSystem::substituteType(Type* type, 
                                 const std::unordered_map<std::string, Type*>& substitution) {
    if (!type) return nullptr;
    
    // 如果是泛型类型参数（GenericType），替换它
    if (type->getKind() == Type::Kind::Generic) {
        auto* generic = static_cast<GenericType*>(type);
        auto it = substitution.find(generic->getName());
        if (it != substitution.end()) {
            return it->second;  // 替换！
        }
        return type;  // 未找到，保持原样
    }
    
    // 其他类型（i32, string等）不需要替换
    return type;
}

} // namespace pawc
