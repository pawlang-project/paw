//===--- monomorphization_pass.h - Generic Monomorphization Pass -*- C++ -*-===//
//
// PawLang Compiler - genericmonomorphizationPass
//
// willgenericdefinitionconvertis/asconcretetypesof/theinstance：
//   type Box<T> = struct { value: T }
//   let x = Box { value: 42 };  // generation Box_i32
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
// genericinstantiationrequest
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/// InstantiationRequest - genericinstantiationrequest
struct InstantiationRequest {
    std::string generic_name;           // genericname，like/such as "Box"
    std::vector<Type*> type_args;       // concretetypesparameter，like/such as [i32]
    
    bool operator<(const InstantiationRequest& other) const {
        if (generic_name != other.generic_name) {
            return generic_name < other.generic_name;
        }
        
        // comparetypesparametercount
        if (type_args.size() != other.type_args.size()) {
            return type_args.size() < other.type_args.size();
        }
        
        // one by one/eachindividual/piececomparetypesparameter（through/viatypesname）
        for (size_t i = 0; i < type_args.size(); ++i) {
            std::string this_type_name = type_args[i]->toString();
            std::string other_type_name = other.type_args[i]->toString();
            
            if (this_type_name != other_type_name) {
                return this_type_name < other_type_name;
            }
        }
        
        return false;  // completelysame
    }
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// monomorphizationPass
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/// MonomorphizationPass - genericmonomorphizationPass
///
/// workingflow：
/// 1. first pass：collectAllgenericdefinition（StructDecl, EnumDecl, FunctionDecl with generics）
/// 2. second pass：collectAllgeneric usagepoint（StructLiteral, FunctionCalletc）
/// 3. inferconcretetypesparameter
/// 4. generatemonomorphizationinstance（cloneASTandsubstitutiontypesparameter）
/// 5. willmonomorphizationinstanceinsertedAST
class MonomorphizationPass : public PassBase<MonomorphizationPass>, public ASTVisitor {
public:
    static std::string name() { return "MonomorphizationPass"; }
    
    PassResult runImpl(PassContext* context);
    
    /// getmonomorphizationback/afterof/theinstance（used forASTmerge）
    std::vector<StmtPtr>& getMonomorphizedInstances() { return monomorphized_stmts_; }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Visitorimplementation
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    // Statements
    void visit(ExprStmt* stmt) override;
    void visit(VarDecl* decl) override;
    void visit(DestructuringDecl* decl) override;
    void visit(StructDestructuringDecl* node) override;
    void visit(FunctionDecl* decl) override;
    void visit(ReturnStmt* stmt) override;
    void visit(IfStmt* stmt) override;
    void visit(LoopStmt* stmt) override;
    void visit(WhileStmt* stmt) override;
    void visit(ForStmt* stmt) override;
    void visit(BreakStmt* stmt) override;
    void visit(ContinueStmt* stmt) override;
    void visit(BlockStmt* stmt) override;
    void visit(StructDecl* decl) override;      // collectgenericdefinition
    void visit(EnumDecl* decl) override;         // collectgenericdefinition
    void visit(InterfaceDecl* decl) override;
    void visit(SupportDecl* decl) override;
    
    // Expressions
    void visit(IntLiteral* expr) override;
    void visit(FloatLiteral* expr) override;
    void visit(BoolLiteral* expr) override;
    void visit(CharLiteral* expr) override;
    void visit(StringLiteral* expr) override;
    void visit(NoneLiteral* expr) override;
    void visit(CastExpr* expr) override;
    void visit(ClosureExpr* expr) override;
    void visit(ArrayLiteral* expr) override;
    void visit(TupleExpr* expr) override;
    void visit(StructLiteral* expr) override;    // generic usagepoint
    void visit(IdentifierExpr* expr) override;
    void visit(BinaryExpr* expr) override;
    void visit(UnaryExpr* expr) override;
    void visit(CallExpr* expr) override;         // generic usagepoint
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
    void visit(StructPattern* pattern) override;
    void visit(ArrayPattern* pattern) override;
    void visit(SlicePattern* pattern) override;
    void visit(RangePattern* pattern) override;
    void visit(OrPattern* pattern) override;
    
private:
    PassContext* context_ = nullptr;
    
    // genericdefinitionstorage
    std::map<std::string, StructDecl*> generic_structs_;   // genericstructbody/struct
    std::map<std::string, EnumDecl*> generic_enums_;       // genericenum
    std::map<std::string, FunctionDecl*> generic_functions_; // genericfunction
    
    // instantiationcache（avoidduplicateinstantiation）
    std::map<InstantiationRequest, std::string> instance_cache_;
    
    // newgenerateof/themonomorphizationinstance
    std::vector<StmtPtr> monomorphized_stmts_;
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // helper function
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    /// collectgenericdefinition
    void collectGenericDefinitions(const std::vector<StmtPtr>& ast);
    
    /// fromStructLiteralinfertypesparameter（completeimplementation）
    std::vector<Type*> inferTypeArgsFromStructLiteral(StructLiteral* lit, StructDecl* generic_decl);
    
    /// fromCallExprinfertypesparameter
    std::vector<Type*> inferTypeArgsFromCallExpr(CallExpr* call, FunctionDecl* generic_func);
    
    /// generationmonomorphizationname: Box<i32> -> Box_i32
    // Note：generateMonomorphizedNamemoved to backend/codegen/generic/mangling.h
    // Keep a forwarding herefunctionTo maintain compatibility
    std::string generateMonomorphizedName(const std::string& base_name,
                                         const std::vector<Type*>& type_args);
    
    /// cloneandsubstitutiontypesparameter
    StmtPtr cloneAndSubstitute(StructDecl* generic_decl, const std::vector<Type*>& type_args);
    StmtPtr cloneAndSubstitute(EnumDecl* generic_decl, const std::vector<Type*>& type_args);
    StmtPtr cloneAndSubstitute(FunctionDecl* generic_decl, const std::vector<Type*>& type_args);
    
    /// typesubstitution（willgenericparameterTsubstitutionis/asconcretetypes）
    Type* substituteType(Type* type, 
                         const std::map<std::string, Type*>& type_mapping);
    
    /// fromexpressioninfertypes（used formonomorphization）
    Type* inferTypeFromExpr(Expr* expr);
    
    /// cloneASTnode（used forfunctionbody/structclone）
    StmtPtr cloneStmt(Stmt* stmt, const std::map<std::string, Type*>& type_mapping);
    ExprPtr cloneExpr(Expr* expr, const std::map<std::string, Type*>& type_mapping);
};

} // namespace pawc

#endif // PAW_MONOMORPHIZATION_PASS_H

