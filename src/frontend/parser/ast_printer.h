//===--- ast_printer.h - AST Pretty Printer ---------------------*- C++ -*-===//
//
// PawLang Compiler - AST Debugging Tool
//
//===----------------------------------------------------------------------===//

#ifndef PAW_AST_PRINTER_H
#define PAW_AST_PRINTER_H

#include "visitor.h"
#include "ast/expr.h"
#include "ast/stmt.h"

#include <ostream>
#include <string>

namespace pawc {

/// ASTPrinter - AST结构打印器（调试工具）
///
/// 用于-ast-dump选项，以可读格式输出AST结构
class ASTPrinter : public ASTVisitor {
public:
    explicit ASTPrinter(std::ostream& os) : os_(os), indent_(0) {}
    
    /// 打印AST节点
    void print(ASTNode* node);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 表达式访问
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
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
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 语句访问
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
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
    
    // Pattern访问
    void visit(LiteralPattern* node) override;
    void visit(WildcardPattern* node) override;
    void visit(VariablePattern* node) override;
    void visit(TuplePattern* node) override;
    void visit(EnumPattern* node) override;
    void visit(StructPattern* node) override;
    
private:
    std::ostream& os_;
    int indent_;
    
    /// 打印缩进
    void printIndent();
    
    /// 打印带缩进的行
    void printLine(const std::string& text);
    
    /// 获取类型字符串
    std::string getTypeString(Type* type);
    
    /// 获取操作符字符串
    std::string getOpString(TokenType op);
};

} // namespace pawc

#endif // PAW_AST_PRINTER_H

