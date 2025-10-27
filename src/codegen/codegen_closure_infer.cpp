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
    // let f: fn(i32, i32) -> i32 = (x, y) -> { x + y }
    if (expr->expected_fn_type) {
        // 检查类型种类
        if (expr->expected_fn_type->kind == Type::Kind::Named) {
            const NamedTypeNode* named = static_cast<const NamedTypeNode*>(expr->expected_fn_type);
            
            // PawLang的函数类型表示为：名为"fn"的NamedType，带泛型参数
            // 例如：fn(i32, i32) -> i32 在AST中可能表示为 Named("fn", [i32, i32, i32])
            // 但实际上我们当前没有这样的表示...
            
            // 让我们采用简化方案：
            // 要求用户在类型标注中使用具体的类型别名
            // 或者我们解析 generic_args
            
            std::cerr << "[DEBUG] Trying to deduce param " << param_idx 
                      << " from expected type: " << named->name << std::endl;
                      
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
        
        // 其他类型（如Function类型，如果我们添加了的话）
        // TODO: 实现完整的函数类型支持
    }
    
    // 无法推导
    return nullptr;
}

void CodeGenerator::setClosureExpectedType(ClosureExpr* closure, const Type* expected_type) {
    // 将期望类型传递给闭包
    if (expected_type && expected_type->kind == Type::Kind::Named) {
        // 检查是否是函数类型（fn(...)）
        // TODO: 完整实现
    }
}

} // namespace pawc

