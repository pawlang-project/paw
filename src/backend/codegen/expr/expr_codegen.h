//===--- expr_codegen.h - Expression Code Generation -------------*- C++ -*-===//

#ifndef PAW_EXPR_CODEGEN_H
#define PAW_EXPR_CODEGEN_H

#include "../codegen_base.h"
#include "frontend/parser/visitor.h"
#include "frontend/parser/ast/expr.h"

namespace pawc {

// Forward declaration
class StmtCodeGen;

/// ExprCodeGen - 表达式代码生成器
class ExprCodeGen : public CodeGenBase, public ASTVisitor {
public:
    explicit ExprCodeGen(CodeGenContext* context);
    
    /// 设置StmtCodeGen（用于BlockExpr）
    void setStmtCodeGen(StmtCodeGen* stmt_codegen) { stmt_codegen_ = stmt_codegen; }
    
    llvm::Value* generate(ASTNode* node) override;
    
    // 表达式访问
    void visit(IntLiteral* node) override;
    void visit(FloatLiteral* node) override;
    void visit(BoolLiteral* node) override;
    void visit(CharLiteral* node) override;
    void visit(StringLiteral* node) override;
    void visit(NullLiteral* node) override;
    void visit(ClosureExpr* node) override;
    void visit(IdentifierExpr* node) override;
    void visit(SelfExpr* node) override;
    void visit(BinaryExpr* node) override;
    void visit(UnaryExpr* node) override;
    void visit(CallExpr* node) override;
    void visit(MemberExpr* node) override;
    void visit(StaticAccessExpr* node) override;
    void visit(IndexExpr* node) override;
    void visit(IfExpr* node) override;
    void visit(BlockExpr* node) override;
    void visit(ArrayLiteral* node) override;
    void visit(TupleExpr* node) override;
    void visit(RangeExpr* node) override;
    void visit(StructLiteral* node) override;
    void visit(MatchExpr* node) override;
    void visit(TryExpr* node) override;
    
    // match表达式辅助方法
    llvm::Value* generatePatternMatch(class Pattern* pattern, llvm::Value* scrutinee, Type* scrutinee_type);
    void bindPatternVariables(class Pattern* pattern, llvm::Value* value, Type* value_type);
    
    // 语句访问（未使用）
    void visit(ExprStmt*) override {}
    void visit(VarDecl*) override {}
    void visit(FunctionDecl*) override {}
    void visit(ReturnStmt*) override {}
    void visit(IfStmt*) override {}
    void visit(LoopStmt*) override {}
    void visit(WhileStmt*) override {}
    void visit(BreakStmt*) override {}
    void visit(ContinueStmt*) override {}
    void visit(BlockStmt*) override {}
    void visit(ForStmt*) override {}
    void visit(StructDecl*) override {}
    void visit(EnumDecl*) override {}
    void visit(InterfaceDecl*) override {}
    void visit(SupportDecl*) override {}
    
    // Pattern访问（不应该在表达式代码生成中使用）
    void visit(LiteralPattern*) override {}
    void visit(WildcardPattern*) override {}
    void visit(VariablePattern*) override {}
    void visit(TuplePattern*) override {}
    void visit(EnumPattern*) override {}
    
private:
    llvm::Value* result_;
    StmtCodeGen* stmt_codegen_ = nullptr;
    
    // 辅助方法
    llvm::Value* generateBinaryOp(TokenType op, llvm::Value* left,
                                  llvm::Value* right, Type* type);
    llvm::Value* generateUnaryOp(TokenType op, llvm::Value* operand, Type* type);
};

} // namespace pawc

#endif // PAW_EXPR_CODEGEN_H
