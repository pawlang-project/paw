//===--- type_inference.h - Type Inference ----------------------*- C++ -*-===//
//
// typeinfer - fromexpressioninfertypes
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_INFERENCE_H
#define PAW_TYPE_INFERENCE_H

#include "semantic_context.h"

namespace pawc {

class Expr;
class Type;

/// TypeInference - typesinferlinker/er
///
/// negativeresponsiblefromexpressioninfertypes，support：
/// - literaltypesinfer
/// - binary operationstypesinfer
/// - functioncallreturntypesinfer
/// - arrayandtupletypesinfer
/// - structbody/structliteraltypesinfer
class TypeInference {
    SemanticContext* context_;
    
public:
    explicit TypeInference(SemanticContext* context);
    
    /// fromexpressioninfertypes（mainentry point）
    Type* inferType(Expr* expr);
    
    /// fromliteralinfer
    Type* inferFromLiteral(Expr* literal);
    
    /// frombinary operationsinfer
    Type* inferFromBinaryOp(Expr* left, Expr* right, const std::string& op);
    
    /// fromarrayliteralinfer
    Type* inferFromArrayLiteral(class ArrayLiteral* arr);
    
    /// fromtupleinfer
    Type* inferFromTuple(class TupleExpr* tuple);
    
    /// fromifexpressioninfer
    Type* inferFromIfExpr(class IfExpr* if_expr);
    
    /// fromstructbody/structliteralinfer
    Type* inferFromStructLiteral(class StructLiteral* struct_lit);
    
    /// fromfunctioncallinfer
    Type* inferFromCallExpr(class CallExpr* call);
    
    /// infercommon/publictypes（used forifexpressionetc）
    Type* inferCommonType(Type* t1, Type* t2);
};

} // namespace pawc

#endif // PAW_TYPE_INFERENCE_H

