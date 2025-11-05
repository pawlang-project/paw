//===--- stmt.h - Statement AST Nodes ---------------------------*- C++ -*-===//
//
// This file defines all statement and declaration AST node classes for PawLang.
//
// Statements represent actions and declarations that don't produce values:
//   - Control flow: if, loop, break, continue, return
//   - Variable declarations: let, let~
//   - Function declarations: fn
//   - Type declarations: struct, enum, interface, support
//
// All statement nodes inherit from Stmt and implement the Visitor pattern
// for traversal during:
//   - Type checking
//   - Code generation
//   - Optimization passes
//   - AST transformations (monomorphization, etc.)
//
// Design:
//   - Immutable after construction (no setters for core data)
//   - Owned pointers for sub-nodes (unique_ptr)
//   - Raw pointers for type references (managed by TypeSystem)
//
//===----------------------------------------------------------------------===//

#ifndef PAW_STMT_H
#define PAW_STMT_H

#include "ast_base.h"
#include "expr.h"
#include <string>

namespace pawc {

// ============================================================================
// Generic Parameters and Constraints
// ============================================================================

/// GenericParam - Generic type parameter
///
/// Represents a type parameter in generic declarations.
///
/// Examples:
///   fn foo<T>(x: T)                    - T is a GenericParam
///   struct Box<T> { value: T }         - T is a GenericParam
///   interface Comparable<T> { ... }    - T is a GenericParam
///
/// Usage:
///   - Used in function, struct, enum, and interface declarations
///   - Name is used for type substitution during monomorphization
///   - No constraints in the parameter itself (constraints in where clauses)
///
/// Monomorphization:
///   - Each GenericParam is replaced with concrete types during instantiation
///   - Example: Box<i32> replaces T with i32
struct GenericParam {
    std::string name;  ///< Parameter name (e.g., "T", "U", "K", "V")
    
    /// Constructs a GenericParam
    /// @param n The parameter name
    explicit GenericParam(const std::string& n) : name(n) {}
    
    /// Compare two generic parameters by name
    bool operator==(const GenericParam& other) const {
        return name == other.name;
    }
};

/// WhereClause - Generic constraint clause
///
/// Represents a type constraint in where clauses.
///
/// Syntax: where T: Interface
///
/// Examples:
///   where T: Display                     - T must implement Display
///   where T: Display, U: Clone           - Multiple constraints
///   where T: Display + Debug             - Combined constraints (T implements both)
///
/// Usage:
///   - Attached to function declarations and support blocks
///   - Enforces interface implementations on generic parameters
///   - Checked during type checking and monomorphization
///
/// Validation:
///   - Type parameter must exist in generic parameters
///   - Interface must be defined
///   - Implementation must exist when instantiated
///
/// Code generation:
///   - Resolved during monomorphization
///   - Method calls dispatch to concrete implementations
struct WhereClause {
    std::string type_param;       ///< Type parameter name (e.g., "T")
    std::string interface_name;   ///< Interface name (e.g., "Display", "Clone")
    
    /// Constructs a WhereClause
    /// @param tp Type parameter to constrain
    /// @param iface Interface that must be implemented
    WhereClause(const std::string& tp, const std::string& iface)
        : type_param(tp), interface_name(iface) {}
    
    /// Compare two where clauses
    bool operator==(const WhereClause& other) const {
        return type_param == other.type_param && 
               interface_name == other.interface_name;
    }
};

// ============================================================================
// Statement AST Nodes
// ============================================================================

/// ExprStmt - Expression statement
///
/// Represents an expression used as a statement (evaluated for side effects).
///
/// Syntax: expression;
///
/// Examples:
///   println("hello");                  - Function call for side effect
///   x = 10;                            - Assignment statement
///   array.push(value);                 - Method call
///
/// Type checking:
///   - Expression is type-checked normally
///   - Result value is discarded
///   - Typically used for expressions with side effects
///
/// Code generation:
///   - Generate code for the expression
///   - Result value is not stored
class ExprStmt : public Stmt {
public:
    /// Constructs an ExprStmt
    /// @param expr The expression to evaluate
    explicit ExprStmt(ExprPtr expr) : expr_(std::move(expr)) {}
    
    /// Get the expression
    Expr* getExpr() const { return expr_.get(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr expr_;  ///< Expression to evaluate for side effects
};

/// VarDecl - Variable declaration statement
///
/// Represents both immutable and mutable variable declarations.
///
/// Syntax:
///   let x: T = value             - Immutable variable
///   let~ x: T = value            - Mutable variable (~  marks mutability)
///   let x = value                - Type inferred
///
/// Examples:
///   let name = "Alice";          - Immutable string
///   let~ counter = 0;            - Mutable integer
///   let point: Point = Point{x: 0, y: 0};
///
/// Type inference:
///   - If type is nullptr, inferred from initializer during type checking
///   - Explicit type annotation takes precedence
///   - Initializer must be compatible with declared type
///
/// Mutability:
///   - Immutable by default (Rust-like semantics)
///   - Mutable marked with ~ symbol
///   - Affects assignability and reference creation
///
/// Code generation:
///   - Stack allocation (alloca instruction)
///   - Store initializer value
///   - Symbol table entry for variable lookup
class VarDecl : public Stmt {
public:
    /// Constructs a VarDecl
    /// @param name Variable name
    /// @param type Explicit type annotation (nullptr if inferred)
    /// @param is_mutable true for let~, false for let
    /// @param init Initializer expression
    VarDecl(const std::string& name, Type* type, bool is_mutable, ExprPtr init)
        : name_(name), type_(type), is_mutable_(is_mutable), init_(std::move(init)) {}
    
    /// Get the variable name
    const std::string& getName() const { return name_; }
    
    /// Get the variable type (may be nullptr before type inference)
    Type* getType() const { return type_; }
    
    /// Set the variable type (during type inference)
    /// @param type The inferred or validated type
    void setType(Type* type) { type_ = type; }
    
    /// Check if this variable is mutable
    bool isMutable() const { return is_mutable_; }
    
    /// Get the initializer expression
    Expr* getInit() const { return init_.get(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;      ///< Variable name
    Type* type_;            ///< Variable type (nullptr before inference)
    bool is_mutable_;       ///< true for let~, false for let
    ExprPtr init_;          ///< Initializer expression
};

/// DestructuringDecl - Tuple destructuring declaration
///
/// Represents destructuring a tuple into multiple variables.
///
/// Syntax:
///   let (name1, name2, ...) = tuple_expr
///   let~ (name1, name2, ...) = tuple_expr
///
/// Examples:
///   let (x, y) = (10, 20);                - Destructure tuple
///   let~ (a, b, c) = get_tuple();         - Mutable destructuring
///   let (first, _, third) = tuple;        - Skip middle element
///
/// Type checking:
///   - init expression must have Tuple type
///   - Number of names must match tuple arity
///   - Each variable gets corresponding element type
///
/// Variable binding:
///   - Creates multiple variable declarations
///   - All variables have same mutability (all mutable or all immutable)
///
/// Code generation:
///   - Extract tuple value
///   - GEP to access each element
///   - Store to individual variables
class DestructuringDecl : public Stmt {
public:
    /// Constructs a DestructuringDecl
    /// @param names Variable names (must match tuple arity)
    /// @param is_mutable true for let~, false for let
    /// @param init Tuple expression to destructure
    DestructuringDecl(std::vector<std::string> names, bool is_mutable, ExprPtr init)
        : names_(std::move(names)), is_mutable_(is_mutable), init_(std::move(init)) {}
    
    /// Get the variable names
    const std::vector<std::string>& getNames() const { return names_; }
    
    /// Check if variables are mutable
    bool isMutable() const { return is_mutable_; }
    
    /// Get the tuple expression being destructured
    Expr* getInit() const { return init_.get(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<std::string> names_;  ///< Variable names
    bool is_mutable_;                 ///< Mutability for all variables
    ExprPtr init_;                    ///< Tuple expression
};

/// StructDestructuringDecl - Struct destructuring declaration
///
/// Represents destructuring a struct into field variables.
///
/// Syntax:
///   let StructName { field1, field2 } = struct_expr
///   let~ StructName { x, y } = point
///
/// Examples:
///   let Point { x, y } = get_point();
///   let~ Person { name, age } = person;
///   let Point { x: px, y: py } = point;     - Rename fields
///
/// Type checking:
///   - init expression must have Struct type matching struct_name
///   - All field names must exist in struct definition
///   - Each variable gets corresponding field type
///
/// Field selection:
///   - Can select subset of fields
///   - Can rename fields during destructuring
///   - Duplicate field names cause error
///
/// Code generation:
///   - Load struct value
///   - GEP to access each selected field
///   - Store to individual variables
class StructDestructuringDecl : public Stmt {
public:
    /// Constructs a StructDestructuringDecl
    /// @param struct_name Struct type name
    /// @param field_names Field names to destructure
    /// @param is_mutable true for let~, false for let
    /// @param init Struct expression to destructure
    StructDestructuringDecl(std::string struct_name, std::vector<std::string> field_names, 
                           bool is_mutable, ExprPtr init)
        : struct_name_(std::move(struct_name)), field_names_(std::move(field_names)), 
          is_mutable_(is_mutable), init_(std::move(init)) {}
    
    /// Get the struct type name
    const std::string& getStructName() const { return struct_name_; }
    
    /// Get the field names being destructured
    const std::vector<std::string>& getFieldNames() const { return field_names_; }
    
    /// Check if variables are mutable
    bool isMutable() const { return is_mutable_; }
    
    /// Get the struct expression being destructured
    Expr* getInit() const { return init_.get(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string struct_name_;                ///< Struct type name
    std::vector<std::string> field_names_;   ///< Fields to destructure
    bool is_mutable_;                        ///< Mutability for all variables
    ExprPtr init_;                           ///< Struct expression
};

/// FunctionDecl - Function declaration
///
/// Represents a function definition with optional generic parameters and where clauses.
///
/// Syntax:
///   fn name(param1: Type1, param2: Type2) -> ReturnType { body }
///   fn name<T>(param: T) -> T where T: Display { body }
///   pub fn name() -> void { body }
///
/// Examples:
///   fn add(x: i32, y: i32) -> i32 { return x + y; }
///   fn swap<T, U>(a: T, b: U) -> (U, T) { return (b, a); }
///   fn print<T>(val: T) where T: Display { val.show(); }
///
/// Generic functions:
///   - Generic parameters in <T, U, V>
///   - Where clauses constrain generic parameters
///   - Monomorphized at call sites with concrete types
///
/// Visibility:
///   - pub: Public, accessible from other modules
///   - Default: Private, module-local only
///
/// Self type support:
///   - Parameters can use Self type in interface methods
///   - Resolved during type checking to implementing type
///   - resolved_param_types_ and resolved_return_type_ store resolved types
///
/// Code generation:
///   - Non-generic: Direct LLVM function generation
///   - Generic: Template stored, instantiated on demand
///   - Two-pass: Declaration first, then body (supports forward references)
class FunctionDecl : public Stmt {
public:
    /// Function parameter specification
    struct Param {
        std::string name;   ///< Parameter name
        Type* type;         ///< Parameter type (may contain Self)
        bool is_mutable;    ///< true for mutable parameters
    };
    
    FunctionDecl(const std::string& name, 
                 std::vector<GenericParam> generic_params,
                 std::vector<Param> params,
                 Type* return_type, 
                 StmtPtr body,
                 std::vector<WhereClause> where_clauses = {},
                 bool is_public = false)
        : name_(name), 
          generic_params_(std::move(generic_params)),
          params_(std::move(params)),
          return_type_(return_type), 
          body_(std::move(body)),
          where_clauses_(std::move(where_clauses)),
          is_public_(is_public) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<Param>& getParams() const { return params_; }
    Type* getReturnType() const { return return_type_; }
    Stmt* getBody() const { return body_.get(); }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    bool isPublic() const { return is_public_; }
    
    bool isGeneric() const { return !generic_params_.empty(); }
    
    // 🔧 Self type support: type after parsing (set by TypeChecker)
    void setResolvedTypes(const std::vector<Type*>& params, Type* ret) {
        resolved_param_types_ = params;
        resolved_return_type_ = ret;
    }
    
    bool hasResolvedTypes() const { return resolved_return_type_ != nullptr; }
    const std::vector<Type*>& getResolvedParamTypes() const { return resolved_param_types_; }
    Type* getResolvedReturnType() const { return resolved_return_type_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
    std::vector<GenericParam> generic_params_;  // genericparameter <T, U>
    std::vector<Param> params_;
    Type* return_type_;
    StmtPtr body_;
    std::vector<WhereClause> where_clauses_;   // whereconstraint
    bool is_public_;                           // Public visibility
    
    // 🔧 Selftypessupport：parseback/afterof/thetypes（Self -> actualtypes）
    std::vector<Type*> resolved_param_types_;
    Type* resolved_return_type_ = nullptr;
};

/// ReturnStmt - Return statement
///
/// Returns a value from a function and exits.
///
/// Syntax: return expression;
///
/// Examples:
///   return 42;
///   return ok(value);
///   return;                - Returns void (value is nullptr)
///
/// Type checking:
///   - Value type must match function return type
///   - Void functions can have empty return
///   - Non-void functions require value expression
///
/// Code generation: LLVM ret instruction
class ReturnStmt : public Stmt {
public:
    explicit ReturnStmt(ExprPtr value) : value_(std::move(value)) {}
    Expr* getValue() const { return value_.get(); }
    void accept(ASTVisitor* visitor) override;
private:
    ExprPtr value_;  ///< Return value (nullptr for void)
};

/// IfStmt - Conditional statement
///
/// Syntax: if condition { then_block } else { else_block }
///
/// Examples:
///   if x > 0 { println("positive"); }
///   if is_valid { process(); } else { error(); }
///
/// Type checking: condition must be bool
///
/// Code generation: Conditional branches with basic blocks (then_bb, else_bb, merge_bb)
class IfStmt : public Stmt {
public:
    IfStmt(ExprPtr condition, StmtPtr then_stmt, StmtPtr else_stmt)
        : condition_(std::move(condition)),
          then_stmt_(std::move(then_stmt)),
          else_stmt_(std::move(else_stmt)) {}
    
    Expr* getCondition() const { return condition_.get(); }
    Stmt* getThenStmt() const { return then_stmt_.get(); }
    Stmt* getElseStmt() const { return else_stmt_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr condition_;   ///< Boolean condition
    StmtPtr then_stmt_;   ///< Then branch
    StmtPtr else_stmt_;   ///< Else branch (may be nullptr)
};

/// LoopStmt - Infinite loop statement
///
/// Syntax: loop { body }
///
/// Infinite loop, exits only via break or return.
///
/// Code generation: Creates loop basic block with unconditional back-edge
class LoopStmt : public Stmt {
public:
    explicit LoopStmt(StmtPtr body) : body_(std::move(body)) {}
    Stmt* getBody() const { return body_.get(); }
    void accept(ASTVisitor* visitor) override;
private:
    StmtPtr body_;  ///< Loop body
};

/// WhileStmt - While loop statement
///
/// Syntax: loop condition { body } (desugars to loop + if + break)
///
/// Code generation: Condition check at loop start, conditional break
class WhileStmt : public Stmt {
public:
    WhileStmt(ExprPtr condition, StmtPtr body)
        : condition_(std::move(condition)), body_(std::move(body)) {}
    
    Expr* getCondition() const { return condition_.get(); }
    Stmt* getBody() const { return body_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr condition_;  ///< Loop condition
    StmtPtr body_;       ///< Loop body
};

/// BreakStmt - Break statement (exit loop)
///
/// Syntax: break;
///
/// Code generation: Branch to loop exit basic block
class BreakStmt : public Stmt {
public:
    BreakStmt() = default;
    void accept(ASTVisitor* visitor) override;
};

/// ContinueStmt - Continue statement (next iteration)
///
/// Syntax: continue;
///
/// Code generation: Branch to loop header basic block
class ContinueStmt : public Stmt {
public:
    ContinueStmt() = default;
    void accept(ASTVisitor* visitor) override;
};

/// BlockStmt - Block statement
///
/// Represents a sequence of statements in a new scope.
///
/// Syntax: { stmt1; stmt2; ... }
///
/// Code generation: Sequential statement generation with scope management
class BlockStmt : public Stmt {
public:
    explicit BlockStmt(std::vector<StmtPtr> stmts)
        : stmts_(std::move(stmts)) {}
    
    const std::vector<StmtPtr>& getStmts() const { return stmts_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<StmtPtr> stmts_;  ///< Statements in block
};

/// ForStmt - Range-based for loop
///
/// Syntax: loop var_name in iterator { body }
///
/// Examples:
///   loop i in 0..10 { println(i); }
///   loop item in array { process(item); }
///   loop ch in 'a'..'z' { println(ch); }
///
/// Iterator types:
///   - Range expressions: 0..10, 1..=100
///   - Arrays: [T; N]
///   - Slices: [T]
///
/// Code generation: Desugars to while loop with iterator variable
class ForStmt : public Stmt {
public:
    ForStmt(std::string var_name, ExprPtr iterator, StmtPtr body)
        : var_name_(std::move(var_name)),
          iterator_(std::move(iterator)),
          body_(std::move(body)) {}
    
    const std::string& getVarName() const { return var_name_; }
    Expr* getIterator() const { return iterator_.get(); }
    Stmt* getBody() const { return body_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string var_name_;  ///< Loop variable name
    ExprPtr iterator_;      ///< Iterator expression (range, array, etc.)
    StmtPtr body_;          ///< Loop body
};

/// StructDecl - Struct type declaration
///
/// Syntax: type Name = struct { field1: Type1, field2: Type2 }
///
/// Examples:
///   type Point = struct { x: i32, y: i32 }
///   type Box<T> = struct { value: T }
///   type Pair<T, U> = struct { first: T, second: U }
///
/// Generic structs:
///   - Type parameters in <T, U, V>
///   - Fields can use generic parameters
///   - Monomorphized at instantiation sites
///
/// Code generation:
///   - Non-generic: LLVM struct type created immediately
///   - Generic: Template stored, instantiated on demand
///   - Field layout: Sequential in memory
class StructDecl : public Stmt {
public:
    using Field = std::pair<std::string, Type*>;  ///< (name, type) pair
    
    StructDecl(std::string name, 
               std::vector<GenericParam> generic_params,
               std::vector<Field> fields)
        : name_(std::move(name)), 
          generic_params_(std::move(generic_params)),
          fields_(std::move(fields)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<Field>& getFields() const { return fields_; }
    bool isGeneric() const { return !generic_params_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;                          ///< Struct name
    std::vector<GenericParam> generic_params_;  ///< Generic parameters <T, U>
    std::vector<Field> fields_;                 ///< Fields (name, type)
};

/// EnumVariant - Enum variant specification
///
/// Represents a single variant in an enum type, with optional associated data.
///
/// Examples:
///   Red                             - Simple variant (no data)
///   Some(T)                         - Single data variant
///   Move(i32, i32)                  - Multi-parameter variant
///
/// Data types:
///   - Empty: No associated data (unit variant)
///   - Single: One associated value
///   - Multiple: Tuple-like multiple values
struct EnumVariant {
    std::string name;                ///< Variant name
    std::vector<Type*> data_types;   ///< Associated data types (empty for unit variants)
    
    /// Constructs an EnumVariant (single parameter, backward compatible)
    /// @param n Variant name
    /// @param t Optional single data type
    EnumVariant(std::string n, Type* t = nullptr)
        : name(std::move(n)) {
        if (t) data_types.push_back(t);
    }
    
    /// Constructs an EnumVariant (multiple parameters)
    /// @param n Variant name
    /// @param types Vector of associated data types
    EnumVariant(std::string n, std::vector<Type*> types)
        : name(std::move(n)), data_types(std::move(types)) {}
    
    /// Check if this variant has associated data
    bool hasData() const { return !data_types.empty(); }
    
    /// Get number of associated data elements
    size_t getDataCount() const { return data_types.empty(); }
    
    /// Get single data type (for single-parameter variants)
    Type* getSingleDataType() const { return data_types.empty() ? nullptr : data_types[0]; }
};

/// EnumDecl - Enum type declaration
///
/// Syntax: type Name = enum { Variant1, Variant2(T), Variant3(T1, T2) }
///
/// Examples:
///   type Color = enum { Red, Green, Blue }
///   type Option<T> = enum { Some(T), None }
///   type Result<T, E> = enum { Ok(T), Err(E) }
///
/// Variants:
///   - Unit variants: No associated data
///   - Data variants: Single or multiple associated values
///   - Accessed via pattern matching
///
/// Generic enums:
///   - Type parameters in <T, E>
///   - Monomorphized per concrete type
///
/// Code generation:
///   - Tagged union representation
///   - Tag discriminant + data payload
class EnumDecl : public Stmt {
public:
    EnumDecl(std::string name, 
             std::vector<GenericParam> generic_params,
             std::vector<EnumVariant> variants)
        : name_(std::move(name)), 
          generic_params_(std::move(generic_params)),
          variants_(std::move(variants)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<EnumVariant>& getVariants() const { return variants_; }
    bool isGeneric() const { return !generic_params_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;                          ///< Enum name
    std::vector<GenericParam> generic_params_;  ///< Generic parameters <T, U>
    std::vector<EnumVariant> variants_;         ///< Enum variants
};

/// InterfaceMethod - Interface method declaration
///
/// Represents a method signature in an interface, with optional default implementation.
///
/// Syntax:
///   fn method_name(params) -> ReturnType;                    - Abstract method
///   fn method_name(params) -> ReturnType { body }            - Default implementation
///
/// Examples:
///   fn show();                                              - Abstract, no params
///   fn display() { println(self.to_string()); }             - Default impl
///
/// Default implementations:
///   - Can reference 'self' and call other interface methods
///   - Implemented types can override or use default
///   - Body is an expression (BlockExpr typically)
struct InterfaceMethod {
    std::string name;                     ///< Method name
    std::vector<FunctionDecl::Param> params;  ///< Method parameters
    Type* return_type;                    ///< Return type
    ExprPtr body;                         ///< Optional default implementation
    
    InterfaceMethod(std::string n, std::vector<FunctionDecl::Param> p, Type* r, ExprPtr b = nullptr)
        : name(std::move(n)), params(std::move(p)), return_type(r), body(std::move(b)) {}
    
    /// Check if this method has a default implementation
    bool hasDefaultImpl() const { return body != nullptr; }
};

/// InterfaceDecl - Interface type declaration
///
/// Syntax: type Name = interface { fn method1(); fn method2() { default } }
///
/// Examples:
///   type Display = interface { fn show(); }
///   type Comparable<T> = interface { fn compare(other: T) -> i32; }
///   type Iterator<T> = interface {
///       fn next() -> T?;
///       fn has_next() -> bool { return self.next() != none; }
///   }
///
/// Features:
///   - Abstract method signatures
///   - Default implementations (v0.2.2)
///   - Generic interfaces with type parameters
///   - Self type for method receivers
///
/// Implementations:
///   - Types implement interfaces via 'support' blocks
///   - Can override default methods
///   - Where clauses can constrain generic parameters
class InterfaceDecl : public Stmt {
public:
    InterfaceDecl(std::string name, 
                  std::vector<GenericParam> generic_params,
                  std::vector<InterfaceMethod> methods)
        : name_(std::move(name)), 
          generic_params_(std::move(generic_params)),
          methods_(std::move(methods)) {}
    
    const std::string& getName() const { return name_; }
    const std::vector<GenericParam>& getGenericParams() const { return generic_params_; }
    const std::vector<InterfaceMethod>& getMethods() const { return methods_; }
    bool isGeneric() const { return !generic_params_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;                          ///< Interface name
    std::vector<GenericParam> generic_params_;  ///< Generic parameters
    std::vector<InterfaceMethod> methods_;      ///< Method signatures and defaults
};

/// SupportDecl - Interface implementation block
///
/// Syntax: support Type<T> with Interface<U> where T: Constraint { methods }
///
/// Examples:
///   support Point with Display { fn show() { ... } }
///   support Box<T> with Display where T: Display { ... }
///   support Vec<T> with Comparable<Vec<T>> where T: Ord { ... }
///
/// Features:
///   - Implements interface methods for a type
///   - Supports generic types and interfaces
///   - Where clauses constrain generic parameters
///   - Can use default implementations from interface
///
/// Method resolution:
///   - All abstract methods must be implemented
///   - Default methods can be overridden or inherited
///   - Methods can reference 'self'
///
/// Code generation:
///   - Static dispatch: Methods mangled as TypeName_methodName
///   - Generic: Monomorphized for each concrete type instantiation
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
    const std::vector<GenericParam>& getTypeGenericParams() const { return type_generic_params_; }
    const std::string& getInterfaceName() const { return interface_name_; }
    const std::vector<GenericParam>& getInterfaceGenericParams() const { return interface_generic_params_; }
    const std::vector<std::unique_ptr<FunctionDecl>>& getMethods() const { return methods_; }
    const std::vector<WhereClause>& getWhereClauses() const { return where_clauses_; }
    
    bool isGeneric() const { return !type_generic_params_.empty(); }
    bool isInterfaceGeneric() const { return !interface_generic_params_.empty(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string type_name_;                                 ///< Type being implemented for
    std::vector<GenericParam> type_generic_params_;         ///< Type's generic params
    std::string interface_name_;                            ///< Interface being implemented
    std::vector<GenericParam> interface_generic_params_;    ///< Interface generic params
    std::vector<std::unique_ptr<FunctionDecl>> methods_;    ///< Method implementations
    std::vector<WhereClause> where_clauses_;                ///< Where constraints
};

} // namespace pawc

#endif // PAW_STMT_H
