/**
 * @file interface_validator.cpp
 * @brief 接口验证器实现
 */

#include "interface_validator.h"

namespace pawc {

InterfaceValidator::InterfaceValidator(
    TypeSystem* type_system,
    DiagnosticEngine* diagnostics,
    const std::map<std::string, const InterfaceStmt*>& interface_defs)
    : type_system_(type_system),
      diagnostics_(diagnostics),
      interface_defs_(interface_defs) {}

bool InterfaceValidator::validateImpl(
    const std::string& type_name,
    const std::string& interface_name,
    const std::vector<std::unique_ptr<FunctionStmt>>& methods,
    const SourceLocation& location) {
    
    // 查找接口定义
    auto interface_it = interface_defs_.find(interface_name);
    if (interface_it == interface_defs_.end()) {
        DiagnosticLocation loc(location.filename, location.line, location.column);
        diagnostics_->reportError(
            "Interface '" + interface_name + "' not found",
            loc
        );
        return false;
    }
    
    const InterfaceStmt* interface = interface_it->second;
    
    // 创建已实现方法的映射
    std::map<std::string, const FunctionStmt*> impl_methods;
    for (const auto& method : methods) {
        impl_methods[method->name] = method.get();
    }
    
    bool all_valid = true;
    
    // 检查所有接口方法是否都已实现
    for (const auto& interface_method : interface->methods) {
        auto impl_it = impl_methods.find(interface_method.name);
        
        // 检查方法是否存在
        if (impl_it == impl_methods.end()) {
            // 如果接口提供了默认实现，则不要求必须实现
            if (interface_method.default_body != nullptr) {
                continue;  // 有默认实现，跳过检查
            }
            
            DiagnosticLocation loc(location.filename, location.line, location.column);
            diagnostics_->reportError(
                "Type '" + type_name + "' does not implement method '" + 
                interface_method.name + "' from interface '" + interface_name + "'",
                loc
            );
            all_valid = false;
            continue;
        }
        
        const FunctionStmt* impl_method = impl_it->second;
        
        // 检查参数数量
        if (impl_method->parameters.size() != interface_method.parameters.size()) {
            DiagnosticLocation loc(impl_method->location.filename, 
                                  impl_method->location.line, 
                                  impl_method->location.column);
            diagnostics_->reportError(
                "Method '" + interface_method.name + "' has wrong number of parameters. " +
                "Expected " + std::to_string(interface_method.parameters.size()) + 
                ", got " + std::to_string(impl_method->parameters.size()),
                loc
            );
            all_valid = false;
            continue;
        }
        
        // 检查参数类型
        for (size_t i = 0; i < interface_method.parameters.size(); ++i) {
            const Type* expected_type = interface_method.parameters[i].type.get();
            const Type* actual_type = impl_method->parameters[i].type.get();
            
            if (!compareTypes(expected_type, actual_type)) {
                DiagnosticLocation loc(impl_method->location.filename,
                                      impl_method->location.line,
                                      impl_method->location.column);
                diagnostics_->reportError(
                    "Parameter '" + interface_method.parameters[i].name + 
                    "' has wrong type in method '" + interface_method.name + "'. " +
                    "Expected: " + typeToString(expected_type) + ", " +
                    "got: " + typeToString(actual_type),
                    loc
                );
                all_valid = false;
            }
        }
        
        // 检查返回类型
        const Type* expected_return = interface_method.return_type.get();
        const Type* actual_return = impl_method->return_type.get();
        
        if (!compareTypes(expected_return, actual_return)) {
            DiagnosticLocation loc(impl_method->location.filename,
                                  impl_method->location.line,
                                  impl_method->location.column);
            diagnostics_->reportError(
                "Method '" + interface_method.name + "' has wrong return type. " +
                "Expected: " + typeToString(expected_return) + ", " +
                "got: " + typeToString(actual_return),
                loc
            );
            all_valid = false;
        }
    }
    
    return all_valid;
}

bool InterfaceValidator::compareTypes(const Type* a, const Type* b) {
    // 使用新类型系统比较
    types::Type* type_a = nullptr;
    types::Type* type_b = nullptr;
    
    // 简化实现：使用 TypeSystem 的转换（需要在 CodeGenerator 中暴露 convertASTType）
    // 暂时使用简单的结构比较
    if (a == nullptr && b == nullptr) return true;
    if (a == nullptr || b == nullptr) return false;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case Type::Kind::Primitive: {
            auto prim_a = static_cast<const PrimitiveTypeNode*>(a);
            auto prim_b = static_cast<const PrimitiveTypeNode*>(b);
            return prim_a->prim_type == prim_b->prim_type;
        }
        case Type::Kind::Named: {
            auto named_a = static_cast<const NamedTypeNode*>(a);
            auto named_b = static_cast<const NamedTypeNode*>(b);
            return named_a->name == named_b->name;
        }
        case Type::Kind::SelfType:
            return true;
        case Type::Kind::Generic: {
            auto gen_a = static_cast<const GenericTypeNode*>(a);
            auto gen_b = static_cast<const GenericTypeNode*>(b);
            return gen_a->name == gen_b->name;
        }
        case Type::Kind::Reference: {
            auto ref_a = static_cast<const ReferenceTypeNode*>(a);
            auto ref_b = static_cast<const ReferenceTypeNode*>(b);
            return ref_a->is_mutable == ref_b->is_mutable &&
                   compareTypes(ref_a->inner_type.get(), ref_b->inner_type.get());
        }
        case Type::Kind::Optional: {
            auto opt_a = static_cast<const OptionalTypeNode*>(a);
            auto opt_b = static_cast<const OptionalTypeNode*>(b);
            return compareTypes(opt_a->inner_type.get(), opt_b->inner_type.get());
        }
        case Type::Kind::Array: {
            auto arr_a = static_cast<const ArrayTypeNode*>(a);
            auto arr_b = static_cast<const ArrayTypeNode*>(b);
            return arr_a->size == arr_b->size &&
                   compareTypes(arr_a->element_type.get(), arr_b->element_type.get());
        }
        case Type::Kind::Slice: {
            auto slice_a = static_cast<const SliceTypeNode*>(a);
            auto slice_b = static_cast<const SliceTypeNode*>(b);
            return compareTypes(slice_a->element_type.get(), slice_b->element_type.get());
        }
        case Type::Kind::Tuple: {
            auto tuple_a = static_cast<const TupleTypeNode*>(a);
            auto tuple_b = static_cast<const TupleTypeNode*>(b);
            if (tuple_a->element_types.size() != tuple_b->element_types.size()) return false;
            for (size_t i = 0; i < tuple_a->element_types.size(); ++i) {
                if (!compareTypes(tuple_a->element_types[i].get(), tuple_b->element_types[i].get())) {
                    return false;
                }
            }
            return true;
        }
        default:
            return false;
    }
}

std::string InterfaceValidator::typeToString(const Type* type) {
    if (!type) return "void";
    
    switch (type->kind) {
        case Type::Kind::Primitive: {
            auto prim = static_cast<const PrimitiveTypeNode*>(type);
            switch (prim->prim_type) {
                case PrimitiveType::I8: return "i8";
                case PrimitiveType::I16: return "i16";
                case PrimitiveType::I32: return "i32";
                case PrimitiveType::I64: return "i64";
                case PrimitiveType::I128: return "i128";
                case PrimitiveType::U8: return "u8";
                case PrimitiveType::U16: return "u16";
                case PrimitiveType::U32: return "u32";
                case PrimitiveType::U64: return "u64";
                case PrimitiveType::U128: return "u128";
                case PrimitiveType::F32: return "f32";
                case PrimitiveType::F64: return "f64";
                case PrimitiveType::BOOL: return "bool";
                case PrimitiveType::CHAR: return "char";
                case PrimitiveType::STRING: return "string";
                case PrimitiveType::VOID: return "void";
            }
            return "unknown";
        }
        case Type::Kind::Named: {
            auto named = static_cast<const NamedTypeNode*>(type);
            return named->name;
        }
        case Type::Kind::SelfType:
            return "Self";
        case Type::Kind::Generic: {
            auto gen = static_cast<const GenericTypeNode*>(type);
            return gen->name;
        }
        case Type::Kind::Reference: {
            auto ref = static_cast<const ReferenceTypeNode*>(type);
            return std::string("&") + (ref->is_mutable ? "mut " : "") + 
                   typeToString(ref->inner_type.get());
        }
        case Type::Kind::Optional: {
            auto opt = static_cast<const OptionalTypeNode*>(type);
            return typeToString(opt->inner_type.get()) + "?";
        }
        case Type::Kind::Array: {
            auto arr = static_cast<const ArrayTypeNode*>(type);
            return "[" + typeToString(arr->element_type.get()) + "; " + 
                   std::to_string(arr->size) + "]";
        }
        case Type::Kind::Slice: {
            auto slice = static_cast<const SliceTypeNode*>(type);
            return "[" + typeToString(slice->element_type.get()) + "]";
        }
        case Type::Kind::Tuple: {
            auto tuple = static_cast<const TupleTypeNode*>(type);
            std::string result = "(";
            for (size_t i = 0; i < tuple->element_types.size(); ++i) {
                if (i > 0) result += ", ";
                result += typeToString(tuple->element_types[i].get());
            }
            result += ")";
            return result;
        }
        default:
            return "unknown";
    }
}

} // namespace pawc

