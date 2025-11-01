//===--- type_checker.h - Semantic Analysis ----------------------*- C++ -*-===//

#ifndef PAW_TYPE_CHECKER_H
#define PAW_TYPE_CHECKER_H

#include "frontend/parser/visitor.h"
#include "frontend/parser/ast/expr.h"
#include "frontend/parser/ast/stmt.h"
#include "middleend/types/type_system.h"
#include "middleend/symbol/symbol_table.h"
#include "diagnostics/diagnostic_engine.h"

namespace pawc {

/// TypeChecker - 类型检查和语义分析
class TypeChecker : public ASTVisitor {
public:
    TypeChecker(TypeSystem* type_system, SymbolTable* symbol_table,
                DiagnosticEngine* diag_engine);
    
    void check(const std::vector<StmtPtr>& stmts);
    
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
    void visit(TryExpr* node) override;
    
    // 语句访问
    void visit(ExprStmt* node) override;
    void visit(VarDecl* node) override;
    void visit(FunctionDecl* node) override;
    void visit(ReturnStmt* node) override;
    void visit(IfStmt* node) override;
    void visit(LoopStmt* node) override;
    void visit(WhileStmt* node) override;
    void visit(BreakStmt* node) override;
    void visit(ContinueStmt* node) override;
    void visit(BlockStmt* node) override;
    void visit(ForStmt* node) override;
    void visit(StructDecl* node) override;
    void visit(EnumDecl* node) override;
    void visit(InterfaceDecl* node) override;
    void visit(SupportDecl* node) override;
    void visit(MatchExpr* node) override;
    
    // match表达式辅助方法
    void checkPatternType(class Pattern* pattern, Type* expected_type);
    bool isExhaustive(const std::vector<class MatchArm>& arms, Type* scrutinee_type);
    
    // Pattern访问
    void visit(LiteralPattern* node) override;
    void visit(WildcardPattern* node) override;
    void visit(VariablePattern* node) override;
    void visit(TuplePattern* node) override;
    void visit(EnumPattern* node) override;
    
private:
    TypeSystem* types_;
    SymbolTable* symbols_;
    DiagnosticEngine* diag_;
    
    Type* current_function_return_type_ = nullptr;
    Type* expected_type_ = nullptr;  // 用于类型推导（向下传播expected type）
    bool in_loop_ = false;
    
    // Where约束验证
    void validateWhereConstraints(const std::vector<WhereClause>& where_clauses,
                                  const std::vector<GenericParam>& generic_params);
};

} // namespace pawc

#endif // PAW_TYPE_CHECKER_H
