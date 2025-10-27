/**
 * @file codegen_closure_infer.cpp
 * @brief 闭包类型推导实现
 */

#include "codegen.h"
#include "../parser/ast.h"
#include <iostream>

namespace pawc {

llvm::Type* CodeGenerator::deduceClosureParamType(const ClosureExpr* expr, size_t param_idx) {
    // Phase 3: 从期望类型推导参数类型
    
    // 策略: 从 expected_fn_type 推导
    // let f: fn(i32, i32) -> i32 = (x, y) -> { x + y };
    if (expr->expected_fn_type) {
        // 检查类型种类
        if (expr->expected_fn_type->kind == Type::Kind::Function) {
            // ✅ 真正的函数类型！
            const FunctionTypeNode* func_type = static_cast<const FunctionTypeNode*>(expr->expected_fn_type);
            
            // 检查参数索引是否有效
            if (param_idx < func_type->param_types.size()) {
                const Type* param_type = func_type->param_types[param_idx].get();
                llvm::Type* llvm_type = convertType(param_type);
                if (llvm_type) {
                    std::cerr << "[INFO] ✅ Successfully deduced parameter " << param_idx 
                              << " type from fn(...) type\n";
                    return llvm_type;
                }
            }
        } else if (expr->expected_fn_type->kind == Type::Kind::Named) {
            // 旧方案：Named 类型（为兼容性保留）
            const NamedTypeNode* named = static_cast<const NamedTypeNode*>(expr->expected_fn_type);
            
            std::cerr << "[DEBUG] Trying to deduce param " << param_idx 
                      << " from named type: " << named->name << std::endl;
                      
            // 检查是否有泛型参数（这些可能是函数参数和返回类型）
            if (!named->generic_args.empty() && param_idx < named->generic_args.size()) {
                // 假设前N个generic_args是参数类型
                const Type* param_type = named->generic_args[param_idx].get();
                llvm::Type* llvm_type = convertType(param_type);
                if (llvm_type) {
                    std::cerr << "[INFO] Successfully deduced parameter type from generic args\n";
                    return llvm_type;
                }
            }
        }
    }
    
    // 无法推导
    std::cerr << "[WARN] Cannot deduce parameter type for closure parameter " << param_idx << std::endl;
    return nullptr;
}

void CodeGenerator::setClosureExpectedType(ClosureExpr* closure, const Type* expected_type) {
    // 将期望类型传递给闭包
    if (expected_type && expected_type->kind == Type::Kind::Function) {
        // 完整的函数类型
        closure->expected_fn_type = expected_type;
    } else if (expected_type && expected_type->kind == Type::Kind::Named) {
        // 命名类型
        closure->expected_fn_type = expected_type;
    }
}

} // namespace pawc
