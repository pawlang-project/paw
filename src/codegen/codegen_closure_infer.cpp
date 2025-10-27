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
    
    // 策略 1: 从 expected_fn_type 推导
    // let f: fn(i32, i32) -> i32 = (x, y) -> { x + y }
    if (expr->expected_fn_type) {
        // TODO: 解析函数类型的参数
        // 目前简化：假设 expected_fn_type 只能从 Let 语句的类型标注得到
        // 实际需要完整的类型系统支持
        
        std::cerr << "[INFO] Expected function type provided for closure, but type parsing not yet implemented\n";
    }
    
    // 策略 2: 从闭包存储的额外信息推导
    // （需要在Parser阶段传递）
    
    // Phase 3: 暂时无法推导
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

