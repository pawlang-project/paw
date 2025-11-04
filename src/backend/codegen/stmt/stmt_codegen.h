//===--- stmt_codegen.h - Statement Code Generation --------------*- C++ -*-===//

#ifndef PAW_STMT_CODEGEN_H
#define PAW_STMT_CODEGEN_H

#include <map>  // 🔧 新增：泛型接口默认方法需要
#include "../codegen_base.h"
#include "frontend/parser/visitor.h"
#include "frontend/parser/ast/stmt.h"

namespace pawc {

class ExprCodeGen;

/// StmtCodeGen - 语句代码生成器
class StmtCodeGen : public CodeGenBase, public ASTVisitor {
public:
    StmtCodeGen(CodeGenContext* context, ExprCodeGen* expr_codegen);
    
    llvm::Value* generate(ASTNode* node) override;
    
    /// 🔧 生成函数声明（仅签名，不生成函数体）
    void generateFunctionDeclaration(FunctionDecl* node);
    
    // 语句访问
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
    
    // 表达式访问（委托给ExprCodeGen）
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
    
    // Pattern访问（不应该在语句代码生成中使用）
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
    llvm::Value* result_;
    
    // 控制流管理
    llvm::BasicBlock* break_target_ = nullptr;
    llvm::BasicBlock* continue_target_ = nullptr;
    
    // 🔧 Self/support上下文管理
    std::string current_support_type_;  // 当前support的类型名（用于方法名修饰）
    
    /// 为所有main()生成C ABI兼容的wrapper
    void generateMainWrapper(llvm::Function* paw_main, llvm::Type* paw_return_type);
    
    /// 🔧 生成接口默认方法的包装
    void generateDefaultMethod(const std::string& type_name, 
                               const class InterfaceType::MethodSignature& method,
                               const std::map<std::string, Type*>& generic_substitution = {});
};

} // namespace pawc

#endif // PAW_STMT_CODEGEN_H
