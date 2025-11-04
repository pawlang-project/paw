//===--- decl.h - Declaration AST Nodes -------------------------*- C++ -*-===//
//
// 声明节点：函数、类型、接口等声明
//
//===----------------------------------------------------------------------===//

#ifndef PAW_AST_DECL_H
#define PAW_AST_DECL_H

#include "ast_base.h"
#include <string>
#include <vector>
#include <memory>

namespace pawc {

// 前向声明
class BlockStmt;

/// GenericParam - 泛型参数 <T, U>
struct GenericParam {
    std::string name;
    
    GenericParam(std::string n) : name(std::move(n)) {}
};

/// WhereClause - Where约束 where T: Display
struct WhereClause {
    std::string type_param;     // T
    std::string interface_name; // Display
    
    WhereClause(std::string tp, std::string iface)
        : type_param(std::move(tp)), interface_name(std::move(iface)) {}
};

/// FunctionDecl - 函数声明
class FunctionDecl : public Stmt {
public:
    struct Param {
        std::string name;
        Type* type;
        bool is_mutable;
        
        Param(std::string n, Type* t, bool mut = false)
            : name(std::move(n)), type(t), is_mutable(mut) {}
    };
    
    FunctionDecl(const std::string& name, 
                 std::vector<Param> params,
                 Type* return_type,
                 std::unique_ptr<BlockStmt> body,
                 std::vector<GenericParam> generic_params = {},
                 std::vector<WhereClause> where_clauses = {})
        : name_(name), params_(std::move(params)), return_type_(return_type),
          body_(std::move(body)), generic_params_(std::move(generic_params)),
          where_clauses_(std::move(where_clauses)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<Param>& getParams() const { return params_; }
    Type* getReturnType() const { return return_type_; }
    BlockStmt* getBody() const { return body_.get(); }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    
    bool isGeneric() const { return !generic_params_.empty(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<Param> params_;
    Type* return_type_;
    std::unique_ptr<BlockStmt> body_;
    std::vector<GenericParam> generic_params_;
    std::vector<WhereClause> where_clauses_;
};

/// StructDecl - 结构体定义
class StructDecl : public Stmt {
public:
    struct Field {
        std::string name;
        Type* type;
        
        Field(std::string n, Type* t) : name(std::move(n)), type(t) {}
    };
    
    StructDecl(std::string name, 
               std::vector<Field> fields,
               std::vector<GenericParam> generic_params = {},
               std::vector<WhereClause> where_clauses = {})
        : name_(std::move(name)), fields_(std::move(fields)),
          generic_params_(std::move(generic_params)),
          where_clauses_(std::move(where_clauses)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<Field>& getFields() const { return fields_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    
    bool isGeneric() const { return !generic_params_.empty(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<Field> fields_;
    std::vector<GenericParam> generic_params_;
    std::vector<WhereClause> where_clauses_;
};

/// EnumDecl - 枚举定义
class EnumDecl : public Stmt {
public:
    struct Variant {
        std::string name;
        std::vector<Type*> types;  // 关联数据
        
        Variant(std::string n, std::vector<Type*> t = {})
            : name(std::move(n)), types(std::move(t)) {}
    };
    
    EnumDecl(std::string name, 
             std::vector<Variant> variants,
             std::vector<GenericParam> generic_params = {},
             std::vector<WhereClause> where_clauses = {})
        : name_(std::move(name)), variants_(std::move(variants)),
          generic_params_(std::move(generic_params)),
          where_clauses_(std::move(where_clauses)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<Variant>& getVariants() const { return variants_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    
    bool isGeneric() const { return !generic_params_.empty(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<Variant> variants_;
    std::vector<GenericParam> generic_params_;
    std::vector<WhereClause> where_clauses_;
};

/// InterfaceMethod - 接口方法签名
struct InterfaceMethod {
    std::string name;
    std::vector<FunctionDecl::Param> params;
    Type* return_type;
    
    InterfaceMethod(std::string n, std::vector<FunctionDecl::Param> p, Type* r)
        : name(std::move(n)), params(std::move(p)), return_type(r) {}
};

/// InterfaceDecl - 接口定义
class InterfaceDecl : public Stmt {
public:
    InterfaceDecl(std::string name, 
                  std::vector<InterfaceMethod> methods,
                  std::vector<GenericParam> generic_params = {},
                  std::vector<WhereClause> where_clauses = {})
        : name_(std::move(name)), methods_(std::move(methods)),
          generic_params_(std::move(generic_params)),
          where_clauses_(std::move(where_clauses)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<InterfaceMethod>& getMethods() const { return methods_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    
    bool isGeneric() const { return !generic_params_.empty(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<InterfaceMethod> methods_;
    std::vector<GenericParam> generic_params_;
    std::vector<WhereClause> where_clauses_;
};

/// SupportDecl - 接口实现 support Point with Display { methods }
class SupportDecl : public Stmt {
public:
    SupportDecl(std::string type_name,
                std::vector<GenericParam> type_generic_params,
                std::string interface_name,
                std::vector<GenericParam> interface_generic_params,
                std::vector<std::unique_ptr<FunctionDecl>> methods,
                std::vector<WhereClause> where_clauses = {})
        : type_name_(std::move(type_name)),
          type_generic_params_(std::move(type_generic_params)),
          interface_name_(std::move(interface_name)),
          interface_generic_params_(std::move(interface_generic_params)),
          methods_(std::move(methods)),
          where_clauses_(std::move(where_clauses)) {}
    
    const std::string& getTypeName() const { return type_name_; }
    const std::string& getInterfaceName() const { return interface_name_; }
    const std::vector<std::unique_ptr<FunctionDecl>>& getMethods() const { return methods_; }
    const std::vector<GenericParam>& getGenericParams() const { return type_generic_params_; }  // 向后兼容
    const std::vector<GenericParam>& getTypeGenericParams() const { return type_generic_params_; }
    const std::vector<GenericParam>& getInterfaceGenericParams() const { return interface_generic_params_; }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    
    bool isGeneric() const { return !type_generic_params_.empty(); }
    bool isInterfaceGeneric() const { return !interface_generic_params_.empty(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string type_name_;
    std::vector<GenericParam> type_generic_params_;        // 类型的泛型参数: support Box<T>
    std::string interface_name_;
    std::vector<GenericParam> interface_generic_params_;   // 接口的泛型参数: with Comparable<T>
    std::vector<std::unique_ptr<FunctionDecl>> methods_;
    std::vector<WhereClause> where_clauses_;
};

} // namespace pawc

#endif // PAW_AST_DECL_H

