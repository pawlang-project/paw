//===--- type_checker.cpp - Type Checker Implementation ----------*- C++ -*-===//
/// @file type_checker.cpp
/// @brief Implementation file
///

#include "type_checker.h"
#include "capture_analyzer.h"
#include "frontend/parser/ast/pattern.h"
#include <set>
#include <map>

namespace pawc {

TypeChecker::TypeChecker(TypeSystem* type_system, SymbolTable* symbol_table,
                         DiagnosticEngine* diag_engine)
    : types_(type_system), symbols_(symbol_table), diag_(diag_engine) {}

void TypeChecker::check(const std::vector<StmtPtr>& stmts) {
    // Two-pass traversal:
    // First pass: register all types and function declarations (don't check function bodies)
    // Second pass: check all function bodies and expressions
    
    // First pass: collect declarations
    for (const auto& stmt : stmts) {
        if (auto* func = dynamic_cast<FunctionDecl*>(stmt.get())) {
            // Only register function, don't check function body
            std::vector<Type*> param_types;
            for (const auto& param : func->getParams()) {
                param_types.push_back(param.type);
            }
            FunctionType* func_type = types_->getFunctionType(param_types, func->getReturnType());
            symbols_->defineFunction(func->getName(), func_type);
        }
        else if (auto* struct_decl = dynamic_cast<StructDecl*>(stmt.get())) {
            // Struct already registered during monomorphization, skip
        }
        else if (auto* enum_decl = dynamic_cast<EnumDecl*>(stmt.get())) {
            // Enum declaration
            stmt->accept(this);
        }
        else if (auto* interface_decl = dynamic_cast<InterfaceDecl*>(stmt.get())) {
            // Interface declaration
            stmt->accept(this);
        }
    }
    
    // Second pass: complete type checking
    for (const auto& stmt : stmts) {
        stmt->accept(this);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Expression type checking
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::visit(IntLiteral* node) {
    // If there's an expected type, use it; otherwise default to i32
    if (expected_type_ && expected_type_->isInteger()) {
        node->setType(expected_type_);
    } else {
        node->setType(types_->getI32Type());  // Default i32
    }
}

void TypeChecker::visit(FloatLiteral* node) {
    // If there's an expected type, use it; otherwise default to f64
    if (expected_type_ && expected_type_->isFloat()) {
        node->setType(expected_type_);
    } else {
        node->setType(types_->getF64Type());  // Default f64
    }
}

void TypeChecker::visit(BoolLiteral* node) {
    node->setType(types_->getBoolType());
}

void TypeChecker::visit(CharLiteral* node) {
    node->setType(types_->getCharType());
}

void TypeChecker::visit(StringLiteral* node) {
    node->setType(types_->getStringType());
}

void TypeChecker::visit(NoneLiteral* node) {
    // None literal's type needs to be inferred from context
    // If in Optional<T> context, type should be Optional<T>
    // Otherwise default to Optional<void>
    
    // TODO: Infer from context (currently simplified to Optional<void>)
    // Should actually be set based on expected type in ReturnStmt or assignment
    node->setType(types_->getOptionalType(types_->getVoidType()));
}

void TypeChecker::visit(CastExpr* node) {
    // as type conversion: expr as TargetType
    node->getExpr()->accept(this);
    
    Type* source_type = node->getExpr()->getType();
    Type* target_type = node->getTargetType();
    
    if (!source_type || !target_type) {
        node->setType(types_->getVoidType());
        return;
    }
    
    // Validate type conversion legality
    // According to documentation, supports: between numeric types, array to slice
    bool is_valid = false;
    
    // 1. Conversion between numeric types
    if (source_type->isNumeric() && target_type->isNumeric()) {
        is_valid = true;
    }
    // 2. Array to slice
    else if (source_type->isArray() && target_type->isSlice()) {
        auto* array_type = static_cast<ArrayType*>(source_type);
        auto* slice_type = static_cast<SliceType*>(target_type);
        if (types_->equals(array_type->getElementType(), slice_type->getElementType())) {
            is_valid = true;
        }
    }
    
    if (!is_valid) {
        diag_->reportError("Invalid type cast from " + source_type->toString() + 
                          " to " + target_type->toString(),
                          node->getLocation());
    }
    
    node->setType(target_type);
}

void TypeChecker::visit(SelfExpr* node) {
    // self expression: use actual type of self parameter (may be reference type)
    if (current_self_param_type_) {
        // Resolve Self type: if &Self, resolve to &Point
        Type* resolved_type = resolveSelfType(current_self_param_type_);
        node->setType(resolved_type);
    } else if (current_self_type_) {
        // Fallback: use current_self_type_ (may be inaccurate)
        node->setType(current_self_type_);
    } else {
        node->setType(nullptr);
    }
}

void TypeChecker::visit(IdentifierExpr* node) {
    const std::string& name = node->getName();
    
    // Special handling: ok/err/some builtins (don't need types when used as CallExpr callee)
    if (name == "ok" || name == "err" || name == "some") {
        // Type determined by containing CallExpr
        node->setType(nullptr);  // Placeholder
        return;
    }
    
    Symbol* sym = symbols_->lookup(name);
    if (!sym) {
        // Special handling: self identifier (if not found in symbol table)
        if (name == "self" && current_self_type_) {
            // self's type is current support's type
            // But if it's a reference parameter, need to return reference type
            std::cerr << "⚠️ [TypeChecker] self not in symbol table, using current_self_type_: "
                      << current_self_type_->toString() << std::endl;
            node->setType(current_self_type_);
            return;
        }
        
        diag_->reportError("Undefined identifier: " + name,
                          node->getLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    if (sym->getKind() == Symbol::Kind::Variable) {
        auto* var_sym = static_cast<VariableSymbol*>(sym);
        Type* var_type = var_sym->getType();
        node->setType(var_type);
    } else {
        node->setType(types_->getVoidType());
    }
}

void TypeChecker::visit(BinaryExpr* node) {
    node->getLeft()->accept(this);
    node->getRight()->accept(this);
    
    Type* left_type = node->getLeft()->getType();
    Type* right_type = node->getRight()->getType();
    auto op = node->getOperator();
    
    // Comparison operators (return bool)
    if (op == TokenType::EQ_EQ || op == TokenType::NOT_EQ ||
        op == TokenType::LT || op == TokenType::LESS_EQ ||
        op == TokenType::GT || op == TokenType::GREATER_EQ) {
        
        // Special handling: Optional<T> vs null comparison
        // null's type is Optional<void>, but can compare with any Optional<T>
        bool is_optional_null_comparison = false;
        if ((left_type->isOptional() && right_type->isOptional())) {
            auto* left_opt = static_cast<OptionalType*>(left_type);
            auto* right_opt = static_cast<OptionalType*>(right_type);
            // If one is Optional<void> (null), allow comparison
            if (left_opt->getInnerType()->isVoid() || right_opt->getInnerType()->isVoid()) {
                is_optional_null_comparison = true;
            }
        }
        
        if (!types_->equals(left_type, right_type) && !is_optional_null_comparison) {
            diag_->reportError("Type mismatch in comparison", node->getLocation());
        }
        node->setType(types_->getBoolType());
        return;
    }
    
    // Logical operators (require bool, return bool)
    if (op == TokenType::AND_AND || op == TokenType::OR_OR) {
        if (!left_type->isBool() || !right_type->isBool()) {
            diag_->reportError("Logical operators require bool operands",
                              node->getLocation());
        }
        node->setType(types_->getBoolType());
        return;
    }
    
    // Arithmetic operators (return operand type)
    if (op == TokenType::PLUS || op == TokenType::MINUS ||
        op == TokenType::STAR || op == TokenType::SLASH ||
        op == TokenType::PERCENT) {
        
        // Check type compatibility
        if (!types_->equals(left_type, right_type)) {
            diag_->reportError("Type mismatch in arithmetic expression",
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // Check if numeric type
        if (!left_type->isNumeric()) {
            diag_->reportError("Arithmetic operators require numeric types",
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        node->setType(left_type);
        return;
    }
    
    // Bitwise operators (integer types)
    if (op == TokenType::AMP || op == TokenType::PIPE ||
        op == TokenType::CARET || op == TokenType::LT_LT ||
        op == TokenType::GT_GT) {
        
        if (!types_->equals(left_type, right_type)) {
            diag_->reportError("Type mismatch in bitwise expression",
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        if (!left_type->isInteger()) {
            diag_->reportError("Bitwise operators require integer types",
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        node->setType(left_type);
        return;
    }
    
    // Default: require equal types, return operand type
    if (types_->equals(left_type, right_type)) {
        node->setType(left_type);
    } else {
        diag_->reportError("Type mismatch in binary expression",
                          node->getLocation());
        node->setType(types_->getVoidType());
    }
}

void TypeChecker::visit(UnaryExpr* node) {
    node->getOperand()->accept(this);
    
    Type* operand_type = node->getOperand()->getType();
    
    if (node->getOperator() == TokenType::BANG) {
        node->setType(types_->getBoolType());
    } else {
        node->setType(operand_type);
    }
}

void TypeChecker::visit(CallExpr* node) {
    // Bug Fix: check if this is an interface method call
    // Method call form: obj.method() is parsed as CallExpr(MemberExpr(obj, "method"), args)
    if (auto* member_expr = dynamic_cast<MemberExpr*>(node->getCallee())) {
        Expr* object = member_expr->getObject();
        const std::string& method_name = member_expr->getMember();
        
        // First check object's type
        object->accept(this);
        Type* obj_type = object->getType();
        
        // Handle reference types: if reference, get pointee type
        if (obj_type && obj_type->isReference()) {
            auto* ref_type = static_cast<ReferenceType*>(obj_type);
            obj_type = ref_type->getPointeeType();
        }
        
        if (obj_type && obj_type->isStruct()) {
            auto* struct_type = static_cast<StructType*>(obj_type);
            
            // Check if it's a field (not a method)
            const auto& fields = struct_type->getFields();
            bool is_field = false;
            for (const auto& field : fields) {
                if (field.first == method_name) {
                    is_field = true;
                    break;
                }
            }
            
            // If not a field, lookup interface method
            if (!is_field) {
                // Try multiple naming conventions
                std::string full_method_name1 = struct_type->getName() + "_" + method_name;   // Old style
                std::string full_method_name2 = struct_type->getName() + "::" + method_name;  // New style
                
                // Lookup method from symbol table
                Symbol* method_symbol = symbols_->lookup(full_method_name1);
                std::string full_method_name = full_method_name1;
                
                if (!method_symbol) {
                    method_symbol = symbols_->lookup(full_method_name2);
                    full_method_name = full_method_name2;
                }
                
                if (method_symbol && method_symbol->getKind() == Symbol::Kind::Function) {
                    // Found interface method! Mark as method call
                    std::cerr << "[TypeChecker] Found method: " << full_method_name << std::endl;
                    node->setIsMethodCall(true);
                    node->setReceiver(object);
                    node->setMethodName(method_name);
                    node->setMethodTarget(full_method_name);
                    
                    // Type check parameters
                    std::vector<Type*> arg_types;
                    for (const auto& arg : node->getArgs()) {
                        arg->accept(this);
                        arg_types.push_back(arg->getType());
                    }
                    
                    // Validate method signature
                    auto* func_symbol = static_cast<FunctionSymbol*>(method_symbol);
                    FunctionType* func_type = func_symbol->getType();
                    const auto& func_params = func_type->getParamTypes();
                    
                    // Self parameter support: method's first parameter is self, user doesn't pass it
                    // So arg_types count should be func_params - 1 (skip self)
                    size_t expected_args = func_params.empty() ? 0 : func_params.size() - 1;
                    if (arg_types.size() != expected_args) {
                        diag_->reportError(
                            "Method call argument count mismatch: expected " +
                            std::to_string(expected_args) + ", got " +
                            std::to_string(arg_types.size()),
                            SourceLocation()
                        );
                    }
                    
                    // Set return type
                    node->setType(func_type->getReturnType());
                    return;
                }
            }
        }
    }
    
    node->getCallee()->accept(this);
    
    // Collect parameter types
    std::vector<Type*> arg_types;
    for (const auto& arg : node->getArgs()) {
        arg->accept(this);
        arg_types.push_back(arg->getType());
    }
    
    // Case 1: Closure call - callee is a variable of FunctionType
    Type* callee_type = node->getCallee()->getType();
    if (callee_type && callee_type->getKind() == Type::Kind::Function) {
        auto* func_type = static_cast<FunctionType*>(callee_type);
        
        // Validate parameter count
        if (arg_types.size() != func_type->getParamTypes().size()) {
            diag_->reportError(
                "Closure call: argument count mismatch. Expected " +
                std::to_string(func_type->getParamTypes().size()) +
                ", got " + std::to_string(arg_types.size()),
                node->getLocation()
            );
            node->setType(types_->getVoidType());
            return;
        }
        
        // Validate parameter types match
        for (size_t i = 0; i < arg_types.size(); ++i) {
            Type* expected = func_type->getParamTypes()[i];
            Type* actual = arg_types[i];
            if (expected != actual) {
                diag_->reportError(
                    "Closure call: argument type mismatch at position " +
                    std::to_string(i) + ". Expected " +
                    expected->toString() + ", got " +
                    actual->toString(),
                    node->getLocation()
                );
            }
        }
        
        // Set return type
        node->setType(func_type->getReturnType());
        return;
    }
    
    // Case 2: Regular function call - callee is IdentifierExpr
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node->getCallee())) {
        const std::string& func_name = ident->getName();
        
        // Generic function call handling
        if (node->hasTypeArgs()) {
            // This is generic function call: identity<i32>(42)
            auto* tmpl = types_->lookupGenericTemplate(func_name);
            if (tmpl && tmpl->kind == GenericTemplate::FUNCTION) {
                // Check where constraints
                FunctionDecl* func_decl = tmpl->func_def;
                if (func_decl) {
                    const auto& where_clauses = func_decl->getWhereClauses();
                    
                    if (!where_clauses.empty()) {
                        std::cerr << "[WhereClause] Checking constraints for generic call: " 
                                  << func_name << std::endl;
                        
                        // Build type substitution map
                        std::map<std::string, Type*> type_sub;
                        const auto& type_args = node->getTypeArgs();
                        for (size_t i = 0; i < tmpl->type_params.size() && i < type_args.size(); ++i) {
                            type_sub[tmpl->type_params[i]] = type_args[i];
                        }
                        
                        // Validate where constraints
                        if (!checkWhereConstraintsSatisfied(where_clauses, type_sub)) {
                            node->setType(types_->getVoidType());
                            return;
                        }
                    }
                }
                
                // Instantiate function
                Type* return_type = instantiateFunctionReturnType(tmpl, node->getTypeArgs());
                if (return_type) {
                    node->setType(return_type);
                    return;
                }
            }
        }
        
        // Special handling: ok(value) - Result constructor
        if (func_name == "ok" && arg_types.size() == 1) {
            // ok(value) returns Result<T>
            Type* value_type = arg_types[0];
            node->setType(types_->getResultType(value_type));
            return;
        }
        
        // Special handling: err(message) - Result constructor
        if (func_name == "err" && arg_types.size() == 1) {
            // err(message) returns Result<T>, T inferred from current function return type
            if (current_function_return_type_ &&
                current_function_return_type_->getKind() == Type::Kind::Result) {
                // Use function's return type
                node->setType(current_function_return_type_);
                return;
            }
            
            // If cannot infer, default to Result<void>
            node->setType(types_->getResultType(types_->getVoidType()));
            return;
        }
        
        // Special handling: some(value) - Optional constructor
        if (func_name == "some" && arg_types.size() == 1) {
            // some(value) returns Optional<T>, T is value's type
            Type* value_type = arg_types[0];
            node->setType(types_->getOptionalType(value_type));
            return;
        }
        
        // Special handling: len(array/slice) - support generic containers
        if (func_name == "len" && arg_types.size() == 1) {
            Type* arg_type = arg_types[0];
            if (arg_type && (arg_type->isString() || arg_type->isArray() || 
                             arg_type->getKind() == Type::Kind::Slice)) {
                // len returns u64
                node->setType(types_->getU64Type());
                return;
            }
        }
        
        // Special handling: println/print/to_string - support all basic types
        if ((func_name == "println" || func_name == "print" || func_name == "to_string") 
            && arg_types.size() == 1) {
            // Validate parameter type is basic type or known type
            Type* arg_type = arg_types[0];
            if (arg_type) {
                // println/print return void
                if (func_name == "println" || func_name == "print") {
                    node->setType(types_->getVoidType());
                    return;
                }
                // to_string returns string
                if (func_name == "to_string") {
                    node->setType(types_->getStringType());
                    return;
                }
            }
        }
        
        FunctionSymbol* func = symbols_->lookupFunction(func_name, arg_types);
        if (func) {
            node->setType(func->getReturnType());
            return;
        }
    }
    
    diag_->reportError("Cannot resolve function call", node->getLocation());
    node->setType(types_->getVoidType());
}

void TypeChecker::visit(StaticAccessExpr* node) {
    // Static access: Type::Variant or Type::method
    // Support generic template static access
    
    // First try to lookup concrete type
    Type* type = types_->lookupType(node->getTypeName());
    
    // If not found, check if it's a generic template
    if (!type) {
        auto* tmpl = types_->lookupGenericTemplate(node->getTypeName());
        if (tmpl && expected_type_ && expected_type_->isEnum()) {
            // Get instantiated type from expected_type_
            type = expected_type_;
        }
    }
    
    if (!type) {
        diag_->reportError("Unknown type: " + node->getTypeName(), node->getLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    // Handle enum constructor Option::Some
    if (type->isEnum()) {
        EnumType* enum_type = static_cast<EnumType*>(type);
        
        // Lookup variant
        bool found = false;
        for (const auto& variant : enum_type->getVariants()) {
            if (variant.first == node->getMember()) {
                // Found variant! Create a special function type representing the constructor
                if (variant.second) {
                    // Variant with associated data
                    std::vector<Type*> param_types;
                    
                    // Check if it's a tuple type (multi-parameter variant)
                    if (variant.second->isTuple()) {
                        // Multi-parameter: expand tuple type into multiple parameters
                        // Move((i32, i32)) -> Move(i32, i32)
                        TupleType* tuple_type = static_cast<TupleType*>(variant.second);
                        param_types = tuple_type->getElementTypes();
                    } else {
                        // Single parameter
                        param_types = {variant.second};
                    }
                    
                    Type* constructor_type = types_->getFunctionType(param_types, enum_type);
                    node->setType(constructor_type);
                } else {
                    // Variant without associated data, directly enum type
                    node->setType(enum_type);
                }
                found = true;
                break;
            }
        }
        
        if (!found) {
            diag_->reportError("Unknown enum variant: " + node->getMember(), 
                             node->getLocation());
            node->setType(types_->getVoidType());
        }
        return;
    }
    
    // TODO: Handle other static accesses (interface methods, etc.)
    node->setType(types_->getVoidType());
}

void TypeChecker::visit(MemberExpr* node) {
    node->getObject()->accept(this);
    
    Type* obj_type = node->getObject()->getType();
    if (!obj_type) {
        node->setType(types_->getVoidType());
        return;
    }
    
    // Reference type handling: if object is reference type, get its pointee type
    if (obj_type->isReference()) {
        auto* ref_type = static_cast<ReferenceType*>(obj_type);
        obj_type = ref_type->getPointeeType();
        // Self type resolution: if pointee is Self, resolve to actual type
        obj_type = resolveSelfType(obj_type);
    }
    
    const std::string& member = node->getMember();
    
    // Check if it's tuple field access (member name is digit)
    bool is_tuple_access = !member.empty() && std::isdigit(member[0]);
    
    if (is_tuple_access && obj_type->isTuple()) {
        // Tuple field access: tuple.0, tuple.1, etc.
        auto* tuple_type = static_cast<TupleType*>(obj_type);
        
        // Parse field index
        int field_idx = std::stoi(member);
        
        // Validate index validity
        const auto& element_types = tuple_type->getElementTypes();
        if (field_idx < 0 || field_idx >= static_cast<int>(element_types.size())) {
            diag_->reportError("Tuple index out of range: " + member,
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // Set field type
        node->setType(element_types[field_idx]);
        
    } else if (obj_type->isStruct()) {
        // Struct member access: struct.field_name
        auto* struct_type = static_cast<StructType*>(obj_type);
        Type* field_type = struct_type->getFieldType(member);
        if (field_type) {
            node->setType(field_type);
        } else {
            // Bug Fix: if not a field, might be an interface method
            // Try multiple method naming conventions
            std::string method_name1 = struct_type->getName() + "_" + member;  // Old style
            std::string method_name2 = struct_type->getName() + "::" + member; // New style (support method)
            
            Symbol* method_symbol = symbols_->lookup(method_name1);
            if (!method_symbol) {
                method_symbol = symbols_->lookup(method_name2);
            }
            
            if (method_symbol && method_symbol->getKind() == Symbol::Kind::Function) {
                // This is a method! Set to method's return type
                // Note: when MemberExpr appears alone, it's actually wrapped in CallExpr
                // Setting type here is mainly to prevent errors
                auto* func_symbol = static_cast<FunctionSymbol*>(method_symbol);
                node->setType(func_symbol->getType()->getReturnType());
            } else {
                // Neither field nor method, report error
                diag_->reportError("Struct has no member: " + member,
                                  node->getLocation());
                node->setType(types_->getVoidType());
            }
        }
    } else {
        diag_->reportError("Member access on non-struct/tuple type",
                          node->getLocation());
        node->setType(types_->getVoidType());
    }
}

void TypeChecker::visit(IndexExpr* node) {
    node->getObject()->accept(this);
    node->getIndex()->accept(this);
    
    Type* obj_type = node->getObject()->getType();
    Type* index_type = node->getIndex()->getType();
    
    // Check index type must be integer
    if (!index_type || !index_type->isInteger()) {
        diag_->reportError("Array index must be integer type",
                          node->getLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    // Check indexed object type
    if (obj_type && obj_type->isArray()) {
        auto* array_type = static_cast<ArrayType*>(obj_type);
        node->setType(array_type->getElementType());
    } else if (obj_type && obj_type->isString()) {
        // string[i] returns char
        node->setType(types_->getCharType());
    } else {
        diag_->reportError("Cannot index non-array type",
                          node->getLocation());
        node->setType(types_->getVoidType());
    }
}

void TypeChecker::visit(IfExpr* node) {
    node->getCondition()->accept(this);
    node->getThenExpr()->accept(this);
    if (node->getElseExpr()) {
        node->getElseExpr()->accept(this);
    }
    node->setType(node->getThenExpr()->getType());
}

void TypeChecker::visit(BlockExpr* node) {
    // BlockExpr: { stmt1; stmt2; expr }
    // Return type inference rules:
    // 1. If has return statement, use return's type
    // 2. If last is expression statement (no semicolon), use that expression's type
    // 3. Otherwise return void
    
    symbols_->enterScope();
    
    Type* block_type = types_->getVoidType();
    bool has_return = false;
    
    const auto& stmts = node->getStmts();
    for (size_t i = 0; i < stmts.size(); ++i) {
        stmts[i]->accept(this);
        
        // Check if there's a return statement
        if (auto* return_stmt = dynamic_cast<ReturnStmt*>(stmts[i].get())) {
            if (return_stmt->getValue()) {
                block_type = return_stmt->getValue()->getType();
                has_return = true;
            }
        }
        
        // If last statement is expression statement (and no return), its type becomes block's type
        if (!has_return && i == stmts.size() - 1) {
            if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmts[i].get())) {
                if (expr_stmt->getExpr()) {
                    block_type = expr_stmt->getExpr()->getType();
                }
            }
        }
    }
    
    symbols_->exitScope();
    node->setType(block_type);
}

void TypeChecker::visit(ArrayLiteral* node) {
    // Array literal type inference: all elements must be same type
    if (node->getElements().empty()) {
        // Empty array, cannot infer type, need context info
        // Temporarily set to unknown or i32 array as default
        node->setType(types_->getArrayType(types_->getI32Type(), 0));
        return;
    }
    
    // Check first element's type
    auto& elements = node->getElements();
    elements[0]->accept(this);
    Type* element_type = elements[0]->getType();
    
    if (!element_type) {
        diag_->reportError("Cannot infer array element type", SourceLocation());
        node->setType(nullptr);
        return;
    }
    
    // Validate all element types are consistent
    for (size_t i = 1; i < elements.size(); ++i) {
        elements[i]->accept(this);
        Type* elem_type = elements[i]->getType();
        
        if (!elem_type || !types_->equals(element_type, elem_type)) {
            diag_->reportError("Array elements must have the same type", SourceLocation());
            node->setType(nullptr);
            return;
        }
    }
    
    // Set array type [T; N]
    node->setType(types_->getArrayType(element_type, elements.size()));
}

void TypeChecker::visit(TupleExpr* node) {
    // Tuple type inference: infer each element's type
    std::vector<Type*> element_types;
    
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
        Type* elem_type = elem->getType();
        
        if (!elem_type) {
            diag_->reportError("Cannot infer tuple element type", SourceLocation());
            node->setType(nullptr);
            return;
        }
        
        element_types.push_back(elem_type);
    }
    
    // Set tuple type (T1, T2, ...)
    node->setType(types_->getTupleType(element_types));
}

void TypeChecker::visit(RangeExpr* node) {
    // Range expression type check
    Type* start_type = nullptr;
    Type* end_type = nullptr;
    
    if (node->getStart()) {
        node->getStart()->accept(this);
        start_type = node->getStart()->getType();
    }
    
    if (node->getEnd()) {
        node->getEnd()->accept(this);
        end_type = node->getEnd()->getType();
    }
    
    // Validate start and end types are consistent (if both exist)
    if (start_type && end_type) {
        if (!types_->equals(start_type, end_type)) {
            diag_->reportError("Range start and end must have the same type", SourceLocation());
            node->setType(nullptr);
            return;
        }
    }
    
    // Range's element type is start or end's type
    Type* range_element_type = start_type ? start_type : end_type;
    if (!range_element_type) {
        range_element_type = types_->getI32Type(); // Default i32
    }
    
    // TODO: Set Range type (current Type system may not have Range type)
    // Temporarily use Slice type to represent
    node->setType(types_->getSliceType(range_element_type));
}

void TypeChecker::visit(StructLiteral* node) {
    // Struct literal type check: Point { x: 10, y: 20 } or Box { value: 42 }
    
    // Generic struct support: lookup struct type definition (including instantiated types)
    Type* struct_type = types_->lookupType(node->getStructName());
    
    // If not found, check if it's a generic template, infer from expected_type_
    if (!struct_type) {
        auto* tmpl = types_->lookupGenericTemplate(node->getStructName());
        if (tmpl && expected_type_ && expected_type_->isStruct()) {
            // Get instantiated type from expected_type_
            struct_type = expected_type_;
        }
    }
    
    if (!struct_type) {
        diag_->reportError("Unknown struct type: " + node->getStructName(), 
                          SourceLocation());
        node->setType(nullptr);
        return;
    }
    
    // Ensure it's StructType
    if (struct_type->getKind() != Type::Kind::Struct) {
        diag_->reportError(node->getStructName() + " is not a struct type", 
                          SourceLocation());
        node->setType(nullptr);
        return;
    }
    
    StructType* stype = static_cast<StructType*>(struct_type);
    const auto& struct_fields = stype->getFields();
    
    // Check each initialized field
    std::set<std::string> initialized_fields;
    
    for (auto& field_init : node->getFields()) {
        // Check if field exists
        bool found = false;
        Type* expected_type = nullptr;
        
        for (const auto& [field_name, field_type] : struct_fields) {
            if (field_name == field_init.name) {
                found = true;
                expected_type = field_type;
                break;
            }
        }
        
        if (!found) {
            diag_->reportError("Struct " + node->getStructName() + 
                              " has no field named " + field_init.name,
                              SourceLocation());
            continue;
        }
        
        // Check field initialization value's type
        field_init.value->accept(this);
        Type* value_type = field_init.value->getType();
        
        if (!value_type || !types_->equals(expected_type, value_type)) {
            diag_->reportError("Type mismatch for field " + field_init.name,
                              SourceLocation());
        }
        
        initialized_fields.insert(field_init.name);
    }
    
    // Check if all fields are initialized
    for (const auto& [field_name, field_type] : struct_fields) {
        if (initialized_fields.find(field_name) == initialized_fields.end()) {
            diag_->reportError("Missing initialization for field " + field_name,
                              SourceLocation());
        }
    }
    
    // Set struct type
    node->setType(struct_type);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Statement type checking
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::visit(ExprStmt* node) {
    node->getExpr()->accept(this);
}

void TypeChecker::visit(VarDecl* node) {
    if (node->getInit()) {
        // Key fix: if has explicit type declaration, set expected_type
        Type* saved_expected = expected_type_;
        if (node->getType()) {
            expected_type_ = node->getType();
        }
        
        // Visit initialization expression (now will use expected_type)
        node->getInit()->accept(this);
        
        // Restore expected_type
        expected_type_ = saved_expected;
        
        Type* init_type = node->getInit()->getType();
        Type* decl_type = nullptr;
        
        // Type inference: if no explicit type, try to infer from initialization expression
        if (node->getType()) {
            // Has explicit type, use it
            decl_type = node->getType();
        } else if (init_type) {
            // No explicit type, use inferred type
            decl_type = init_type;
            std::cerr << "[TypeInference] Inferred type for variable '" 
                      << node->getName() << "': " << decl_type->toString() << std::endl;
        } else {
            // Cannot infer, report error
            diag_->reportError(
                "Cannot infer type for variable '" + node->getName() + 
                "' - please provide explicit type annotation",
                node->getLocation()
            );
            return;
        }
        
        // Set VarDecl's type (CodeGen needs to use it)
        node->setType(decl_type);
        
        symbols_->defineVariable(node->getName(), decl_type, node->isMutable());
    } else if (node->getType()) {
        symbols_->defineVariable(node->getName(), node->getType(), node->isMutable());
    } else {
        diag_->reportError("Variable must have type or initializer",
                          node->getLocation());
    }
}

void TypeChecker::visit(DestructuringDecl* node) {
    // Tuple destructuring: let (a, b) = tuple;
    if (!node->getInit()) {
        diag_->reportError("Destructuring declaration must have initializer",
                          SourceLocation());
        return;
    }
    
    // Check initialization expression's type
    node->getInit()->accept(this);
    Type* init_type = node->getInit()->getType();
    
    if (!init_type || !init_type->isTuple()) {
        diag_->reportError("Destructuring requires tuple type",
                          SourceLocation());
        return;
    }
    
    auto* tuple_type = static_cast<TupleType*>(init_type);
    const auto& element_types = tuple_type->getElementTypes();
    
    // Validate variable count matches tuple element count
    if (node->getNames().size() != element_types.size()) {
        diag_->reportError("Destructuring pattern size mismatch",
                          SourceLocation());
        return;
    }
    
    // Define symbol for each variable
    for (size_t i = 0; i < node->getNames().size(); ++i) {
        symbols_->defineVariable(node->getNames()[i], element_types[i], node->isMutable());
    }
}

void TypeChecker::visit(StructDestructuringDecl* node) {
    // Struct destructuring: let Point { x, y } = p;
    if (!node->getInit()) {
        diag_->reportError("Struct destructuring declaration must have initializer",
                          SourceLocation());
        return;
    }
    
    // Check initialization expression's type
    node->getInit()->accept(this);
    Type* init_type = node->getInit()->getType();
    
    if (!init_type || !init_type->isStruct()) {
        diag_->reportError("Struct destructuring requires struct type",
                          SourceLocation());
        return;
    }
    
    auto* struct_type = static_cast<StructType*>(init_type);
    
    // Validate struct name matches
    if (struct_type->getName() != node->getStructName()) {
        diag_->reportError("Struct name mismatch in destructuring: expected " + 
                          node->getStructName() + ", got " + struct_type->getName(),
                          SourceLocation());
        return;
    }
    
    // Define variable for each field
    for (const auto& field_name : node->getFieldNames()) {
        Type* field_type = struct_type->getFieldType(field_name);
        if (!field_type) {
            diag_->reportError("Unknown field " + field_name + " in struct " + 
                              struct_type->getName(),
                              SourceLocation());
            continue;
        }
        symbols_->defineVariable(field_name, field_type, node->isMutable());
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Statement type checking
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::visit(FunctionDecl* node) {
    // Create function type
    std::vector<Type*> param_types;
    for (const auto& param : node->getParams()) {
        // Fix: Resolve Self type to actual type
        param_types.push_back(resolveSelfType(param.type));
    }
    
    // Fix: Resolve Self in return type
    Type* resolved_return_type = resolveSelfType(node->getReturnType());
    FunctionType* func_type = types_->getFunctionType(param_types, resolved_return_type);
    
    // Self type support: save resolved types to AST node for CodeGen use
    node->setResolvedTypes(param_types, resolved_return_type);
    
    // Bug Fix: if in interface implementation context (current_self_type_ non-null),
    // use decorated method name TypeName_methodName
    std::string func_name = node->getName();
    if (current_self_type_) {
        func_name = current_self_type_->toString() + "_" + node->getName();
    }
    
    symbols_->defineFunction(func_name, func_type);
    
    // Validate where constraints
    validateWhereConstraints(node->getWhereClauses(), node->getGenericParams());
    
    // Check function body
    symbols_->enterScope();
    // Fix: Use resolved return type
    current_function_return_type_ = resolved_return_type;
    
    // Add parameters to scope
    for (size_t i = 0; i < node->getParams().size(); ++i) {
        const auto& param = node->getParams()[i];
        // Fix: Use resolved parameter type
        symbols_->defineVariable(param.name, param_types[i], param.is_mutable);
        
        // Self parameter type recording: if parameter name is self, record its actual type (may be reference)
        if (param.name == "self") {
            current_self_param_type_ = param_types[i];
        }
    }
    
    if (node->getBody()) {
        node->getBody()->accept(this);
    }
    
    current_function_return_type_ = nullptr;
    symbols_->exitScope();
}

void TypeChecker::visit(ReturnStmt* node) {
    if (node->getValue()) {
        node->getValue()->accept(this);
        
        Type* return_type = node->getValue()->getType();
        if (current_function_return_type_) {
            // === Automatic wrapping for Optional and Result ===
            // 1. If function returns Optional<T>, return value is T, allow (auto-wrap)
            if (current_function_return_type_->getKind() == Type::Kind::Optional) {
                auto* optional_type = static_cast<OptionalType*>(current_function_return_type_);
                Type* inner_type = optional_type->getInnerType();
                
                // If returning T (non-Optional), allow auto-wrap
                if (return_type == inner_type) {
                    return;  // OK, CodeGen will auto-wrap
                }
                // If returning Optional<T>, also allow
                if (return_type == current_function_return_type_) {
                    return;  // OK, direct return
                }
                // Special: if returning null (Optional<void>), also allow
                if (return_type->getKind() == Type::Kind::Optional) {
                    auto* ret_optional = static_cast<OptionalType*>(return_type);
                    if (ret_optional->getInnerType() == types_->getVoidType()) {
                        return;  // OK, null can be used for any Optional<T>
                    }
                }
            }
            
            // 2. If function returns Result<T>, return value is T, not allowed (must use explicit ok/err)
            // Result requires explicit construction, no auto-wrapping
            
            // 3. Special handling: Result type needs deep comparison
            if (!return_type || !types_->equals(return_type, current_function_return_type_)) {
                // If both are Result types, check if ok_type matches
                if (return_type && 
                    return_type->getKind() == Type::Kind::Result &&
                    current_function_return_type_->getKind() == Type::Kind::Result) {
                    auto* ret_results = static_cast<ResultType*>(return_type);
                    auto* func_results = static_cast<ResultType*>(current_function_return_type_);
                    if (types_->equals(ret_results->getOkType(), func_results->getOkType())) {
                        return;  // Result<T> type matches
                    }
                }
                
                diag_->reportError("Return type mismatch",
                                  node->getLocation());
            }
        }
    }
}

void TypeChecker::visit(IfStmt* node) {
    node->getCondition()->accept(this);
    node->getThenStmt()->accept(this);
    if (node->getElseStmt()) {
        node->getElseStmt()->accept(this);
    }
}

void TypeChecker::visit(LoopStmt* node) {
    bool prev_in_loop = in_loop_;
    in_loop_ = true;
    node->getBody()->accept(this);
    in_loop_ = prev_in_loop;
}

void TypeChecker::visit(WhileStmt* node) {
    node->getCondition()->accept(this);
    bool prev_in_loop = in_loop_;
    in_loop_ = true;
    node->getBody()->accept(this);
    in_loop_ = prev_in_loop;
}

void TypeChecker::visit(BreakStmt* node) {
    if (!in_loop_) {
        diag_->reportError("break outside of loop",
                          node->getLocation());
    }
}

void TypeChecker::visit(ContinueStmt* node) {
    if (!in_loop_) {
        diag_->reportError("continue outside of loop",
                          node->getLocation());
    }
}

void TypeChecker::visit(BlockStmt* node) {
    symbols_->enterScope();
    
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
    
    symbols_->exitScope();
}

void TypeChecker::visit(ForStmt* node) {
    // for loop type check: for i in 0..10 { ... }
    
    // Check iterator expression
    node->getIterator()->accept(this);
    Type* iter_type = node->getIterator()->getType();
    
    if (!iter_type) {
        diag_->reportError("Cannot determine iterator type", SourceLocation());
        return;
    }
    
    // Determine loop variable type
    Type* var_type = nullptr;
    
    // If it's Range type, loop variable type is Range's element type
    if (iter_type->getKind() == Type::Kind::Slice) {
        // Range temporarily represented as Slice, element type is Slice's element type
        SliceType* slice = static_cast<SliceType*>(iter_type);
        var_type = slice->getElementType();
    } else if (iter_type->getKind() == Type::Kind::Array) {
        // Array iteration, element type is array's element type
        ArrayType* arr = static_cast<ArrayType*>(iter_type);
        var_type = arr->getElementType();
    } else {
        // Default i32 (for Range)
        var_type = types_->getI32Type();
    }
    
    // Add loop variable in new scope
    symbols_->enterScope();
    symbols_->defineVariable(node->getVarName(), var_type, false);
    
    // Check loop body
    node->getBody()->accept(this);
    
    symbols_->exitScope();
}

void TypeChecker::visit(StructDecl* node) {
    // Struct definition: already registered in Parser phase, can do additional semantic checks here
    
    // Type already registered in Parser phase, avoid duplicate registration
    // Can do here:
    // - Deep validation of field types
    // - Generic constraint checking
    // - Loop reference checking
    
    // Current: skip (Parser already processed)
}

void TypeChecker::visit(EnumDecl* node) {
    // Enum definition: already registered in Parser phase, can do additional semantic checks here
    
    // Type already registered in Parser phase, avoid duplicate registration
    // Can do here:
    // - Deep validation of variant types
    // - Generic constraint checking
    
    // Current: skip (Parser already processed)
}

void TypeChecker::visit(MatchExpr* node) {
    // Match expression type check - complete implementation
    
    // 1. Check matched expression
    node->getScrutinee()->accept(this);
    Type* scrutinee_type = node->getScrutinee()->getType();
    
    if (!scrutinee_type) {
        diag_->reportError("Match scrutinee has no type", SourceLocation());
        return;
    }
    
    // 2. Check each arm
    Type* results_type = nullptr;
    bool has_wildcard = false;
    
    const auto& arms = node->getArms();
    
    if (arms.empty()) {
        diag_->reportError("Match expression must have at least one arm", SourceLocation());
        return;
    }
    
    for (size_t i = 0; i < arms.size(); ++i) {
        const auto& arm = arms[i];
        
        // Enter new scope (arm's variable bindings only valid within arm)
        symbols_->enterScope();
        
        // Set expected_type_, let pattern know what type to match
        Type* saved_expected = expected_type_;
        expected_type_ = scrutinee_type;
        
        // Check pattern (will automatically bind variables)
        arm.pattern->accept(this);
        
        // Restore expected_type_
        expected_type_ = saved_expected;
        
        // Type check guard condition (if present)
        //
        // Guard conditions are optional boolean expressions that provide additional
        // filtering after pattern matching succeeds. They can access variables bound
        // by the pattern.
        //
        // Requirements:
        //   - Guard must be a well-typed expression
        //   - Guard type must be bool (strict requirement)
        //   - Guard is evaluated after pattern matching and variable binding
        //
        // Execution order:
        //   1. Pattern matches -> bind variables
        //   2. Evaluate guard with bound variables in scope
        //   3. If guard is true, execute arm body
        //   4. If guard is false, try next arm (pattern match "fails")
        if (arm.guard) {
            arm.guard->accept(this);
            Type* guard_type = arm.guard->getType();
            
            if (!guard_type) {
                diag_->reportError("Guard condition has no type", SourceLocation());
            } else if (guard_type->getKind() != Type::Kind::Bool) {
                diag_->reportError(
                    "Guard condition must be a boolean expression, got: " + 
                    guard_type->toString(),
                    SourceLocation()
                );
            }
        }
        
        // Check if there's a wildcard pattern
        if (dynamic_cast<WildcardPattern*>(arm.pattern.get()) ||
            dynamic_cast<VariablePattern*>(arm.pattern.get())) {
            has_wildcard = true;
        }
        
        // Check arm expression (variables already in scope)
        arm.expression->accept(this);
        Type* arm_type = arm.expression->getType();
        
        if (!arm_type) {
            diag_->reportError("Match arm expression has no type", SourceLocation());
            symbols_->exitScope();
            continue;
        }
        
        // All arms must return same type
        if (!results_type) {
            results_type = arm_type;
        } else if (!types_->equals(results_type, arm_type)) {
            diag_->reportError(
                "Match arm types must be compatible: expected " + 
                results_type->toString() + ", got " + arm_type->toString(),
                SourceLocation()
            );
        }
        
        // Exit arm scope
        symbols_->exitScope();
    }
    
    // 3. Exhaustiveness check
    if (!has_wildcard && !isExhaustive(arms, scrutinee_type)) {
        diag_->reportWarning(
            "Match expression is not exhaustive, consider adding a wildcard '_' pattern",
            SourceLocation()
        );
    }
    
    // 4. Set match expression's type
    if (results_type) {
        node->setType(results_type);
    }
}

void TypeChecker::visit(ClosureExpr* node) {
    // Closure type check: complete implementation + type inference
    // 1. Infer parameter types from expected_type_ (if needed)
    // 2. Check parameter types
    // 3. Infer return type (if not specified)
    // 4. Create FunctionType
    
    // Type inference: infer parameter types from expected_type_
    if (expected_type_ && expected_type_->isFunction()) {
        auto* expected_fn_type = static_cast<FunctionType*>(expected_type_);
        const auto& expected_params = expected_fn_type->getParamTypes();
        auto& closure_params = const_cast<std::vector<ClosureExpr::Param>&>(node->getParams());
        
        // Infer types for parameters without types
        for (size_t i = 0; i < closure_params.size() && i < expected_params.size(); i++) {
            if (!closure_params[i].type) {
                closure_params[i].type = expected_params[i];  // Infer!
            }
        }
        
        // Infer return type (if not specified)
        if (!node->getReturnType()) {
            node->setReturnType(expected_fn_type->getReturnType());
        }
    }
    
    // Enter closure scope
    symbols_->enterScope();
    
    // Add parameters to scope
    for (const auto& param : node->getParams()) {
        if (!param.type) {
            diag_->reportError(
                "Cannot infer closure parameter type. Please add type annotation or use in typed context",
                SourceLocation()
            );
            continue;
        }
        symbols_->defineVariable(param.name, param.type, param.is_mutable);
    }
    
    // === Key fix: set current_function_return_type_ ===
    // Save old return type
    Type* saved_return_type = current_function_return_type_;
    
    // If closure has explicit return type, set as current function return type
    // So ReturnStmt can correctly validate type
    if (node->getReturnType()) {
        current_function_return_type_ = node->getReturnType();
    }
    
    // Check closure body
    node->getBody()->accept(this);
    Type* body_type = node->getBody()->getType();
    
    // Restore return type
    current_function_return_type_ = saved_return_type;
    
    // If body_type is nullptr, default to void
    if (!body_type) {
        body_type = types_->getVoidType();
    }
    
    // Infer return type (if not specified)
    if (!node->getReturnType()) {
        node->setReturnType(body_type);
    }
    
    // Validate return type matches
    Type* declared_return = node->getReturnType();
    if (declared_return && body_type) {
        // Use type system equality check instead of pointer comparison
        bool types_match = (declared_return == body_type) ||
                          (declared_return->toString() == body_type->toString());
        
        if (!types_match) {
            diag_->reportError(
                "Closure body type mismatch. Expected " + 
                declared_return->toString() + ", got " + 
                body_type->toString(), 
                SourceLocation()
            );
        }
    }
    
    // === Captured variable analysis ===
    // Note: Must be done before exiting closure scope, so we can lookup external variable types from symbol table
    CaptureAnalyzer analyzer(symbols_);
    auto captured = analyzer.analyze(node->getBody(), node->getParams());
    
    // Add captured variables to ClosureExpr
    for (const auto& var : captured) {
        node->addCapturedVar(var);
    }
    
    // Exit closure scope
    symbols_->exitScope();
    
    // Create FunctionType
    std::vector<Type*> param_types;
    for (const auto& param : node->getParams()) {
        param_types.push_back(param.type);
    }
    
    Type* return_type = node->getReturnType() ? 
        node->getReturnType() : 
        types_->getVoidType();
    
    FunctionType* fn_type = types_->getFunctionType(param_types, return_type);
    
    // Set capture flag (for CodeGen to determine closure call method)
    fn_type->setHasCaptures(!captured.empty());
    
    node->setType(fn_type);
}

void TypeChecker::visit(TryExpr* node) {
    node->getExpr()->accept(this);
    Type* expr_type = node->getExpr()->getType();
    
    if (!expr_type) {
        diag_->reportError("Try expression has no type", SourceLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    // === Process according to operator ===
    if (node->isResultTry()) {
        // expr! - Result<T> unwrap
        if (expr_type->getKind() != Type::Kind::Result) {
            diag_->reportError("! operator can only be used on Result types", SourceLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // ! operator returns T of Result<T>
        auto* results_type = static_cast<ResultType*>(expr_type);
        node->setType(results_type->getOkType());
    }
    else if (node->isOptionalTry()) {
        // expr? - Optional<T> unwrap
        if (expr_type->getKind() != Type::Kind::Optional) {
            diag_->reportError("? operator can only be used on Optional types", SourceLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // ? operator returns T of Optional<T>
        auto* optional_type = static_cast<OptionalType*>(expr_type);
        node->setType(optional_type->getInnerType());
    }
    else {
        diag_->reportError("Unknown try operator", SourceLocation());
        node->setType(types_->getVoidType());
    }
}

// Helper method: check pattern type compatibility
void TypeChecker::checkPatternType(Pattern* pattern, Type* expected_type) {
    if (auto* lit = dynamic_cast<LiteralPattern*>(pattern)) {
        // Check if literal type matches
        Type* pattern_type = nullptr;
        
        switch (lit->getKind()) {
            case LiteralPattern::Kind::Int:
                // Check if integer type
                if (expected_type->getKind() >= Type::Kind::I8 && 
                    expected_type->getKind() <= Type::Kind::U128) {
                    pattern_type = expected_type;
                }
                break;
            case LiteralPattern::Kind::Bool:
                if (expected_type->getKind() == Type::Kind::Bool) {
                    pattern_type = expected_type;
                }
                break;
            case LiteralPattern::Kind::String:
                if (expected_type->getKind() == Type::Kind::String) {
                    pattern_type = expected_type;
                }
                break;
            case LiteralPattern::Kind::Char:
                if (expected_type->getKind() == Type::Kind::Char) {
                    pattern_type = expected_type;
                }
                break;
            case LiteralPattern::Kind::Float:
                // Check if floating-point type
                if (expected_type->getKind() >= Type::Kind::F8 && 
                    expected_type->getKind() <= Type::Kind::F128) {
                    pattern_type = expected_type;
                }
                break;
            default:
                break;
        }
        
        if (!pattern_type) {
            diag_->reportError(
                "Pattern type does not match scrutinee type: expected " +
                expected_type->toString(),
                SourceLocation()
            );
        }
    }
    
    // WildcardPattern and VariablePattern match any type
    if (dynamic_cast<WildcardPattern*>(pattern) || 
        dynamic_cast<VariablePattern*>(pattern)) {
        return;
    }
    
    // TuplePattern: check tuple element types
    if (auto* tuple = dynamic_cast<TuplePattern*>(pattern)) {
        if (expected_type->getKind() != Type::Kind::Tuple) {
            diag_->reportError(
                "Cannot match tuple pattern against non-tuple type",
                SourceLocation()
            );
            return;
        }
        
        auto* tuple_type = static_cast<TupleType*>(expected_type);
        const auto& elements = tuple->getElements();
        const auto& element_types = tuple_type->getElementTypes();
        
        if (elements.size() != element_types.size()) {
            diag_->reportError(
                "Tuple pattern has wrong number of elements",
                SourceLocation()
            );
            return;
        }
        
        // Recursively check each element
        for (size_t i = 0; i < elements.size(); ++i) {
            checkPatternType(elements[i].get(), element_types[i]);
        }
    }
    
    // EnumPattern: check enum variant
    if (auto* enum_pat = dynamic_cast<EnumPattern*>(pattern)) {
        if (expected_type->getKind() != Type::Kind::Enum) {
            diag_->reportError(
                "Cannot match enum pattern against non-enum type",
                SourceLocation()
            );
            return;
        }
        
        auto* enum_type = static_cast<EnumType*>(expected_type);
        const auto& variants = enum_type->getVariants();
        
        // Check if variant exists
        bool found = false;
        Type* variant_data_type = nullptr;
        
        for (const auto& [variant_name, data_type] : variants) {
            if (variant_name == enum_pat->getVariantName()) {
                found = true;
                variant_data_type = data_type;
                break;
            }
        }
        
        if (!found) {
            diag_->reportError(
                "Enum variant '" + enum_pat->getVariantName() + 
                "' does not exist in enum '" + enum_type->getName() + "'",
                SourceLocation()
            );
            return;
        }
        
        // If has inner pattern, check data type
        if (enum_pat->getInner()) {
            if (!variant_data_type) {
                diag_->reportError(
                    "Enum variant '" + enum_pat->getVariantName() + 
                    "' does not have associated data",
                    SourceLocation()
                );
                return;
            }
            
            // Recursively check inner pattern
            checkPatternType(enum_pat->getInner(), variant_data_type);
        } else if (variant_data_type) {
            diag_->reportWarning(
                "Enum variant '" + enum_pat->getVariantName() + 
                "' has data but pattern does not extract it",
                SourceLocation()
            );
        }
    }
}

// Helper method: check exhaustiveness
bool TypeChecker::isExhaustive(const std::vector<MatchArm>& arms, Type* scrutinee_type) {
    // If has wildcard or variable binding pattern, it's exhaustive
    for (const auto& arm : arms) {
        if (dynamic_cast<WildcardPattern*>(arm.pattern.get()) ||
            dynamic_cast<VariablePattern*>(arm.pattern.get())) {
            return true;
        }
    }
    
    // For integer types, hard to check exhaustiveness (range too large)
    if (scrutinee_type->getKind() >= Type::Kind::I8 &&
        scrutinee_type->getKind() <= Type::Kind::U128) {
        return false;  // Conservative: always require wildcard
    }
    
    // For boolean types, check if covers true and false
    if (scrutinee_type->getKind() == Type::Kind::Bool) {
        bool has_true = false;
        bool has_false = false;
        
        for (const auto& arm : arms) {
            if (auto* lit = dynamic_cast<LiteralPattern*>(arm.pattern.get())) {
                if (lit->getKind() == LiteralPattern::Kind::Bool) {
                    if (lit->getValue() == "true") has_true = true;
                    if (lit->getValue() == "false") has_false = true;
                }
            }
        }
        
        return has_true && has_false;
    }
    
    // For enum types, check if covers all variants
    if (scrutinee_type->getKind() == Type::Kind::Enum) {
        auto* enum_type = static_cast<EnumType*>(scrutinee_type);
        const auto& variants = enum_type->getVariants();
        
        // Collect all matched variants
        std::set<std::string> matched_variants;
        
        for (const auto& arm : arms) {
            if (auto* enum_pat = dynamic_cast<EnumPattern*>(arm.pattern.get())) {
                matched_variants.insert(enum_pat->getVariantName());
            }
        }
        
        // Check if all variants are covered
        if (matched_variants.size() == variants.size()) {
            // Check if each variant name is included
            bool all_covered = true;
            for (const auto& [variant_name, _] : variants) {
                if (matched_variants.find(variant_name) == matched_variants.end()) {
                    all_covered = false;
                    break;
                }
            }
            return all_covered;
        }
        
        return false;
    }
    
    // Other types: conservative handling
    return false;
}

void TypeChecker::visit(InterfaceDecl* node) {
    // Interface definition: already registered in Parser phase, skip type check here
    
    // Interface default method body not checked here, reasons:
    // - At interface definition time, Self is abstract type, don't know concrete fields
    // - Default method type check should be done during each support, when concrete type is known
    // - Current simplification: skip default method body type check (only used during CodeGen)
    
    // Future optimization: can substitute Self with concrete type during support, then check default method body
}

void TypeChecker::visit(SupportDecl* node) {
    // Support implementation: validate interface method completeness
    
    // 1. Lookup target type
    Type* target_type = types_->lookupType(node->getTypeName());
    if (!target_type) {
        diag_->reportError(
            "Type '" + node->getTypeName() + "' not found",
            SourceLocation()
        );
        return;
    }
    
    // Set Self type context
    Type* saved_self_type = current_self_type_;
    current_self_type_ = target_type;
    
    // 2. Lookup interface type
    Type* interface_type = types_->lookupType(node->getInterfaceName());
    if (!interface_type || interface_type->getKind() != Type::Kind::Interface) {
        diag_->reportError(
            "Interface '" + node->getInterfaceName() + "' not found",
            SourceLocation()
        );
        return;
    }
    
    auto* iface = static_cast<InterfaceType*>(interface_type);
    
    // Generic interface support: create generic parameter substitution map
    // If interface is generic (like Comparable<T>), and has instantiation parameters (like <Number>)
    // Need to substitute T with Number
    std::map<std::string, Type*> generic_substitution;
    
    if (iface->isGeneric() && node->isInterfaceGeneric()) {
        const auto& interface_def_param_names = iface->getGenericParamNames();
        const auto& interface_inst_params = node->getInterfaceGenericParams();
        
        std::cerr << "[GenericInterface] Building substitution map:" << std::endl;
        std::cerr << "  Interface: " << node->getInterfaceName() << std::endl;
        std::cerr << "  Generic params: " << interface_def_param_names.size() << std::endl;
        std::cerr << "  Instance params: " << interface_inst_params.size() << std::endl;
        
        // Build substitution map: T -> Number
        for (size_t i = 0; i < interface_def_param_names.size() && i < interface_inst_params.size(); ++i) {
            // Lookup instantiated type
            Type* concrete_type = types_->lookupType(interface_inst_params[i].name);
            if (concrete_type) {
                generic_substitution[interface_def_param_names[i]] = concrete_type;
                std::cerr << "  Mapping: " << interface_def_param_names[i] 
                          << " -> " << concrete_type->toString() << std::endl;
            }
        }
    } else {
        std::cerr << "[GenericInterface] No substitution:" << std::endl;
        std::cerr << "  isGeneric: " << iface->isGeneric() << std::endl;
        std::cerr << "  isInterfaceGeneric: " << node->isInterfaceGeneric() << std::endl;
    }
    
    // 3. Get all methods in interface definition
    const auto& required_methods = iface->getMethods();
    const auto& implemented_methods = node->getMethods();
    
    // 4. Validate all interface methods are implemented
    for (const auto& required : required_methods) {
        bool found = false;
        
        for (const auto& impl_method : implemented_methods) {
            if (impl_method->getName() == required.name) {
                found = true;
                
                // Validate method signature
                const auto& impl_params = impl_method->getParams();
                
                // Check parameter count (excluding self)
                if (impl_params.size() != required.param_types.size()) {
                    diag_->reportError(
                        "Method '" + required.name + "' has wrong number of parameters: " +
                        "expected " + std::to_string(required.param_types.size()) +
                        ", got " + std::to_string(impl_params.size()),
                        SourceLocation()
                    );
                }
                
                // Check parameter types (apply generic parameter substitution)
                for (size_t i = 0; i < impl_params.size() && i < required.param_types.size(); ++i) {
                    Type* expected_type = required.param_types[i];
                    
                    // Generic interface: substitute generic parameters
                    if (!generic_substitution.empty()) {
                        expected_type = substituteGenericType(expected_type, generic_substitution);
                    }
                    
                    if (!types_->equals(impl_params[i].type, expected_type)) {
                        diag_->reportError(
                            "Method '" + required.name + "' parameter " + std::to_string(i) +
                            " type mismatch: expected " + expected_type->toString() +
                            ", got " + impl_params[i].type->toString(),
                            SourceLocation()
                        );
                    }
                }
                
                // Check return type (apply generic parameter substitution)
                Type* expected_return = required.return_type;
                if (!generic_substitution.empty()) {
                    expected_return = substituteGenericType(expected_return, generic_substitution);
                }
                
                if (!types_->equals(impl_method->getReturnType(), expected_return)) {
                    diag_->reportError(
                        "Method '" + required.name + "' return type mismatch: " +
                        "expected " + expected_return->toString() +
                        ", got " + impl_method->getReturnType()->toString(),
                        SourceLocation()
                    );
                }
                
                break;
            }
        }
        
        if (!found) {
            // Check if interface method has default implementation
            if (!required.has_default_impl) {
            diag_->reportError(
                "Method '" + required.name + "' not implemented for interface '" +
                node->getInterfaceName() + "'",
                SourceLocation()
            );
            }
            // If has default implementation, don't report error (will use default implementation)
        }
    }
    
    // 5. Check all implemented methods
    for (const auto& impl_method : implemented_methods) {
        // Check method body under self parameter scope
        // TODO: Set self type to symbol table
        impl_method->accept(this);
    }
    
    // 6. Validate where constraints (conditional implementation)
    if (!node->getWhereClauses().empty()) {
        std::cerr << "[SupportDecl] Validating where clauses for " 
                  << node->getTypeName() << " with " << node->getInterfaceName() << std::endl;
        
        // Validate where constraint syntax (check interface existence and generic parameter validity)
        validateWhereConstraints(node->getWhereClauses(), node->getTypeGenericParams());
        
        // Note: Actual constraint satisfaction check happens during monomorphization
        // Because generic parameters are abstract at definition time
    }
    
    // 7. For methods with default implementation but not implemented, register function symbol and check body
    for (const auto& required : required_methods) {
        if (required.has_default_impl) {
            // Check if already implemented
            bool found = false;
            for (const auto& impl_method : implemented_methods) {
                if (impl_method->getName() == required.name) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::cerr << "[TypeChecker] Processing default method: " << required.name << std::endl;
                
                // Register default method's function symbol
                std::string method_name = node->getTypeName() + "::" + required.name;
                
                // Build function type
                std::vector<Type*> param_types_with_self;
                param_types_with_self.insert(
                    param_types_with_self.end(),
                    required.param_types.begin(),
                    required.param_types.end()
                );
                
                FunctionType* func_type = types_->getFunctionType(
                    param_types_with_self,
                    required.return_type
                );
                
                symbols_->defineFunction(method_name, func_type);
                
                // Key fix: perform type check on default method body
                // At this point current_self_type_ is already concrete type (target_type)
                if (required.default_body) {
                    std::cerr << "[TypeChecker] Checking default method body with concrete type: " 
                              << current_self_type_->toString() << std::endl;
                    
                    // Enter method scope
                    symbols_->enterScope();
                    
                    // Bind parameters (including self) - use concrete type + generic substitution!
                    for (size_t i = 0; i < required.param_types.size(); ++i) {
                        Type* param_type = required.param_types[i];
                        std::string param_name = (i < required.param_names.size()) 
                                               ? required.param_names[i] 
                                               : ("arg" + std::to_string(i));
                        
                        // Generic interface: apply generic parameter substitution (T -> Number)
                        if (!generic_substitution.empty()) {
                            param_type = substituteGenericType(param_type, generic_substitution);
                            std::cerr << "[GenericInterface] Substituted param " << param_name 
                                      << " type to " << param_type->toString() << std::endl;
                        }
                        
                        // Check if it's Self or &Self
                        if (param_type->getKind() == Type::Kind::SelfType) {
                            // Self (value) - substitute with concrete type
                            symbols_->defineVariable(param_name, current_self_type_, false);
                        } else if (param_type->getKind() == Type::Kind::Reference) {
                            auto* ref_type = static_cast<ReferenceType*>(param_type);
                            if (ref_type->getPointeeType()->getKind() == Type::Kind::SelfType) {
                                // &Self (reference) - create reference of concrete type
                                Type* concrete_ref = types_->getReferenceType(current_self_type_, ref_type->isMutable());
                                symbols_->defineVariable(param_name, concrete_ref, false);
                            } else {
                                // Regular parameter (generic substitution already applied)
                                symbols_->defineVariable(param_name, param_type, false);
                            }
                        } else {
                            // Other type parameters (generic substitution already applied)
                            symbols_->defineVariable(param_name, param_type, false);
                        }
                    }
                    
                    // Visit body for type check (now Self is concrete type, generic parameters already substituted)
                    required.default_body->accept(this);
                    
                    // Exit scope
                    symbols_->exitScope();
                    
                    std::cerr << "[TypeChecker] Default method body checked successfully" << std::endl;
                }
                
                std::cerr << "[TypeChecker] Registered default method: " << method_name << std::endl;
            }
        }
    }
    
    // Restore Self type context
    current_self_type_ = saved_self_type;
}

// Pattern type check - these methods called in checkPatternType
void TypeChecker::visit(LiteralPattern* node) {
    // Literal pattern: validate literal matches expected_type_
    // Current simplified version: no deep checking
}

void TypeChecker::visit(WildcardPattern* node) {
    // Wildcard pattern: always matches, no type check needed
}

void TypeChecker::visit(VariablePattern* node) {
    // Variable binding pattern: bind variable to expected_type_
    if (expected_type_) {
        symbols_->defineVariable(node->getName(), expected_type_, false);
    } else {
        // If no expected_type_, report error or use void
        diag_->reportError(
            "Cannot infer type for pattern variable '" + node->getName() + "'",
            SourceLocation()
        );
    }
}

void TypeChecker::visit(TuplePattern* node) {
    // Tuple pattern: recursively check each element
    if (!expected_type_ || expected_type_->getKind() != Type::Kind::Tuple) {
        return;
    }
    
    TupleType* tuple_type = static_cast<TupleType*>(expected_type_);
    const auto& elements = node->getElements();
    const auto& element_types = tuple_type->getElementTypes();
    
    if (elements.size() != element_types.size()) {
        diag_->reportError(
            "Tuple pattern size mismatch",
            SourceLocation()
        );
        return;
    }
    
    for (size_t i = 0; i < elements.size(); i++) {
        Type* saved_expected = expected_type_;
        expected_type_ = element_types[i];
        elements[i]->accept(this);
        expected_type_ = saved_expected;
    }
}

void TypeChecker::visit(EnumPattern* node) {
    // Need expected_type_ to know what type to match
    if (!expected_type_) {
        diag_->reportError("Cannot infer pattern type", SourceLocation());
        return;
    }
    
    Type* scrutinee_type = expected_type_;
    
    // === Special handling for Result type ===
    if (scrutinee_type->getKind() == Type::Kind::Result) {
        handleResultPattern(node, static_cast<ResultType*>(scrutinee_type));
        return;
    }
    
    // === Special handling for Optional type ===
    if (scrutinee_type->getKind() == Type::Kind::Optional) {
        handleOptionalPattern(node, static_cast<OptionalType*>(scrutinee_type));
        return;
    }
    
    // === Handle user-defined enum type ===
    if (scrutinee_type->getKind() == Type::Kind::Enum) {
        handleEnumPattern(node, static_cast<EnumType*>(scrutinee_type));
        return;
    }
    
    // Error: not an enum type
    diag_->reportError(
        "Cannot match enum pattern against non-enum type",
        SourceLocation()
    );
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Enum pattern handling helper methods
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::handleResultPattern(EnumPattern* node, ResultType* results_type) {
    std::string variant = node->getVariantName();
    
    // Ok variant
    if (variant == "Ok") {
        auto& inner_patterns = node->getInnerPatterns();
        
        // Check parameter count
        if (inner_patterns.size() != 1) {
            diag_->reportError(
                "Result::Ok expects exactly 1 argument",
                SourceLocation()
            );
            return;
        }
        
        // Recursively check inner pattern, expected type is results_type->getOkType()
        Type* saved_expected = expected_type_;
        expected_type_ = results_type->getOkType();
        inner_patterns[0]->accept(this);
        expected_type_ = saved_expected;
    }
    // Err variant
    else if (variant == "Err") {
        auto& inner_patterns = node->getInnerPatterns();
        
        if (inner_patterns.size() != 1) {
            diag_->reportError(
                "Result::Err expects exactly 1 argument",
                SourceLocation()
            );
            return;
        }
        
        // Err's parameter is fixed as string
        Type* saved_expected = expected_type_;
        expected_type_ = types_->getStringType();
        inner_patterns[0]->accept(this);
        expected_type_ = saved_expected;
    }
    else {
        diag_->reportError(
            "Unknown Result variant: " + variant + " (expected Ok or Err)",
            SourceLocation()
        );
    }
}

void TypeChecker::handleOptionalPattern(EnumPattern* node, OptionalType* opt_type) {
    std::string variant = node->getVariantName();
    
    if (variant == "Some") {
        auto& inner_patterns = node->getInnerPatterns();
        
        if (inner_patterns.size() != 1) {
            diag_->reportError(
                "Optional::Some expects exactly 1 argument",
                SourceLocation()
            );
            return;
        }
        
        Type* saved_expected = expected_type_;
        expected_type_ = opt_type->getInnerType();
        inner_patterns[0]->accept(this);
        expected_type_ = saved_expected;
    }
    else if (variant == "None") {
        // None has no parameters
        if (!node->getInnerPatterns().empty()) {
            diag_->reportError(
                "Optional::None takes no arguments",
                SourceLocation()
            );
        }
    }
    else {
        diag_->reportError(
            "Unknown Optional variant: " + variant + " (expected Some or None)",
            SourceLocation()
        );
    }
}

void TypeChecker::handleEnumPattern(EnumPattern* node, EnumType* enum_type) {
    std::string variant_name = node->getVariantName();
    auto& inner_patterns = node->getInnerPatterns();
    
    // Lookup variant definition
    const auto& variants = enum_type->getVariants();
    Type* variant_data_type = nullptr;
    bool found = false;
    
    for (const auto& variant : variants) {
        if (variant.first == variant_name) {
            variant_data_type = variant.second;
            found = true;
            break;
        }
    }
    
    if (!found) {
        diag_->reportError(
            "Unknown variant '" + variant_name + "' for enum type " + enum_type->getName(),
            SourceLocation()
        );
        return;
    }
    
    // Variant without data
    if (!variant_data_type) {
        if (!inner_patterns.empty()) {
        diag_->reportError(
                "Variant '" + variant_name + "' takes no arguments",
            SourceLocation()
        );
        }
        return;
    }
    
    // Multi-parameter support: check if it's tuple type
    if (variant_data_type->isTuple()) {
        // Multi-parameter variant: expand tuple into multiple types
        TupleType* tuple_type = static_cast<TupleType*>(variant_data_type);
        const auto& element_types = tuple_type->getElementTypes();
        
        // Check if pattern count matches
        if (inner_patterns.size() != element_types.size()) {
        diag_->reportError(
                "Variant '" + variant_name + "' expects " + 
                std::to_string(element_types.size()) + " arguments, got " +
                std::to_string(inner_patterns.size()),
            SourceLocation()
        );
        return;
    }
    
        // Set correct type for each inner pattern
        Type* saved_expected = expected_type_;
        for (size_t i = 0; i < inner_patterns.size(); ++i) {
            expected_type_ = element_types[i];
            inner_patterns[i]->accept(this);
        }
        expected_type_ = saved_expected;
    } else {
        // Single parameter variant
        if (inner_patterns.size() != 1) {
            diag_->reportError(
                "Variant '" + variant_name + "' expects exactly 1 argument",
                SourceLocation()
            );
            return;
        }
        
        Type* saved_expected = expected_type_;
        expected_type_ = variant_data_type;
        inner_patterns[0]->accept(this);
        expected_type_ = saved_expected;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Where constraint validation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::validateWhereConstraints(
    const std::vector<WhereClause>& where_clauses,
    const std::vector<GenericParam>& generic_params) {
    
    // For each where constraint
    for (const auto& clause : where_clauses) {
        // 1. Check if type_param is a generic parameter
        bool found = false;
        for (const auto& param : generic_params) {
            if (param.name == clause.type_param) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            diag_->reportError("Where clause references unknown type parameter: " + 
                              clause.type_param, SourceLocation());
            continue;
        }
        
        // 2. Check if interface exists
        Type* interface_type = types_->lookupType(clause.interface_name);
        if (!interface_type) {
            diag_->reportError("Unknown interface in where clause: " + 
                              clause.interface_name, SourceLocation());
            continue;
        }
        
        if (interface_type->getKind() != Type::Kind::Interface) {
            diag_->reportError("'" + clause.interface_name + 
                              "' is not an interface in where clause", 
                              SourceLocation());
            continue;
        }
        
        std::cerr << "[WhereClause] Validated: " << clause.type_param 
                  << " : " << clause.interface_name << std::endl;
    }
}

// Check if type implements interface
bool TypeChecker::typeImplementsInterface(Type* type, Type* interface_type) {
    if (!type || !interface_type) {
        return false;
    }
    
    if (interface_type->getKind() != Type::Kind::Interface) {
        return false;
    }
    
    auto* iface = static_cast<InterfaceType*>(interface_type);
    std::string interface_name = iface->getName();
    
    // Check if type has corresponding support implementation
    // By checking if methods in form TypeName::MethodName exist in symbol table
    
    if (!type->isStruct() && !type->isEnum()) {
        return false;  // Only struct and enum can implement interface
    }
    
    std::string type_name;
    if (type->isStruct()) {
        type_name = static_cast<StructType*>(type)->getName();
    } else if (type->isEnum()) {
        type_name = static_cast<EnumType*>(type)->getName();
    }
    
    // Check if all interface methods are implemented
    const auto& methods = iface->getMethods();
    for (const auto& method : methods) {
        // Try two naming conventions
        std::string method_name1 = type_name + "_" + method.name;
        std::string method_name2 = type_name + "::" + method.name;
        
        Symbol* method_symbol = symbols_->lookup(method_name1);
        if (!method_symbol) {
            method_symbol = symbols_->lookup(method_name2);
        }
        
        // If method not implemented and no default implementation, not satisfied
        if (!method_symbol && !method.has_default_impl) {
            return false;
        }
    }
    
    return true;
}

// Validate where constraints are satisfied
bool TypeChecker::checkWhereConstraintsSatisfied(
    const std::vector<WhereClause>& where_clauses,
    const std::map<std::string, Type*>& type_substitution) {
    
    for (const auto& clause : where_clauses) {
        // Get actual type (from substitution map)
        auto it = type_substitution.find(clause.type_param);
        if (it == type_substitution.end()) {
            std::cerr << "[WhereClause] Type parameter not found in substitution: " 
                      << clause.type_param << std::endl;
            continue;
        }
        
        Type* actual_type = it->second;
        Type* interface_type = types_->lookupType(clause.interface_name);
        
        if (!interface_type) {
            diag_->reportError("Unknown interface in where clause: " + 
                              clause.interface_name, SourceLocation());
            return false;
        }
        
        // Check if type implements interface
        if (!typeImplementsInterface(actual_type, interface_type)) {
            diag_->reportError(
                "Type '" + actual_type->toString() + 
                "' does not implement interface '" + clause.interface_name + 
                "' required by where clause",
                SourceLocation()
            );
            return false;
        }
        
        std::cerr << "[WhereClause] Constraint satisfied: " 
                  << actual_type->toString() << " : " << clause.interface_name << std::endl;
    }
    
    return true;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Self type resolution
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Type* TypeChecker::resolveSelfType(Type* type) {
    if (!type) {
        return nullptr;
    }
    
    // If it's SelfType, replace with current Self type
    if (type->getKind() == Type::Kind::SelfType) {
        if (current_self_type_) {
            return current_self_type_;
        }
        // If no current Self type, report error
        diag_->reportError("Self type used outside of support context", SourceLocation());
        return types_->getVoidType();
    }
    
    // Recursively handle composite types
    switch (type->getKind()) {
        case Type::Kind::Optional: {
            auto* opt = static_cast<OptionalType*>(type);
            Type* inner = resolveSelfType(opt->getInnerType());
            return types_->getOptionalType(inner);
        }
        case Type::Kind::Result: {
            auto* res = static_cast<ResultType*>(type);
            Type* ok_type = resolveSelfType(res->getOkType());
            return types_->getResultType(ok_type);
        }
        case Type::Kind::Array: {
            auto* arr = static_cast<ArrayType*>(type);
            Type* elem = resolveSelfType(arr->getElementType());
            return types_->getArrayType(elem, arr->getSize());
        }
        case Type::Kind::Slice: {
            auto* slice = static_cast<SliceType*>(type);
            Type* elem = resolveSelfType(slice->getElementType());
            return types_->getSliceType(elem);
        }
        case Type::Kind::Function: {
            auto* func = static_cast<FunctionType*>(type);
            std::vector<Type*> params;
            for (auto* param : func->getParamTypes()) {
                params.push_back(resolveSelfType(param));
            }
            Type* ret = resolveSelfType(func->getReturnType());
            return types_->getFunctionType(params, ret);
        }
        case Type::Kind::Reference: {
            // Reference type: resolve Self in pointee
            auto* ref = static_cast<ReferenceType*>(type);
            Type* pointee = resolveSelfType(ref->getPointeeType());
            return types_->getReferenceType(pointee, ref->isMutable());
        }
        default:
            // Base type, no substitution needed
            return type;
    }
}

// Generic parameter substitution helper function
Type* TypeChecker::substituteGenericType(Type* type, const std::map<std::string, Type*>& substitution) {
    if (!type) return nullptr;
    
    // If it's generic type, lookup substitution
    if (type->getKind() == Type::Kind::Generic) {
        auto* generic = static_cast<GenericType*>(type);
        auto it = substitution.find(generic->getName());
        if (it != substitution.end()) {
            return it->second;  // Return substituted type
        }
        return type;  // No substitution found, keep as is
    }
    
    // Recursively handle composite types
    // TODO: If need to support more complex cases (like Pair<T, U>), need recursive handling
    
    return type;
}

void TypeChecker::visit(ArrayPattern* node) {
    // Array pattern matching - complete implementation
    
    // 1. Check if expected_type_ is ArrayType
    if (!expected_type_) {
        diag_->reportError(
            "Array pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    if (expected_type_->getKind() != Type::Kind::Array) {
        diag_->reportError(
            "Cannot match array pattern against non-array type: " + expected_type_->toString(),
            SourceLocation()
        );
        return;
    }
    
    ArrayType* array_type = static_cast<ArrayType*>(expected_type_);
    
    // 2. Check if array size matches
    if (array_type->getSize() != node->getExpectedSize()) {
        diag_->reportError(
            "Array pattern size mismatch: expected " + 
            std::to_string(array_type->getSize()) + 
            ", got " + std::to_string(node->getExpectedSize()),
            SourceLocation()
        );
        return;
    }
    
    // 3. Recursively check each element pattern
    Type* elem_type = array_type->getElementType();
    for (const auto& elem_pattern : node->getElements()) {
        Type* saved_expected = expected_type_;
        expected_type_ = elem_type;
        elem_pattern->accept(this);
        expected_type_ = saved_expected;
    }
}

void TypeChecker::visit(SlicePattern* node) {
    // Slice pattern matching - support Array and Slice types
    
    if (!expected_type_) {
        diag_->reportError(
            "Slice pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    Type* elem_type = nullptr;
    Type* rest_type = nullptr;
    
    // Support both Array and Slice types
    if (expected_type_->getKind() == Type::Kind::Array) {
        auto* array_type = static_cast<ArrayType*>(expected_type_);
        elem_type = array_type->getElementType();
        
        // Check prefix count doesn't exceed array size
        if (node->getPrefix().size() > array_type->getSize()) {
            diag_->reportError(
                "Slice pattern prefix too long: array size is " +
                std::to_string(array_type->getSize()) +
                ", but pattern requires at least " +
                std::to_string(node->getPrefix().size()) + " elements",
                SourceLocation()
            );
            return;
        }
        
        // rest part is Slice type
        rest_type = types_->getSliceType(elem_type);
        
    } else if (expected_type_->getKind() == Type::Kind::Slice) {
        auto* slice_type = static_cast<SliceType*>(expected_type_);
        elem_type = slice_type->getElementType();
        rest_type = slice_type;  // rest is also Slice type
        
    } else {
        diag_->reportError(
            "Cannot match slice pattern against non-array/slice type: " + 
            expected_type_->toString(),
            SourceLocation()
        );
        return;
    }
    
    // Check prefix elements
    for (const auto& prefix_pattern : node->getPrefix()) {
        Type* saved_expected = expected_type_;
        expected_type_ = elem_type;
        prefix_pattern->accept(this);
        expected_type_ = saved_expected;
    }
    
    // Check suffix elements
    for (const auto& suffix_pattern : node->getSuffix()) {
        Type* saved_expected = expected_type_;
        expected_type_ = elem_type;
        suffix_pattern->accept(this);
        expected_type_ = saved_expected;
    }
    
    // Check if array size is sufficient (if Array type)
    if (expected_type_->getKind() == Type::Kind::Array) {
        auto* array_type = static_cast<ArrayType*>(expected_type_);
        size_t min_size = node->getPrefix().size() + node->getSuffix().size();
        
        if (min_size > array_type->getSize()) {
            diag_->reportError(
                "Slice pattern requires at least " + std::to_string(min_size) +
                " elements, but array size is " + std::to_string(array_type->getSize()),
                SourceLocation()
            );
            return;
        }
    }
    
    // Check rest part
    if (node->hasRest()) {
        Type* saved_expected = expected_type_;
        expected_type_ = rest_type;
        node->getRest()->accept(this);
        expected_type_ = saved_expected;
    }
}

/// Type check a range pattern (e.g., 1..10, 'a'..'z')
///
/// Validates:
///   1. Pattern has expected type context
///   2. Expected type is integer or char (range patterns only support these)
///   3. Start and end patterns are well-typed
///
/// Type constraints:
///   - Only integer types (i8-i128, u8-u128) and char are allowed
///   - Start and end must be literal values of the same type
///   - Type is inherited from the scrutinee being matched
///
/// Error conditions:
///   - No expected type context
///   - Expected type is not integer or char
///   - Start or end pattern has wrong type
void TypeChecker::visit(RangePattern* node) {
    // Ensure we have type context from the scrutinee
    if (!expected_type_) {
        diag_->reportError(
            "Range pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    // Range patterns only work with integer and character types
    // This is a semantic constraint - ranges need ordered types with successor/predecessor
    if (!expected_type_->isInteger() && expected_type_->getKind() != Type::Kind::Char) {
        diag_->reportError(
            "Range pattern only supports integer and char types, got: " + 
            expected_type_->toString(),
            SourceLocation()
        );
        return;
    }
    
    // Type check start pattern (usually a literal)
    if (node->getStart()) {
        // Start pattern should match the expected type (no need to change expected_type_)
        node->getStart()->accept(this);
    }
    
    // Type check end pattern (usually a literal)
    if (node->getEnd()) {
        // End pattern should match the expected type (no need to change expected_type_)
        node->getEnd()->accept(this);
    }
}

/// Type check an OR pattern (e.g., 1 | 2 | 3)
///
/// Validates:
///   1. Pattern has expected type context
///   2. All alternative patterns are well-typed
///   3. All alternatives are compatible with expected type
///
/// Type constraints:
///   - All alternatives must be type-compatible with the scrutinee
///   - Type checking is performed independently for each alternative
///   - Variables bound by alternatives must be consistent (checked elsewhere)
///
/// Note: The actual type compatibility is enforced by checking each alternative
/// against the expected_type. The alternatives don't need to be identical types,
/// just compatible with the scrutinee type.
///
/// Error conditions:
///   - No expected type context
///   - Any alternative has incompatible type
void TypeChecker::visit(OrPattern* node) {
    // Ensure we have type context from the scrutinee
    if (!expected_type_) {
        diag_->reportError(
            "Or pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    // Type check each alternative pattern independently
    // Each alternative is checked against the same expected type
    for (const auto& alt : node->getAlternatives()) {
        // Each alternative should match the expected type (no need to change expected_type_)
        alt->accept(this);
    }
}

void TypeChecker::visit(StructPattern* node) {
    // structbody/structpattern matching - completeimplementation
    
    // 1. checkexpected_type_yesnois/asStructType
    if (!expected_type_) {
        diag_->reportError(
            "Struct pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    if (expected_type_->getKind() != Type::Kind::Struct) {
        diag_->reportError(
            "Cannot match struct pattern against non-struct type: " + expected_type_->toString(),
            SourceLocation()
        );
        return;
    }
    
    StructType* struct_type = static_cast<StructType*>(expected_type_);
    
    // 2. checkstructbody/structnameyesnomatch
    if (struct_type->getName() != node->getStructName()) {
        diag_->reportError(
            "Struct pattern name mismatch: expected " + struct_type->getName() + 
            ", got " + node->getStructName(),
            SourceLocation()
        );
        return;
    }
    
    // 3. Check each field pattern
    for (const auto& field_pattern : node->getFields()) {
        const std::string& field_name = field_pattern.field_name;
        
        // 3.1 Check if field exists
        Type* field_type = struct_type->getFieldType(field_name);
        if (!field_type) {
            diag_->reportError(
                "Struct " + struct_type->getName() + " has no field named '" + field_name + "'",
                SourceLocation()
            );
            continue;
        }
        
        // 3.2 Recursively check field pattern, expected type is field type
        Type* saved_expected = expected_type_;
        expected_type_ = field_type;
        field_pattern.pattern->accept(this);
        expected_type_ = saved_expected;
    }
    
    // Note: Don't check if all fields are matched, because struct pattern can be partial match
}

// Generic function return type instantiation
Type* TypeChecker::instantiateFunctionReturnType(GenericTemplate* tmpl, 
                                                  const std::vector<Type*>& type_args) {
    if (!tmpl || tmpl->kind != GenericTemplate::FUNCTION) {
        return nullptr;
    }
    
    FunctionDecl* func_def = tmpl->func_def;
    
    // Create type substitution map
    std::unordered_map<std::string, Type*> substitution;
    for (size_t i = 0; i < tmpl->type_params.size() && i < type_args.size(); i++) {
        substitution[tmpl->type_params[i]] = type_args[i];
    }
    
    // Substitute type parameters in return type
    return types_->substituteType(func_def->getReturnType(), substitution);
}

} // namespace pawc
