//===--- expr.h - Expression AST Nodes ---------------------------*- C++ -*-===//

#ifndef PAW_EXPR_H
#define PAW_EXPR_H

#include "ast_base.h"
#include "frontend/lexer/token.h"
#include <string>

namespace pawc {

/// IntLiteral - Integer literal expression
///
/// Represents a literal integer value in source code.
/// Stores the value as a string to preserve exact representation
/// and support arbitrary precision during parsing.
///
/// Examples: 42, 0, -100, 0x2A, 0b1010
///
/// Type inference: The exact integer type (i32, i64, etc.) is determined
/// during type checking based on context and suffix hints.
///
/// Code generation: Converted to LLVM ConstantInt with appropriate bit width.
class IntLiteral : public Expr {
public:
    /// Constructs an IntLiteral
    /// @param value String representation of the integer value
    explicit IntLiteral(const std::string& value) : value_(value) {}
    
    /// Get the string representation of this integer
    const std::string& getValue() const { return value_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string value_;  ///< String representation (preserves exact form from source)
};

/// FloatLiteral - Floating-point literal expression
///
/// Represents a literal floating-point value in source code.
/// Stored as string to preserve precision and format.
///
/// Examples: 3.14, 0.5, 1.0e10, 2.5f32
///
/// Type inference: Float type (f32, f64, etc.) determined by suffix or context.
///
/// Code generation: Converted to LLVM ConstantFP with appropriate precision.
class FloatLiteral : public Expr {
public:
    /// Constructs a FloatLiteral
    /// @param value String representation of the floating-point value
    explicit FloatLiteral(const std::string& value) : value_(value) {}
    
    /// Get the string representation of this float
    const std::string& getValue() const { return value_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string value_;  ///< String representation of the float value
};

/// BoolLiteral - Boolean literal expression
///
/// Represents the literal values 'true' or 'false'.
///
/// Type: Always has type 'bool'
///
/// Code generation: Converted to LLVM i1 constant (0 or 1).
class BoolLiteral : public Expr {
public:
    /// Constructs a BoolLiteral
    /// @param value The boolean value (true or false)
    explicit BoolLiteral(bool value) : value_(value) {}
    
    /// Get the boolean value
    bool getValue() const { return value_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    bool value_;  ///< The boolean value
};

/// CharLiteral - Character literal expression
///
/// Represents a single character literal enclosed in single quotes.
///
/// Examples: 'a', 'Z', '\n', '\t'
///
/// Type: Always has type 'char' (represented as i8 in LLVM)
///
/// Code generation: Converted to LLVM i8 constant.
class CharLiteral : public Expr {
public:
    /// Constructs a CharLiteral
    /// @param value The character value
    explicit CharLiteral(char value) : value_(value) {}
    
    /// Get the character value
    char getValue() const { return value_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    char value_;  ///< The character value
};

/// StringLiteral - String literal expression
///
/// Represents a string literal enclosed in double quotes.
///
/// Examples: "hello", "world\n", ""
///
/// Type: Always has type 'string'
///
/// Code generation: Converted to LLVM global constant string.
/// The string data is stored in the program's data section.
class StringLiteral : public Expr {
public:
    /// Constructs a StringLiteral
    /// @param value The string value (without quotes)
    explicit StringLiteral(const std::string& value) : value_(value) {}
    
    /// Get the string value
    const std::string& getValue() const { return value_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string value_;  ///< The string content (excluding quotes)
};

/// IdentifierExpr - Variable or function identifier reference
///
/// Represents a reference to a variable, function, or other named entity.
/// The actual meaning is resolved during semantic analysis by looking up
/// the name in the symbol table.
///
/// Examples: x, calculate, MyStruct
///
/// Resolution:
///   - Variables: Resolved to memory location
///   - Functions: Resolved to function pointer
///   - Types: Used in type expressions (not as IdentifierExpr)
///
/// Monomorphization: The name may be modified during generic instantiation
/// to refer to specialized versions (e.g., "foo" -> "foo$i32$i64").
class IdentifierExpr : public Expr {
public:
    /// Constructs an IdentifierExpr
    /// @param name The identifier name
    explicit IdentifierExpr(const std::string& name) : name_(name) {}
    
    /// Get the identifier name
    const std::string& getName() const { return name_; }
    
    /// Set the identifier name (used during generic monomorphization)
    /// @param name New name (e.g., mangled name for specialized function)
    void setName(const std::string& name) { name_ = name; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;  ///< The identifier name (may be mangled for generics)
};

/// SelfExpr - 'self' keyword expression
///
/// Represents the 'self' parameter in interface methods and support blocks.
/// 'self' refers to the instance on which the method is called.
///
/// Usage contexts:
///   - Interface method default implementations
///   - Support block method implementations
///
/// Type: The type of 'self' is the implementing type (struct, enum, etc.)
///
/// Code generation: Resolved to the first parameter of the method function.
class SelfExpr : public Expr {
public:
    SelfExpr() = default;
    void accept(ASTVisitor* visitor) override;
};

/// BinaryExpr - Binary operation expression
///
/// Represents any binary operation with a left operand, operator, and right operand.
///
/// Supported operators:
///   - Arithmetic: +, -, *, /, %
///   - Comparison: ==, !=, <, >, <=, >=
///   - Logical: &&, ||
///   - Bitwise: &, |, ^, <<, >>
///
/// Type checking:
///   - Both operands must have compatible types
///   - Result type depends on operator and operand types
///   - Comparison operators always return bool
///
/// Code generation:
///   - Arithmetic: LLVM arithmetic instructions (Add, Sub, Mul, etc.)
///   - Comparison: LLVM comparison instructions (ICmp, FCmp)
///   - Logical: Short-circuit evaluation with basic blocks
class BinaryExpr : public Expr {
public:
    /// Constructs a BinaryExpr
    /// @param op The binary operator
    /// @param left Left operand expression
    /// @param right Right operand expression
    BinaryExpr(TokenType op, ExprPtr left, ExprPtr right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {}
    
    /// Get the operator
    TokenType getOperator() const { return op_; }
    
    /// Get the left operand
    Expr* getLeft() const { return left_.get(); }
    
    /// Get the right operand
    Expr* getRight() const { return right_.get(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    TokenType op_;     ///< The binary operator token
    ExprPtr left_;     ///< Left operand expression
    ExprPtr right_;    ///< Right operand expression
};

/// UnaryExpr - Unary operation expression
///
/// Represents unary operations with a single operand.
///
/// Supported operators:
///   - Arithmetic negation: -
///   - Logical negation: !
///   - Bitwise negation: ~
///   - Dereference: * (for pointers/references)
///   - Reference: & (creates reference)
///
/// Type checking:
///   - Operand type must support the operator
///   - Result type depends on operator:
///     * -: same as operand (numeric types)
///     * !: bool (operand must be bool)
///     * ~: same as operand (integer types)
///     * *: T for &T or &~T (dereference)
///     * &: &T for T (create immutable reference)
///
/// Code generation:
///   - Arithmetic/logical/bitwise: LLVM unary instructions
///   - Reference operations: Load/Store instructions or pointer manipulation
class UnaryExpr : public Expr {
public:
    /// Constructs a UnaryExpr
    /// @param op The unary operator
    /// @param operand The operand expression
    UnaryExpr(TokenType op, ExprPtr operand)
        : op_(op), operand_(std::move(operand)) {}
    
    /// Get the operator
    TokenType getOperator() const { return op_; }
    
    /// Get the operand expression
    Expr* getOperand() const { return operand_.get(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    TokenType op_;      ///< The unary operator token
    ExprPtr operand_;   ///< The operand expression
};

/// CallExpr - Function or method call expression
///
/// Represents a function call with optional generic type parameters.
/// Supports both regular function calls and interface method dispatch.
///
/// Syntax:
///   function(arg1, arg2)          - Regular function call
///   function<T>(arg)              - Generic function with explicit type args
///   object.method(args)           - Method call (desugared to this form)
///
/// Examples:
///   println("hello")              - Builtin function call
///   add<i32>(1, 2)                - Generic function with type argument
///   point.distance(other)         - Method call on struct
///   value.show()                  - Interface method call
///
/// Generic functions:
///   - Type arguments can be explicit: foo<i32, string>(x, y)
///   - Or inferred: foo(x, y) where types determined from arguments
///   - Type arguments stored in type_args_ vector
///
/// Method calls:
///   - Set is_method_call_ = true during type checking
///   - receiver_ points to the object expression
///   - method_target_ contains the mangled method name (e.g., "Point_distance")
///
/// Code generation:
///   - Regular calls: LLVM call instruction
///   - Method calls: Static dispatch to mangled method name
///   - Generic calls: Dispatch to monomorphized version
class CallExpr : public Expr {
public:
    /// Constructs a CallExpr
    /// @param callee Expression evaluating to callable (function name or lambda)
    /// @param args Vector of argument expressions
    CallExpr(ExprPtr callee, std::vector<ExprPtr> args)
        : callee_(std::move(callee)), args_(std::move(args)) {}
    
    /// Get the callee expression
    Expr* getCallee() const { return callee_.get(); }
    
    /// Get the argument expressions
    const std::vector<ExprPtr>& getArgs() const { return args_; }
    
    // Generic function support
    
    /// Get the explicit type arguments (for generic calls)
    const std::vector<Type*>& getTypeArgs() const { return type_args_; }
    
    /// Set type arguments (during parsing or type inference)
    /// @param type_args Vector of concrete types for generic parameters
    void setTypeArgs(const std::vector<Type*>& type_args) { type_args_ = type_args; }
    
    /// Check if this call has explicit type arguments
    bool hasTypeArgs() const { return !type_args_.empty(); }
    
    // Interface method call support (set during type checking)
    
    /// Check if this is a method call (vs regular function call)
    bool isMethodCall() const { return is_method_call_; }
    
    /// Mark this as a method call
    /// @param is true if this is a method call
    void setIsMethodCall(bool is) { is_method_call_ = is; }
    
    /// Get the receiver object (for method calls)
    /// @return Pointer to receiver expression (not owned)
    Expr* getReceiver() const { return receiver_; }
    
    /// Set the receiver object
    /// @param recv Pointer to receiver (must outlive this CallExpr)
    void setReceiver(Expr* recv) { receiver_ = recv; }
    
    /// Get the method name (unqualified)
    const std::string& getMethodName() const { return method_name_; }
    
    /// Set the method name
    /// @param name Method name without type prefix
    void setMethodName(const std::string& name) { method_name_ = name; }
    
    /// Get the fully qualified method target name
    /// @return Mangled name like "TypeName_methodName"
    const std::string& getMethodTarget() const { return method_target_; }
    
    /// Set the method target (mangled name)
    /// @param target Fully qualified method name
    void setMethodTarget(const std::string& target) { method_target_ = target; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr callee_;                      ///< Callee expression (function name or lambda)
    std::vector<ExprPtr> args_;           ///< Argument expressions
    std::vector<Type*> type_args_;        ///< Explicit generic type arguments (e.g., <i32>)
    
    // Method call metadata (set by TypeChecker)
    bool is_method_call_ = false;         ///< true if this is a method call
    Expr* receiver_ = nullptr;            ///< Receiver object (not owned, references MemberExpr)
    std::string method_name_;             ///< Unqualified method name
    std::string method_target_;           ///< Fully qualified target (e.g., "Point_distance")
};

/// MemberExpr - Member access expression
///
/// Represents accessing a field of a struct or calling a method on an object.
///
/// Syntax: object.member
///
/// Examples:
///   point.x                       - Access struct field
///   point.distance(other)         - Method call (CallExpr wraps this)
///   array.len                     - Access built-in property
///
/// Type checking:
///   - object must have a struct, enum, or interface type
///   - member must exist in that type's definition
///   - Result type is the field/method type
///
/// Code generation:
///   - Struct fields: GEP instruction to field offset
///   - Methods: Transformed into CallExpr with object as first argument
class MemberExpr : public Expr {
public:
    /// Constructs a MemberExpr
    /// @param object Expression evaluating to object with members
    /// @param member Name of the member to access
    MemberExpr(ExprPtr object, const std::string& member)
        : object_(std::move(object)), member_(member) {}
    
    /// Get the object expression
    Expr* getObject() const { return object_.get(); }
    
    /// Get the member name
    const std::string& getMember() const { return member_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr object_;      ///< Object expression
    std::string member_;  ///< Member name to access
};

/// StaticAccessExpr - Static member access expression
///
/// Represents accessing a static member or enum variant using :: notation.
///
/// Syntax: TypeName::member
///
/// Examples:
///   Color::Red                    - Enum variant
///   Option::Some                  - Enum constructor
///   Math::PI                      - Static constant (future feature)
///
/// Usage:
///   - Primarily for enum variant construction
///   - Type name must refer to an enum or interface
///   - Member must be a valid variant or static member
///
/// Type checking:
///   - Resolves TypeName to actual type definition
///   - Validates member exists in that type
///   - For enum variants, may wrap constructor arguments
///
/// Code generation:
///   - Enum variants: Generate variant constructor call
///   - Static members: Direct reference to global/constant
class StaticAccessExpr : public Expr {
public:
    /// Constructs a StaticAccessExpr
    /// @param type_name Name of the type (left side of ::)
    /// @param member Name of the static member (right side of ::)
    StaticAccessExpr(const std::string& type_name, const std::string& member)
        : type_name_(type_name), member_(member) {}
    
    /// Get the type name
    const std::string& getTypeName() const { return type_name_; }
    
    /// Get the member name
    const std::string& getMember() const { return member_; }
    
    /// Set the type name (used during monomorphization)
    /// @param name New type name (potentially mangled)
    void setTypeName(const std::string& name) { type_name_ = name; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string type_name_;  ///< Type name (e.g., "Color", "Option")
    std::string member_;     ///< Member name (e.g., "Red", "Some")
};

/// IndexExpr - Array/Slice indexing expression
///
/// Represents indexing into arrays, slices, or other indexable types.
///
/// Syntax: object[index]
///
/// Examples:
///   array[0]                      - Access first element
///   matrix[i][j]                  - Multi-dimensional indexing
///   slice[len - 1]                - Dynamic index expression
///
/// Type checking:
///   - object must have Array or Slice type
///   - index must be integer type (i32, i64, usize, etc.)
///   - Result type is the element type of the array/slice
///
/// Bounds checking:
///   - Array: Compile-time bounds checking when index is constant
///   - Slice: Runtime bounds checking (panics on out-of-bounds)
///
/// Code generation:
///   - Array: GEP instruction with bounds check
///   - Slice: Extract data pointer, check bounds, load element
class IndexExpr : public Expr {
public:
    /// Constructs an IndexExpr
    /// @param object Expression evaluating to array or slice
    /// @param index Expression evaluating to integer index
    IndexExpr(ExprPtr object, ExprPtr index)
        : object_(std::move(object)), index_(std::move(index)) {}
    
    /// Get the object being indexed
    Expr* getObject() const { return object_.get(); }
    
    /// Get the index expression
    Expr* getIndex() const { return index_.get(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr object_;  ///< Object to index into (array or slice)
    ExprPtr index_;   ///< Index expression (must evaluate to integer)
};

/// IfExpr - Conditional expression
///
/// Represents a ternary conditional expression that returns a value.
///
/// Syntax: condition if then_value else else_value
///
/// Examples:
///   let x = value if is_valid else default;
///   return ok(x) if x > 0 else err("negative");
///
/// Type checking:
///   - condition must have type bool
///   - then_expr and else_expr must have compatible types
///   - Result type is the common type of both branches
///
/// Code generation:
///   - Creates two basic blocks (then_bb, else_bb, merge_bb)
///   - Phi node to merge values from both branches
///   - Short-circuit evaluation: only executed branch is evaluated
class IfExpr : public Expr {
public:
    /// Constructs an IfExpr
    /// @param condition Boolean condition expression
    /// @param then_expr Expression to evaluate if condition is true
    /// @param else_expr Expression to evaluate if condition is false
    IfExpr(ExprPtr condition, ExprPtr then_expr, ExprPtr else_expr)
        : condition_(std::move(condition)),
          then_expr_(std::move(then_expr)),
          else_expr_(std::move(else_expr)) {}
    
    Expr* getCondition() const { return condition_.get(); }
    Expr* getThenExpr() const { return then_expr_.get(); }
    Expr* getElseExpr() const { return else_expr_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr condition_;  ///< Boolean condition
    ExprPtr then_expr_;  ///< Value if condition is true
    ExprPtr else_expr_;  ///< Value if condition is false
};

/// BlockExpr - Block expression
///
/// Represents a block of statements that evaluates to a value.
///
/// Syntax: { stmt1; stmt2; ...; expr }
///
/// The last expression (without semicolon) is the block's value.
///
/// Examples:
///   let x = { let y = 10; y * 2 };        // x = 20
///   { println("start"); calculate(); }     // Block with side effects
///
/// Type checking:
///   - All statements are type-checked in order
///   - Block creates a new scope for variables
///   - Result type is the type of the last expression (or void)
///
/// Code generation:
///   - Statements generated sequentially
///   - Last expression's value is the block results
///   - Variables scoped to block (stack cleanup on exit)
class BlockExpr : public Expr {
public:
    /// Constructs a BlockExpr
    /// @param stmts Sequence of statements (last may be expression)
    explicit BlockExpr(std::vector<StmtPtr> stmts)
        : stmts_(std::move(stmts)) {}
    
    const std::vector<StmtPtr>& getStmts() const { return stmts_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<StmtPtr> stmts_;  ///< Statements in block
};

/// ArrayLiteral - Array literal expression
///
/// Represents array construction with explicit elements.
///
/// Syntax: [element1, element2, ...]
///
/// Examples:
///   [1, 2, 3]                      - Array of 3 integers
///   [Point{x:0,y:0}, Point{x:1,y:1}]  - Array of structs
///
/// Type inference:
///   - Element types must be compatible
///   - Array type: [T; N] where T is element type, N is count
///
/// Code generation:
///   - Stack allocation for array
///   - Sequential store for each element
class ArrayLiteral : public Expr {
public:
    /// Constructs an ArrayLiteral
    /// @param elements Vector of element expressions
    explicit ArrayLiteral(std::vector<ExprPtr> elements)
        : elements_(std::move(elements)) {}
    
    const std::vector<ExprPtr>& getElements() const { return elements_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<ExprPtr> elements_;  ///< Element expressions
};

/// TupleExpr - Tuple literal expression
///
/// Represents tuple construction.
///
/// Syntax: (element1, element2, ...)
///
/// Examples:
///   (1, "hello")                   - Tuple of i32 and string
///   (x, y, z)                      - Tuple of three values
///
/// Type: (T1, T2, ..., Tn) where Ti is type of ith element
///
/// Code generation:
///   - Struct with fields for each element
///   - Sequential field initialization
class TupleExpr : public Expr {
public:
    /// Constructs a TupleExpr
    /// @param elements Vector of element expressions
    explicit TupleExpr(std::vector<ExprPtr> elements)
        : elements_(std::move(elements)) {}
    
    const std::vector<ExprPtr>& getElements() const { return elements_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<ExprPtr> elements_;  ///< Tuple element expressions
};

/// RangeExpr - Range expression (for iteration)
///
/// Represents a range of values for use in loop iteration.
///
/// Syntax:
///   start..end         - Exclusive range [start, end)
///   start..=end        - Inclusive range [start, end]
///
/// Examples:
///   0..10              - Range from 0 to 9
///   1..=100            - Range from 1 to 100 (inclusive)
///   'a'..'z'           - Character range
///
/// Usage:
///   - Primarily in loop iteration: loop i in 0..10 { ... }
///   - Not the same as RangePattern (which is for pattern matching)
///
/// Type: Iterator type (implementation-defined)
///
/// Code generation:
///   - Generates loop with start, end, and increment logic
class RangeExpr : public Expr {
public:
    /// Constructs a RangeExpr
    /// @param start Starting value (inclusive)
    /// @param end Ending value
    /// @param inclusive true for ..=, false for ..
    RangeExpr(ExprPtr start, ExprPtr end, bool inclusive)
        : start_(std::move(start)),
          end_(std::move(end)),
          inclusive_(inclusive) {}
    
    Expr* getStart() const { return start_.get(); }
    Expr* getEnd() const { return end_.get(); }
    bool isInclusive() const { return inclusive_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr start_;      ///< Start of range (inclusive)
    ExprPtr end_;        ///< End of range
    bool inclusive_;     ///< true for ..=, false for ..
};

/// FieldInit - Field initialization for struct literals
///
/// Represents a single field assignment in struct literal syntax.
///
/// Syntax: field_name: value_expression
///
/// Example: x: 10, name: "Alice"
///
/// Used in: StructLiteral construction
struct FieldInit {
    std::string name;  ///< Field name
    ExprPtr value;     ///< Field value expression
    
    /// Constructs a FieldInit
    /// @param n Field name
    /// @param v Field value expression
    FieldInit(std::string n, ExprPtr v)
        : name(std::move(n)), value(std::move(v)) {}
};

/// StructLiteral - Struct literal expression
///
/// Represents struct construction with field initialization.
///
/// Syntax: StructName { field1: value1, field2: value2 }
///
/// Examples:
///   Point { x: 10, y: 20 }
///   Person { name: "Alice", age: 30 }
///   Point { x: 0, ..other }         - Future: field spread syntax
///
/// Type checking:
///   - struct_name must refer to a defined struct type
///   - All required fields must be initialized
///   - Field types must match struct definition
///   - Extra fields cause error
///
/// Field order:
///   - Fields can be in any order
///   - Missing fields cause error
///   - Duplicate fields cause error
///
/// Code generation:
///   - Stack allocation for struct
///   - GEP + store for each field
///   - Fields stored in definition order (not initialization order)
///
/// Monomorphization:
///   - struct_name may be mangled for generic structs (e.g., "Box$i32")
class StructLiteral : public Expr {
public:
    /// Constructs a StructLiteral
    /// @param struct_name Name of the struct type
    /// @param fields Vector of field initializations
    StructLiteral(std::string struct_name, std::vector<FieldInit> fields)
        : struct_name_(std::move(struct_name)), fields_(std::move(fields)) {}
    
    /// Get the struct type name
    const std::string& getStructName() const { return struct_name_; }
    
    /// Set the struct name (used during monomorphization)
    /// @param name New struct name (potentially mangled)
    void setStructName(const std::string& name) { struct_name_ = name; }
    
    /// Get the field initializations (const)
    const std::vector<FieldInit>& getFields() const { return fields_; }
    
    /// Get the field initializations (mutable, for monomorphization pass)
    std::vector<FieldInit>& getFields() { return fields_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string struct_name_;         ///< Struct type name (may be mangled for generics)
    std::vector<FieldInit> fields_;   ///< Field initializations
};

/// MatchArm - A single branch in a match expression
///
/// Represents one case in a match expression with optional guard condition.
///
/// Syntax:
///   pattern => expression           - Basic match arm
///   pattern if guard => expression  - Match arm with guard condition
///
/// Examples:
///   Some(x) => x                          - Match Some variant
///   Some(x) if x > 0 => x * 2             - Match Some with guard
///   [first, .., last] if first == last => first  - Array pattern with guard
///
/// Execution flow:
///   1. Check if the pattern matches
///   2. If matched, bind variables from the pattern
///   3. If guard exists, evaluate guard condition (must be bool)
///   4. If guard passes (or no guard), evaluate and return expression
///   5. If pattern doesn't match or guard fails, try next arm
///
/// Guard conditions:
///   - Optional boolean expression
///   - Can access variables bound by the pattern
///   - Evaluated after pattern matching succeeds
///   - If false, continue to next arm (not a match)
struct MatchArm {
    std::unique_ptr<class Pattern> pattern;  ///< Pattern to match against
    ExprPtr guard;                           ///< Optional guard condition (if clause)
    ExprPtr expression;                      ///< Expression to evaluate if matched
    
    /// Constructs a MatchArm
    /// @param p Pattern to match
    /// @param e Expression to evaluate on successful match
    /// @param g Optional guard condition (must evaluate to bool)
    MatchArm(std::unique_ptr<class Pattern> p, ExprPtr e, ExprPtr g = nullptr)
        : pattern(std::move(p)), guard(std::move(g)), expression(std::move(e)) {}
};

/// MatchExpr - Pattern matching expression
///
/// Represents a match expression that matches a value against multiple patterns
/// and executes the corresponding branch.
///
/// Syntax:
///   value is {
///       pattern1 => results1,
///       pattern2 if guard => results2,
///       _ => default
///   }
///
/// Supported patterns (v0.2.2):
///   - Literals: 42, "hello", true, 'a'
///   - Variables: x, name (bind value)
///   - Wildcards: _ (ignore value)
///   - Tuples: (a, b, c)
///   - Arrays: [a, b, c]
///   - Structs: Point{x, y}
///   - Enums: Some(x), Ok(value)
///   - Slices: [first, .., last], [a, b, .., y, z]
///   - Ranges: 1..10, 'a'..'z', 90..=100
///   - OR: 1 | 2 | 3
///   - Nested: ((a, b), (c, d))
///   - Guards: pattern if condition
///
/// Type checking:
///   - Scrutinee is type-checked first
///   - All patterns must be compatible with scrutinee type
///   - All branch resultss must have compatible types
///   - Guards must be boolean expressions
///   - Exhaustiveness checking (warns if cases missing)
///
/// Code generation:
///   - Sequential if-else chain with pattern matching conditions
///   - Guard blocks inserted between pattern match and arm body
///   - Phi node to merge resultss from all branches
///   - Optimized: unreachable code eliminated
///
/// Performance: O(n) where n is number of arms (evaluated sequentially)
class MatchExpr : public Expr {
public:
    /// Constructs a MatchExpr
    /// @param scrutinee Expression to match against
    /// @param arms Vector of match arms (pattern => expr, with optional guard)
    MatchExpr(ExprPtr scrutinee, std::vector<MatchArm> arms)
        : scrutinee_(std::move(scrutinee)), arms_(std::move(arms)) {}
    
    /// Get the scrutinee expression (value being matched)
    Expr* getScrutinee() const { return scrutinee_.get(); }
    
    /// Get all match arms
    const std::vector<MatchArm>& getArms() const { return arms_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr scrutinee_;              ///< Expression being matched
    std::vector<MatchArm> arms_;     ///< All match arms (at least one required)
};

/// TryExpr - Try expression for error propagation
///
/// Automatically unwraps Result/Optional or propagates errors up the call stack.
///
/// Syntax:
///   expr?          - For Optional types: unwrap Some, return none on None
///   expr!          - For Result types: unwrap Ok, return Err on Err
///
/// Examples:
///   let value = might_fail()?;                    - Propagate Optional
///   let results = database_query()!;               - Propagate Result
///   let x = parse_int(input)? + parse_int(y)?;    - Chain operations
///
/// Type requirements:
///   - expr? requires expr to have Optional<T> type
///   - expr! requires expr to have Result<T, E> type
///   - Enclosing function must return compatible Optional/Result type
///
/// Semantics:
///   - Optional: If Some(v), unwrap to v; if none, early return none
///   - Result: If Ok(v), unwrap to v; if Err(e), early return Err(e)
///
/// Code generation:
///   - Check if expr is Some/Ok variant
///   - If success: unwrap value and continue
///   - If failure: early return from current function
///   - Uses LLVM conditional branches
class TryExpr : public Expr {
public:
    /// Constructs a TryExpr
    /// @param expr Expression to try (must be Optional or Result type)
    /// @param op Operator: QUESTION for ?, BANG for !
    TryExpr(ExprPtr expr, TokenType op) 
        : expr_(std::move(expr)), operator_(op) {}
    
    /// Get the inner expression being tried
    Expr* getExpr() const { return expr_.get(); }
    
    /// Get the try operator
    TokenType getOperator() const { return operator_; }
    
    /// Check if this is Optional try (?)
    bool isOptionalTry() const { return operator_ == TokenType::QUESTION; }
    
    /// Check if this is Result try (!)
    bool isResultTry() const { return operator_ == TokenType::BANG; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr expr_;       ///< Expression to try
    TokenType operator_; ///< QUESTION(?) for Optional, BANG(!) for Result
};

/// NoneLiteral - 'none' literal for Optional types
///
/// Represents the absence of a value in Optional types.
///
/// Syntax: none
///
/// Type: Inferred as Optional<T> based on context
///
/// Examples:
///   let x: i32? = none;
///   return none;
///
/// Code generation: Creates Optional variant with is_some = false
class NoneLiteral : public Expr {
public:
    NoneLiteral() = default;
    void accept(ASTVisitor* visitor) override;
};

/// CastExpr - Type cast expression
///
/// Represents explicit type conversion using 'as' keyword.
///
/// Syntax: expr as TargetType
///
/// Examples:
///   x as i64                       - Widen integer
///   value as f64                   - Int to float conversion
///   ptr as *const T                - Pointer cast (unsafe, future)
///
/// Supported conversions:
///   - Integer widening: i32 -> i64
///   - Integer to float: i32 -> f64
///   - Float conversion: f32 -> f64
///   - Numeric conversions (with potential precision loss)
///
/// Type checking:
///   - Validates conversion is allowed
///   - Some conversions may be unsafe (requires explicit cast)
///   - Errors on incompatible types (e.g., string -> i32)
///
/// Code generation:
///   - LLVM conversion instructions (ZExt, SExt, FPExt, etc.)
///   - May involve precision loss (truncation, rounding)
class CastExpr : public Expr {
public:
    /// Constructs a CastExpr
    /// @param expr Expression to cast
    /// @param target_type Target type to cast to
    CastExpr(ExprPtr expr, Type* target_type)
        : expr_(std::move(expr)), target_type_(target_type) {}
    
    /// Get the expression being cast
    Expr* getExpr() const { return expr_.get(); }
    
    /// Get the target type
    Type* getTargetType() const { return target_type_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    ExprPtr expr_;          ///< Expression to cast
    Type* target_type_;     ///< Target type for conversion
};

/// ClosureExpr - Lambda/Closure expression
///
/// Represents an anonymous function (lambda) with potential variable capture.
///
/// Syntax: (param1: Type1, param2: Type2) -> ReturnType { body }
///
/// Examples:
///   let add = (x: i32, y: i32) -> i32 { return x + y; };
///   let filter = |item: i32| -> bool { return item > 0; };
///   map(array, (x: i32) -> i32 { return x * 2; })
///
/// Closures vs Functions:
///   - Closures can capture variables from enclosing scope
///   - Functions cannot capture (static only)
///   - Closures have anonymous generated names
///
/// Variable capture:
///   - By value: Copies variable into closure environment
///   - By reference: Stores reference (for mutable variables)
///   - Capture list determined during semantic analysis
///
/// Type: Function type fn(T1, T2) -> R
///
/// Code generation:
///   - Closure -> regular function + environment struct (if captures exist)
///   - Captured variables passed as environment pointer
///   - Generated function name: closure$unique_id
class ClosureExpr : public Expr {
public:
    /// Closure parameter specification
    struct Param {
        std::string name;   ///< Parameter name
        Type* type;         ///< Parameter type
        bool is_mutable;    ///< true for mutable parameters
        
        /// Constructs a closure Param
        /// @param n Parameter name
        /// @param t Parameter type
        /// @param mut true if parameter is mutable
        Param(std::string n, Type* t, bool mut = false)
            : name(std::move(n)), type(t), is_mutable(mut) {}
    };
    
    /// Captured variable information
    struct CapturedVar {
        std::string name;      ///< Variable name
        Type* type;            ///< Variable type
        bool by_reference;     ///< true: capture by reference, false: by value
        
        /// Constructs a CapturedVar
        /// @param n Variable name
        /// @param t Variable type
        /// @param ref true for reference capture
        CapturedVar(std::string n, Type* t, bool ref = false)
            : name(std::move(n)), type(t), by_reference(ref) {}
    };
    
    ClosureExpr(std::vector<Param> params, Type* return_type, ExprPtr body)
        : params_(std::move(params)), 
          return_type_(return_type),
          body_(std::move(body)) {}
    
    const std::vector<Param>& getParams() const { return params_; }
    Type* getReturnType() const { return return_type_; }
    void setReturnType(Type* type) { return_type_ = type; }
    
    Expr* getBody() const { return body_.get(); }
    
    // capturevariablemanage
    const std::vector<CapturedVar>& getCapturedVars() const { return captured_vars_; }
    void addCapturedVar(const CapturedVar& var) { captured_vars_.push_back(var); }
    
    // generationof/theclosurefunction name（CodeGentime/whenset）
    const std::string& getGeneratedName() const { return generated_name_; }
    void setGeneratedName(const std::string& name) { generated_name_ = name; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<Param> params_;
    Type* return_type_;
    ExprPtr body_;
    std::vector<CapturedVar> captured_vars_;
    std::string generated_name_;  // generationof/thefunction name
};

} // namespace pawc

#endif // PAW_EXPR_H
