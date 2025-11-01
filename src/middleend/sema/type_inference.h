//===--- type_inference.h - Type Inference ----------------------*- C++ -*-===//
//
// 类型推导 - 从表达式推导类型
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_INFERENCE_H
#define PAW_TYPE_INFERENCE_H

#include "semantic_context.h"

namespace pawc {

class Expr;
class Type;

/// TypeInference - 类型推导器
///
/// 负责从表达式推导类型，支持：
/// - 字面量类型推导
/// - 二元运算类型推导
/// - 函数调用返回类型推导
/// - 泛型类型参数推导
class TypeInference {
    SemanticContext* context_;
    
public:
    explicit TypeInference(SemanticContext* context);
    
    /// 从表达式推导类型
    Type* inferType(Expr* expr);
    
    /// 从字面量推导
    Type* inferFromLiteral(Expr* literal);
    
    /// 从二元运算推导
    Type* inferFromBinaryOp(Expr* left, Expr* right, const std::string& op);
    
    /// 推导公共类型（用于if表达式等）
    Type* inferCommonType(Type* t1, Type* t2);
};

} // namespace pawc

#endif // PAW_TYPE_INFERENCE_H

