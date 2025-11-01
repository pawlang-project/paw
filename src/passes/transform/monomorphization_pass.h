//===--- monomorphization_pass.h - Generic Monomorphization Pass -*- C++ -*-===//
//
// PawLang Compiler - 泛型单态化Pass
//
// 将泛型定义转换为具体类型的实例：
//   type Box<T> = struct { value: T }
//   let x = Box { value: 42 };  // 生成 Box_i32
//
//===----------------------------------------------------------------------===//

#ifndef PAW_MONOMORPHIZATION_PASS_H
#define PAW_MONOMORPHIZATION_PASS_H

#include "pass/pass.h"
#include "frontend/parser/ast/ast_base.h"
#include "frontend/parser/ast/stmt.h"
#include "frontend/parser/visitor.h"
#include <map>
#include <vector>
#include <string>

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 泛型实例化请求
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/// InstantiationRequest - 泛型实例化请求
struct InstantiationRequest {
    std::string generic_name;           // 泛型名称，如 "Box"
    std::vector<Type*> type_args;       // 具体类型参数，如 [i32]
    
    bool operator<(const InstantiationRequest& other) const {
        if (generic_name != other.generic_name) {
            return generic_name < other.generic_name;
        }
        
        // 比较类型参数数量
        if (type_args.size() != other.type_args.size()) {
            return type_args.size() < other.type_args.size();
        }
        
        // 逐个比较类型参数（通过类型名称）
        for (size_t i = 0; i < type_args.size(); ++i) {
            std::string this_type_name = type_args[i]->toString();
            std::string other_type_name = other.type_args[i]->toString();
            
            if (this_type_name != other_type_name) {
                return this_type_name < other_type_name;
            }
        }
        
        return false;  // 完全相同
    }
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 单态化Pass
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/// MonomorphizationPass - 泛型单态化Pass
///
/// 工作流程：
/// 1. 第一遍：收集所有泛型定义（StructDecl, EnumDecl, FunctionDecl with generics）
/// 2. 第二遍：收集所有泛型使用点（StructLiteral, FunctionCall等）
/// 3. 推导具体类型参数
/// 4. 生成单态化实例（克隆AST并替换类型参数）
/// 5. 将单态化实例插入AST
class MonomorphizationPass : public PassBase<MonomorphizationPass>, public ASTVisitor {
public:
    static std::string name() { return "MonomorphizationPass"; }
    
    PassResult runImpl(PassContext* context);
    
    /// 获取单态化后的实例（用于AST合并）
    std::vector<StmtPtr>& getMonomorphizedInstances() { return monomorphized_stmts_; }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Visitor实现
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    // Statements
    void visit(ExprStmt* stmt) override;
    void visit(VarDecl* decl) override;
    void visit(FunctionDecl* decl) override;
    void visit(ReturnStmt* stmt) override;
    void visit(IfStmt* stmt) override;
    void visit(LoopStmt* stmt) override;
    void visit(WhileStmt* stmt) override;
    void visit(ForStmt* stmt) override;
    void visit(BreakStmt* stmt) override;
    void visit(ContinueStmt* stmt) override;
    void visit(BlockStmt* stmt) override;
    void visit(StructDecl* decl) override;      // 收集泛型定义
    void visit(EnumDecl* decl) override;         // 收集泛型定义
    void visit(InterfaceDecl* decl) override;
    void visit(SupportDecl* decl) override;
    
    // Expressions
    void visit(IntLiteral* expr) override;
    void visit(FloatLiteral* expr) override;
    void visit(BoolLiteral* expr) override;
    void visit(CharLiteral* expr) override;
    void visit(StringLiteral* expr) override;
    void visit(NullLiteral* expr) override;
    void visit(ClosureExpr* expr) override;
    void visit(ArrayLiteral* expr) override;
    void visit(TupleExpr* expr) override;
    void visit(StructLiteral* expr) override;    // 泛型使用点
    void visit(IdentifierExpr* expr) override;
    void visit(BinaryExpr* expr) override;
    void visit(UnaryExpr* expr) override;
    void visit(CallExpr* expr) override;         // 泛型使用点
    void visit(MemberExpr* expr) override;
    void visit(StaticAccessExpr* expr) override;
    void visit(IndexExpr* expr) override;
    void visit(BlockExpr* expr) override;
    void visit(IfExpr* expr) override;
    void visit(RangeExpr* expr) override;
    void visit(SelfExpr* expr) override;
    void visit(MatchExpr* expr) override;
    void visit(TryExpr* expr) override;
    
    // Match expressions and patterns
    void visit(LiteralPattern* pattern) override;
    void visit(VariablePattern* pattern) override;
    void visit(WildcardPattern* pattern) override;
    void visit(TuplePattern* pattern) override;
    void visit(EnumPattern* pattern) override;
    
private:
    PassContext* context_ = nullptr;
    
    // 泛型定义存储
    std::map<std::string, StructDecl*> generic_structs_;   // 泛型结构体
    std::map<std::string, EnumDecl*> generic_enums_;       // 泛型枚举
    std::map<std::string, FunctionDecl*> generic_functions_; // 泛型函数
    
    // 实例化缓存（避免重复实例化）
    std::map<InstantiationRequest, std::string> instance_cache_;
    
    // 新生成的单态化实例
    std::vector<StmtPtr> monomorphized_stmts_;
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 辅助函数
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    /// 收集泛型定义
    void collectGenericDefinitions(const std::vector<StmtPtr>& ast);
    
    /// 从StructLiteral推导类型参数（完整实现）
    std::vector<Type*> inferTypeArgsFromStructLiteral(StructLiteral* lit, StructDecl* generic_decl);
    
    /// 从CallExpr推导类型参数
    std::vector<Type*> inferTypeArgsFromCallExpr(CallExpr* call, FunctionDecl* generic_func);
    
    /// 生成单态化名称: Box<i32> -> Box_i32
    // 注意：generateMonomorphizedName已移至 backend/codegen/generic/mangling.h
    // 这里保留一个转发函数以保持兼容性
    std::string generateMonomorphizedName(const std::string& base_name,
                                         const std::vector<Type*>& type_args);
    
    /// 克隆并替换类型参数
    StmtPtr cloneAndSubstitute(StructDecl* generic_decl, const std::vector<Type*>& type_args);
    StmtPtr cloneAndSubstitute(EnumDecl* generic_decl, const std::vector<Type*>& type_args);
    StmtPtr cloneAndSubstitute(FunctionDecl* generic_decl, const std::vector<Type*>& type_args);
    
    /// 类型替换（将泛型参数T替换为具体类型）
    Type* substituteType(Type* type, 
                         const std::map<std::string, Type*>& type_mapping);
    
    /// 从表达式推导类型（用于单态化）
    Type* inferTypeFromExpr(Expr* expr);
    
    /// 克隆AST节点（用于函数体克隆）
    StmtPtr cloneStmt(Stmt* stmt, const std::map<std::string, Type*>& type_mapping);
    ExprPtr cloneExpr(Expr* expr, const std::map<std::string, Type*>& type_mapping);
};

} // namespace pawc

#endif // PAW_MONOMORPHIZATION_PASS_H

