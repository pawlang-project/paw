//===--- type_inference.cpp - Type Inference Implementation -----*- C++ -*-===//

#include "type_inference.h"
#include "frontend/parser/ast/expr.h"
#include "middleend/types/type.h"
#include "middleend/types/primitive_types.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/type_system.h"
#include <iostream>

namespace pawc {

TypeInference::TypeInference(SemanticContext* context)
    : context_(context) {}

Type* TypeInference::inferType(Expr* expr) {
    // 如果表达式已有类型，直接返回
    if (expr->getType()) {
        return expr->getType();
    }
    
    // 根据表达式类型推导
    if (auto* lit = dynamic_cast<IntLiteral*>(expr)) {
        return context_->getTypeSystem()->getI32Type();
    }
    if (auto* lit = dynamic_cast<FloatLiteral*>(expr)) {
        return context_->getTypeSystem()->getF64Type();
    }
    if (auto* lit = dynamic_cast<BoolLiteral*>(expr)) {
        return context_->getTypeSystem()->getBoolType();
    }
    if (auto* lit = dynamic_cast<CharLiteral*>(expr)) {
        return context_->getTypeSystem()->getCharType();
    }
    if (auto* lit = dynamic_cast<StringLiteral*>(expr)) {
        return context_->getTypeSystem()->getStringType();
    }
    if (auto* lit = dynamic_cast<NoneLiteral*>(expr)) {
        // none 默认为 Optional<void>
        return context_->getTypeSystem()->getOptionalType(
            context_->getTypeSystem()->getVoidType()
        );
    }
    
    if (auto* arr = dynamic_cast<ArrayLiteral*>(expr)) {
        return inferFromArrayLiteral(arr);
    }
    if (auto* tuple = dynamic_cast<TupleExpr*>(expr)) {
        return inferFromTuple(tuple);
    }
    if (auto* if_expr = dynamic_cast<IfExpr*>(expr)) {
        return inferFromIfExpr(if_expr);
    }
    if (auto* struct_lit = dynamic_cast<StructLiteral*>(expr)) {
        return inferFromStructLiteral(struct_lit);
    }
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        return inferFromCallExpr(call);
    }
    if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        return inferFromBinaryOp(binary->getLeft(), binary->getRight(), "");
    }
    
    return nullptr;
}

Type* TypeInference::inferFromLiteral(Expr* literal) {
    // 字面量类型推导
    if (dynamic_cast<IntLiteral*>(literal)) {
        return context_->getTypeSystem()->getI32Type();
    }
    if (dynamic_cast<FloatLiteral*>(literal)) {
        return context_->getTypeSystem()->getF64Type();
    }
    if (dynamic_cast<BoolLiteral*>(literal)) {
        return context_->getTypeSystem()->getBoolType();
    }
    if (dynamic_cast<CharLiteral*>(literal)) {
        return context_->getTypeSystem()->getCharType();
    }
    if (dynamic_cast<StringLiteral*>(literal)) {
        return context_->getTypeSystem()->getStringType();
    }
    if (dynamic_cast<NoneLiteral*>(literal)) {
        return context_->getTypeSystem()->getOptionalType(
            context_->getTypeSystem()->getVoidType()
        );
    }
    
    return nullptr;
}

Type* TypeInference::inferFromBinaryOp(Expr* left, Expr* right, const std::string& op) {
    // 推导左右操作数的类型
    Type* left_type = inferType(left);
    Type* right_type = inferType(right);
    
    if (!left_type || !right_type) {
        return nullptr;
    }
    
    // 算术运算：返回操作数类型（假设类型一致）
    // +, -, *, /, %
    if (left_type->isNumeric() && right_type->isNumeric()) {
        // 简化：返回左操作数类型
        // TODO: 实现更精确的类型提升规则（i32 + f64 -> f64）
        return left_type;
    }
    
    // 比较运算：返回 bool
    // ==, !=, <, >, <=, >=
    return context_->getTypeSystem()->getBoolType();
}

Type* TypeInference::inferFromArrayLiteral(ArrayLiteral* arr) {
    const auto& elements = arr->getElements();
    
    if (elements.empty()) {
        // 空数组：无法推导，返回 void 数组
        return context_->getTypeSystem()->getArrayType(
            context_->getTypeSystem()->getVoidType(),
            0
        );
    }
    
    // 从第一个元素推导类型
    Type* elem_type = inferType(elements[0].get());
    
    if (!elem_type) {
        return nullptr;
    }
    
    // 创建数组类型
    return context_->getTypeSystem()->getArrayType(elem_type, elements.size());
}

Type* TypeInference::inferFromTuple(TupleExpr* tuple) {
    const auto& elements = tuple->getElements();
    std::vector<Type*> element_types;
    
    for (const auto& elem : elements) {
        Type* elem_type = inferType(elem.get());
        if (!elem_type) {
            return nullptr;
        }
        element_types.push_back(elem_type);
    }
    
    return context_->getTypeSystem()->getTupleType(element_types);
}

Type* TypeInference::inferFromIfExpr(IfExpr* if_expr) {
    // 推导 then 和 else 分支的类型
    Type* then_type = nullptr;
    Type* else_type = nullptr;
    
    if (if_expr->getThenExpr()) {
        then_type = inferType(if_expr->getThenExpr());
    }
    
    if (if_expr->getElseExpr()) {
        else_type = inferType(if_expr->getElseExpr());
    }
    
    // 如果两个分支类型一致，返回该类型
    if (then_type && else_type) {
        return inferCommonType(then_type, else_type);
    }
    
    // 否则返回其中一个非空类型
    return then_type ? then_type : else_type;
}

Type* TypeInference::inferFromStructLiteral(StructLiteral* struct_lit) {
    // 从结构体名称查找类型
    const std::string& struct_name = struct_lit->getStructName();
    return context_->getTypeSystem()->lookupType(struct_name);
}

Type* TypeInference::inferFromCallExpr(CallExpr* call) {
    // 从callee推导返回类型
    Expr* callee = call->getCallee();
    
    if (!callee) {
        return nullptr;
    }
    
    Type* callee_type = inferType(callee);
    
    if (!callee_type) {
        return nullptr;
    }
    
    // 如果callee是函数类型，返回其返回类型
    if (callee_type->isFunction()) {
        FunctionType* func_type = static_cast<FunctionType*>(callee_type);
        return func_type->getReturnType();
    }
    
    return nullptr;
}

Type* TypeInference::inferCommonType(Type* t1, Type* t2) {
    // 如果两个类型相同，返回该类型
    if (t1 && t2 && t1->equals(t2)) {
        return t1;
    }
    
    // 数值类型提升规则
    if (t1 && t2 && t1->isNumeric() && t2->isNumeric()) {
        // i32 + f64 -> f64 (浮点优先)
        if (t1->isFloat() || t2->isFloat()) {
            return context_->getTypeSystem()->getF64Type();
        }
        // 默认 i32（简化：不区分 i32/i64）
        return context_->getTypeSystem()->getI32Type();
    }
    
    // 默认返回第一个类型
    return t1 ? t1 : t2;
}

} // namespace pawc

