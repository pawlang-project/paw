//===--- type_inference.cpp - Type Inference Implementation -----*- C++ -*-===//
/// @file type_inference.cpp
/// @brief Type system and semantic analysis implementation

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
    // ifexpressionalreadyhastypes，directlyreturn
    if (expr->getType()) {
        return expr->getType();
    }
    
    // according toexpressiontypesinfer
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
        // none defaultis/as Optional<void>
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
    // literaltypesinfer
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
    // inferleftrightoperationnumberof/thetypes
    Type* left_type = inferType(left);
    Type* right_type = inferType(right);
    
    if (!left_type || !right_type) {
        return nullptr;
    }
    
    // arithmeticoperation：returnoperationnumbertypes（assumptiontypesconsistent）
    // +, -, *, /, %
    if (left_type->isNumeric() && right_type->isNumeric()) {
        // simplify：returnleftoperationnumbertypes
        // TODO: implementationmoreprecise/exactof/thetypesimproverule（i32 + f64 -> f64）
        return left_type;
    }
    
    // compareoperation：return bool
    // ==, !=, <, >, <=, >=
    return context_->getTypeSystem()->getBoolType();
}

Type* TypeInference::inferFromArrayLiteral(ArrayLiteral* arr) {
    const auto& elements = arr->getElements();
    
    if (elements.empty()) {
        // emptyarray：noway/cannotinfer，return void array
        return context_->getTypeSystem()->getArrayType(
            context_->getTypeSystem()->getVoidType(),
            0
        );
    }
    
    // fromfirstelementinfertypes
    Type* elem_type = inferType(elements[0].get());
    
    if (!elem_type) {
        return nullptr;
    }
    
    // createarraytypes
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
    // infer then and else branchof/thetypes
    Type* then_type = nullptr;
    Type* else_type = nullptr;
    
    if (if_expr->getThenExpr()) {
        then_type = inferType(if_expr->getThenExpr());
    }
    
    if (if_expr->getElseExpr()) {
        else_type = inferType(if_expr->getElseExpr());
    }
    
    // iftwoindividual/piecebranchtypesconsistent，returnshouldtypes
    if (then_type && else_type) {
        return inferCommonType(then_type, else_type);
    }
    
    // nootherwisereturnitsmiddle/centerone/aindividual/piecenotemptytypes
    return then_type ? then_type : else_type;
}

Type* TypeInference::inferFromStructLiteral(StructLiteral* struct_lit) {
    // fromstructbody/structnamelookuptypes
    const std::string& struct_name = struct_lit->getStructName();
    return context_->getTypeSystem()->lookupType(struct_name);
}

Type* TypeInference::inferFromCallExpr(CallExpr* call) {
    // fromcalleeinferreturntypes
    Expr* callee = call->getCallee();
    
    if (!callee) {
        return nullptr;
    }
    
    Type* callee_type = inferType(callee);
    
    if (!callee_type) {
        return nullptr;
    }
    
    // ifcalleeyesfunctiontypes，returnitsreturntypes
    if (callee_type->isFunction()) {
        FunctionType* func_type = static_cast<FunctionType*>(callee_type);
        return func_type->getReturnType();
    }
    
    return nullptr;
}

Type* TypeInference::inferCommonType(Type* t1, Type* t2) {
    // iftwoindividual/piecetypessame，returnshouldtypes
    if (t1 && t2 && t1->equals(t2)) {
        return t1;
    }
    
    // numbervaluetypesimproverule
    if (t1 && t2 && t1->isNumeric() && t2->isNumeric()) {
        // i32 + f64 -> f64 (floating-point takes precedence)
        if (t1->isFloat() || t2->isFloat()) {
            return context_->getTypeSystem()->getF64Type();
        }
        // Default i32 (simplified: don't distinguish i32/i64)
        return context_->getTypeSystem()->getI32Type();
    }
    
    // defaultreturnfirsttypes
    return t1 ? t1 : t2;
}

} // namespace pawc

