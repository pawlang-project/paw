//===--- capture_analyzer.h - Closure Capture Analysis -------*- C++ -*-===//
/// @file capture_analyzer.h
/// @brief Compiler infrastructure
///
//
// closurecapturevariableanalysislinker/er
// traverseclosurebody/struct，lookupAllself/frombyvariable（in/atexternal/outsidescopedefinitionof/thevariable）
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CAPTURE_ANALYZER_H
#define PAW_CAPTURE_ANALYZER_H

#include "frontend/parser/visitor.h"
#include "frontend/parser/ast/expr.h"
#include "frontend/parser/ast/stmt.h"
#include "middleend/symbol/symbol_table.h"
#include <set>
#include <string>
#include <vector>

namespace pawc {

class CaptureAnalyzer : public ASTVisitor {
public:
    CaptureAnalyzer(SymbolTable* symbols) : symbols_(symbols) {}
    
    /// analysisclosurebody/struct，returncapturevariablelist
    std::vector<ClosureExpr::CapturedVar> analyze(Expr* body, const std::vector<ClosureExpr::Param>& params);
    
    // expressionvisit
    void visit(IntLiteral* node) override {}
    void visit(FloatLiteral* node) override {}
    void visit(BoolLiteral* node) override {}
    void visit(CharLiteral* node) override {}
    void visit(StringLiteral* node) override {}
    void visit(NoneLiteral* node) override {}
    void visit(CastExpr* node) override { if (node->getExpr()) node->getExpr()->accept(this); }
    void visit(IdentifierExpr* node) override;
    void visit(SelfExpr* node) override {}
    void visit(BinaryExpr* node) override;
    void visit(UnaryExpr* node) override;
    void visit(CallExpr* node) override;
    void visit(MemberExpr* node) override;
    void visit(StaticAccessExpr* node) override {}
    void visit(IndexExpr* node) override;
    void visit(IfExpr* node) override;
    void visit(BlockExpr* node) override;
    void visit(ArrayLiteral* node) override;
    void visit(TupleExpr* node) override;
    void visit(RangeExpr* node) override;
    void visit(StructLiteral* node) override;
    void visit(MatchExpr* node) override;
    void visit(ClosureExpr* node) override;
    void visit(TryExpr* node) override;
    
    // statementvisit
    void visit(ExprStmt* node) override;
    void visit(VarDecl* node) override;
    void visit(DestructuringDecl* node) override;
    void visit(StructDestructuringDecl* node) override;
    void visit(FunctionDecl* node) override {}
    void visit(ReturnStmt* node) override;
    void visit(IfStmt* node) override;
    void visit(LoopStmt* node) override;
    void visit(WhileStmt* node) override;
    void visit(BreakStmt* node) override {}
    void visit(ContinueStmt* node) override {}
    void visit(BlockStmt* node) override;
    void visit(ForStmt* node) override;
    void visit(StructDecl* node) override {}
    void visit(EnumDecl* node) override {}
    void visit(InterfaceDecl* node) override {}
    void visit(SupportDecl* node) override {}
    
    // Patternvisit
    void visit(LiteralPattern* node) override {}
    void visit(WildcardPattern* node) override {}
    void visit(VariablePattern* node) override;
    void visit(TuplePattern* node) override;
    void visit(EnumPattern* node) override;
    void visit(StructPattern* node) override;
    void visit(ArrayPattern* node) override;
    void visit(SlicePattern* node) override;
    void visit(RangePattern* node) override {}
    void visit(OrPattern* node) override;
    
private:
    SymbolTable* symbols_;
    std::vector<std::set<std::string>> local_vars_stack_;  // Scope stack: each level's local variablesiable
    std::set<std::string> captured_vars_;  // captureof/theexternal/outsidevariable
    int closure_depth_;  // nestedclosuredepth
    
    // helpermethod
    void enterScope();
    void exitScope();
    bool isLocalVariable(const std::string& name) const;
};

} // namespace pawc

#endif // PAW_CAPTURE_ANALYZER_H

