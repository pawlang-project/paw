//===--- expr_codegen.h - Expression Code Generation -------------*- C++ -*-===//

#ifndef PAW_EXPR_CODEGEN_H
#define PAW_EXPR_CODEGEN_H

#include "../codegen_base.h"
#include "frontend/parser/visitor.h"
#include "frontend/parser/ast/expr.h"

#include "frontend/parser/ast/stmt.h"
#include "backend/codegen/codegen_context.h"
#include "middleend/types/generic_template.h"
namespace pawc {

// Forward declaration
class StmtCodeGen;

/// ExprCodeGen - expressioncodegenerator
class ExprCodeGen : public CodeGenBase, public ASTVisitor {
public:
    explicit ExprCodeGen(CodeGenContext* context);
    
    /// setStmtCodeGen（used forBlockExpr）
    void setStmtCodeGen(StmtCodeGen* stmt_codegen) { stmt_codegen_ = stmt_codegen; }
    
    llvm::Value* generate(ASTNode* node) override;
    
    // expressionvisit
    void visit(IntLiteral* node) override;
    void visit(FloatLiteral* node) override;
    void visit(BoolLiteral* node) override;
    void visit(CharLiteral* node) override;
    void visit(StringLiteral* node) override;
    void visit(NoneLiteral* node) override;
    void visit(CastExpr* node) override;
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
    
    // matchexpressionhelpermethod
    llvm::Value* generatePatternMatch(class Pattern* pattern, llvm::Value* scrutinee, Type* scrutinee_type);
    void bindPatternVariables(class Pattern* pattern, llvm::Value* value, Type* value_type);
    
    // ResultandOptionalpattern matchinghelpermethod
    llvm::Value* generateResultPatternMatch(class EnumPattern* pattern, llvm::Value* scrutinee, class ResultType* type);
    llvm::Value* generateOptionalPatternMatch(class EnumPattern* pattern, llvm::Value* scrutinee, class OptionalType* type);
    void bindResultPatternVariables(class EnumPattern* pattern, llvm::Value* value, class ResultType* type);
    void bindOptionalPatternVariables(class EnumPattern* pattern, llvm::Value* value, class OptionalType* type);
    void bindStructPatternVariables(class StructPattern* pattern, llvm::Value* value, class StructType* type);
    
    // genericfunctionCodeGen
    llvm::Value* generateGenericFunctionCall(const std::string& func_name,
                                            const std::vector<Type*>& type_args,
                                            const std::vector<std::unique_ptr<Expr>>& args);
    llvm::Function* generateGenericFunctionInstance(const std::string& func_name,
                                                    const std::vector<Type*>& type_args,
                                                    GenericTemplate* tmpl);
    
    // statementvisit（not yetuse）
    void visit(ExprStmt*) override {}
    void visit(VarDecl*) override {}
    void visit(DestructuringDecl*) override {}
    void visit(StructDestructuringDecl*) override {}
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
    
    // Patternvisit（notshouldin/atexpressioncode generationmiddle/centeruse）
    void visit(LiteralPattern*) override {}
    void visit(WildcardPattern*) override {}
    void visit(VariablePattern*) override {}
    void visit(TuplePattern*) override {}
    void visit(EnumPattern*) override {}
    void visit(StructPattern*) override {}
    void visit(ArrayPattern*) override {}
    void visit(SlicePattern*) override {}
    void visit(RangePattern*) override {}
    void visit(OrPattern*) override {}
    
    // 🔧 getup/abovepower/timesexpressionof/theresults（for/provideStmtCodeGenuse）
    llvm::Value* getResult() const { return results_; }
    
private:
    llvm::Value* results_;
    StmtCodeGen* stmt_codegen_ = nullptr;
    
    // helpermethod
    llvm::Value* generateBinaryOp(TokenType op, llvm::Value* left,
                                  llvm::Value* right, Type* type);
    llvm::Value* generateUnaryOp(TokenType op, llvm::Value* operand, Type* type);
};

} // namespace pawc

#endif // PAW_EXPR_CODEGEN_H
