//===--- stmt_codegen.h - Statement Code Generation --------------*- C++ -*-===//

#ifndef PAW_STMT_CODEGEN_H
#define PAW_STMT_CODEGEN_H

#include <map>  // 🔧 newadd：genericinterfacedefaultmethodneed
#include "../codegen_base.h"
#include "frontend/parser/visitor.h"
#include "frontend/parser/ast/stmt.h"

namespace pawc {

class ExprCodeGen;

/// StmtCodeGen - statementcodegenerator
class StmtCodeGen : public CodeGenBase, public ASTVisitor {
public:
    StmtCodeGen(CodeGenContext* context, ExprCodeGen* expr_codegen);
    
    llvm::Value* generate(ASTNode* node) override;
    
    /// 🔧 generatefunctiondeclaration（onlysignature，notgeneratefunctionbody/struct）
    void generateFunctionDeclaration(FunctionDecl* node);
    
    // statementvisit
    void visit(ExprStmt* node) override;
    void visit(VarDecl* node) override;
    void visit(DestructuringDecl* node) override;
    void visit(StructDestructuringDecl* node) override;
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
    
    // Expression visiting (delegated to ExprCodeGen)
    void visit(IntLiteral*) override {}
    void visit(FloatLiteral*) override {}
    void visit(BoolLiteral*) override {}
    void visit(CharLiteral*) override {}
    void visit(StringLiteral*) override {}
    void visit(NoneLiteral*) override {}
    void visit(CastExpr*) override {}
    void visit(ClosureExpr*) override {}
    void visit(IdentifierExpr*) override {}
    void visit(BinaryExpr*) override {}
    void visit(UnaryExpr*) override {}
    void visit(CallExpr*) override {}
    void visit(MemberExpr*) override {}
    void visit(StaticAccessExpr*) override {}
    void visit(IndexExpr*) override {}
    void visit(IfExpr*) override {}
    void visit(BlockExpr*) override {}
    void visit(ArrayLiteral*) override {}
    void visit(TupleExpr*) override {}
    void visit(RangeExpr*) override {}
    void visit(StructLiteral*) override {}
    void visit(MatchExpr*) override {}
    void visit(SelfExpr*) override {}
    void visit(TryExpr*) override {}
    
    // Patternvisit（notshouldin/atstatementcode generationmiddle/centeruse）
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
    
private:
    ExprCodeGen* expr_codegen_;
    llvm::Value* results_;
    
    // control flowmanage
    llvm::BasicBlock* break_target_ = nullptr;
    llvm::BasicBlock* continue_target_ = nullptr;
    
    // 🔧 Self/supportcontextmanage
    std::string current_support_type_;  // currentsupportof/thetypesname（used formethodnamequalify）
    
    /// is/asAllmain()generateC ABIcompatibleof/thewrapper
    void generateMainWrapper(llvm::Function* paw_main, llvm::Type* paw_return_type);
    
    /// 🔧 generateinterfacedefaultmethodof/thewrap
    void generateDefaultMethod(const std::string& type_name, 
                               const class InterfaceType::MethodSignature& method,
                               const std::map<std::string, Type*>& generic_substitution = {});
};

} // namespace pawc

#endif // PAW_STMT_CODEGEN_H
