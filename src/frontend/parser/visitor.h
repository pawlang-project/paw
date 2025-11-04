//===--- visitor.h - AST Visitor Pattern -------------------------*- C++ -*-===//

#ifndef PAW_VISITOR_H
#define PAW_VISITOR_H

namespace pawc {

// 前向声明所有AST节点类型
class IntLiteral;
class FloatLiteral;
class BoolLiteral;
class CharLiteral;
class StringLiteral;
class NoneLiteral;
class CastExpr;
class ClosureExpr;
class IdentifierExpr;
class SelfExpr;
class BinaryExpr;
class UnaryExpr;
class CallExpr;
class MemberExpr;
class StaticAccessExpr;
class IndexExpr;
class IfExpr;
class BlockExpr;
class ArrayLiteral;
class TupleExpr;
class RangeExpr;
class StructLiteral;
class MatchExpr;
class TryExpr;

// Pattern前向声明
class LiteralPattern;
class WildcardPattern;
class VariablePattern;
class TuplePattern;
class EnumPattern;
class StructPattern;
class ArrayPattern;
class SlicePattern;

class ExprStmt;
class VarDecl;
class DestructuringDecl;
class StructDestructuringDecl;
class FunctionDecl;
class ReturnStmt;
class IfStmt;
class LoopStmt;
class WhileStmt;
class BreakStmt;
class ContinueStmt;
class BlockStmt;
class ForStmt;
class StructDecl;
class EnumDecl;
class InterfaceDecl;
class SupportDecl;

/// ASTVisitor - 访问者基类
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    // 表达式访问
    virtual void visit(IntLiteral* node) = 0;
    virtual void visit(FloatLiteral* node) = 0;
    virtual void visit(BoolLiteral* node) = 0;
    virtual void visit(CharLiteral* node) = 0;
    virtual void visit(StringLiteral* node) = 0;
    virtual void visit(NoneLiteral* node) = 0;
    virtual void visit(CastExpr* node) = 0;
    virtual void visit(ClosureExpr* node) = 0;
    virtual void visit(IdentifierExpr* node) = 0;
    virtual void visit(SelfExpr* node) = 0;
    virtual void visit(BinaryExpr* node) = 0;
    virtual void visit(UnaryExpr* node) = 0;
    virtual void visit(CallExpr* node) = 0;
    virtual void visit(MemberExpr* node) = 0;
    virtual void visit(StaticAccessExpr* node) = 0;
    virtual void visit(IndexExpr* node) = 0;
    virtual void visit(IfExpr* node) = 0;
    virtual void visit(BlockExpr* node) = 0;
    virtual void visit(ArrayLiteral* node) = 0;
    virtual void visit(TupleExpr* node) = 0;
    virtual void visit(RangeExpr* node) = 0;
    virtual void visit(StructLiteral* node) = 0;
    virtual void visit(MatchExpr* node) = 0;
    virtual void visit(TryExpr* node) = 0;
    
    // Pattern访问
    virtual void visit(LiteralPattern* node) = 0;
    virtual void visit(WildcardPattern* node) = 0;
    virtual void visit(VariablePattern* node) = 0;
    virtual void visit(TuplePattern* node) = 0;
    virtual void visit(EnumPattern* node) = 0;
    virtual void visit(StructPattern* node) = 0;
    virtual void visit(ArrayPattern* node) = 0;
    virtual void visit(SlicePattern* node) = 0;
    
    // 语句访问
    virtual void visit(ExprStmt* node) = 0;
    virtual void visit(VarDecl* node) = 0;
    virtual void visit(DestructuringDecl* node) = 0;
    virtual void visit(StructDestructuringDecl* node) = 0;
    virtual void visit(FunctionDecl* node) = 0;
    virtual void visit(ReturnStmt* node) = 0;
    virtual void visit(IfStmt* node) = 0;
    virtual void visit(LoopStmt* node) = 0;
    virtual void visit(WhileStmt* node) = 0;
    virtual void visit(BreakStmt* node) = 0;
    virtual void visit(ContinueStmt* node) = 0;
    virtual void visit(BlockStmt* node) = 0;
    virtual void visit(ForStmt* node) = 0;
    virtual void visit(StructDecl* node) = 0;
    virtual void visit(EnumDecl* node) = 0;
    virtual void visit(InterfaceDecl* node) = 0;
    virtual void visit(SupportDecl* node) = 0;
};

} // namespace pawc

#endif // PAW_VISITOR_H
