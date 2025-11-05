//===--- monomorphization_pass.cpp - Monomorphization Pass ------*- C++ -*-===//
/// @file monomorphization_pass.cpp
/// @brief Implementation file
///

#include "monomorphization_pass.h"
#include "backend/codegen/generic/mangling.h"  // Use new Mangling module
#include "pass/pass_context.h"
#include "middleend/types/type_system.h"
#include "frontend/parser/ast/pattern.h"
#include <iostream>

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pass main entry point
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PassResult MonomorphizationPass::runImpl(PassContext* context) {
    context_ = context;
    
    auto* ast = context->getAST();
    if (!ast || ast->empty()) {
        return PassResult{true, "No AST for monomorphization"};
    }
    
    // First pass: collect all generic definitions
    collectGenericDefinitions(*ast);
    
    // Second pass: traverse AST, find generic use sites and generate monomorphized instances
    for (const auto& stmt : *ast) {
        stmt->accept(this);
    }
    
    // Register monomorphized instances (TypeChecker will process them)
    // Note: Monomorphized functions and types will be processed in subsequent TypeChecker/CodeGen
    // because they've been added to monomorphized_stmts_, and will be traversed later
    
    // Record generated instance count to context
    if (!monomorphized_stmts_.empty()) {
        context->cacheAnalysisResult("monomorphized_instances", 
                                     static_cast<int>(monomorphized_stmts_.size()));
    }
    
    // Debug info
    std::string results_msg = "Monomorphization completed, generated " + 
                             std::to_string(monomorphized_stmts_.size()) + " instances";
    if (!instance_cache_.empty()) {
        results_msg += " (cache size: " + std::to_string(instance_cache_.size()) + ")";
    }
    
    return PassResult{true, results_msg};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Collect generic definitions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void MonomorphizationPass::collectGenericDefinitions(const std::vector<StmtPtr>& ast) {
    for (const auto& stmt : ast) {
        // StructDecl with generics
        if (auto* struct_decl = dynamic_cast<StructDecl*>(stmt.get())) {
            if (struct_decl->isGeneric()) {
                generic_structs_[struct_decl->getName()] = struct_decl;
            }
        }
        // EnumDecl with generics
        else if (auto* enum_decl = dynamic_cast<EnumDecl*>(stmt.get())) {
            if (enum_decl->isGeneric()) {
                generic_enums_[enum_decl->getName()] = enum_decl;
            }
        }
        // FunctionDecl with generics
        else if (auto* func_decl = dynamic_cast<FunctionDecl*>(stmt.get())) {
            if (func_decl->isGeneric()) {
                generic_functions_[func_decl->getName()] = func_decl;
            }
        }
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// helper function
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::vector<Type*> MonomorphizationPass::inferTypeArgsFromStructLiteral(
    StructLiteral* lit, StructDecl* generic_decl) {
    
    // Infer generic parameters from field value types
    // Example: Box { value: 42 } => T = i32
    
    std::vector<Type*> type_args;
    const auto& generic_params = generic_decl->getGenericParams();
    const auto& generic_fields = generic_decl->getFields();
    
    if (generic_params.empty()) {
        return type_args;  // Non-generic struct
    }
    
    // Build type parameter map
    std::map<std::string, Type*> type_param_map;
    
    // Traverse fields, try to infer types from literals
    for (const auto& field_init : lit->getFields()) {
        // Find corresponding field declaration
        for (const auto& field_decl : generic_fields) {
            if (field_decl.first == field_init.name) {
                // Infer type from field value
                Type* inferred_type = inferTypeFromExpr(field_init.value.get());
                
                if (inferred_type && field_decl.second) {
                    // If field type is generic parameter, record inference results
                    std::string field_type_name = field_decl.second->toString();
                    
                    if (type_param_map.find(field_type_name) == type_param_map.end()) {
                        type_param_map[field_type_name] = inferred_type;
                    }
                }
                break;
            }
        }
    }
    
    // Build type_args in generic parameter order
    for (const auto& param : generic_params) {
        auto it = type_param_map.find(param.name);
        if (it != type_param_map.end()) {
            type_args.push_back(it->second);
        } else {
            // If a generic parameter cannot be inferred, return empty (inference failure)
            // TODO: Consider default types or error reporting
            return std::vector<Type*>{};
        }
    }
    
    return type_args;
}

std::string MonomorphizationPass::generateMonomorphizedName(
    const std::string& base_name, const std::vector<Type*>& type_args) {
    
    // Generate monomorphized name: Box<i32> -> Box_i32
    std::string name = base_name;
    for (auto* type : type_args) {
        name += "_" + type->toString();
    }
    return name;
}

Type* MonomorphizationPass::substituteType(
    Type* type, const std::map<std::string, Type*>& type_mapping) {
    
    if (!type) return nullptr;
    
    // If it's generic parameter type (by name matching), substitute with concrete type
    auto it = type_mapping.find(type->toString());
    if (it != type_mapping.end()) {
        return it->second;
    }
    
    // Process composite types (recursive substitution)
    auto* type_system = context_->getTypeSystem();
    
    switch (type->getKind()) {
        case Type::Kind::Array: {
            auto* array_type = static_cast<ArrayType*>(type);
            Type* new_elem = substituteType(array_type->getElementType(), type_mapping);
            if (new_elem != array_type->getElementType()) {
                return type_system->getArrayType(new_elem, array_type->getSize());
            }
            break;
        }
        
        case Type::Kind::Slice: {
            auto* slice_type = static_cast<SliceType*>(type);
            Type* new_elem = substituteType(slice_type->getElementType(), type_mapping);
            if (new_elem != slice_type->getElementType()) {
                return type_system->getSliceType(new_elem);
            }
            break;
        }
        
        case Type::Kind::Tuple: {
            auto* tuple_type = static_cast<TupleType*>(type);
            std::vector<Type*> new_elements;
            bool changed = false;
            
            for (auto* elem : tuple_type->getElementTypes()) {
                Type* new_elem = substituteType(elem, type_mapping);
                new_elements.push_back(new_elem);
                if (new_elem != elem) changed = true;
            }
            
            if (changed) {
                return type_system->getTupleType(new_elements);
            }
            break;
        }
        
        case Type::Kind::Optional: {
            auto* opt_type = static_cast<OptionalType*>(type);
            Type* new_inner = substituteType(opt_type->getInnerType(), type_mapping);
            if (new_inner != opt_type->getInnerType()) {
                return type_system->getOptionalType(new_inner);
            }
            break;
        }
        
        case Type::Kind::Result: {
            auto* results_type = static_cast<ResultType*>(type);
            Type* new_ok = substituteType(results_type->getOkType(), type_mapping);
            if (new_ok != results_type->getOkType()) {
                return type_system->getResultType(new_ok);
            }
            break;
        }
        
        case Type::Kind::Reference: {
            auto* ref_type = static_cast<ReferenceType*>(type);
            Type* new_pointee = substituteType(ref_type->getPointeeType(), type_mapping);
            if (new_pointee != ref_type->getPointeeType()) {
                return type_system->getReferenceType(new_pointee, ref_type->isMutable());
            }
            break;
        }
        
        case Type::Kind::Struct: {
            // For StructType, check if it's unmonomorphized version of generic struct
            // Example: Box<T> needs recursive substitution of T
            auto* struct_type = static_cast<StructType*>(type);
            std::vector<StructType::Field> new_fields;
            bool changed = false;
            
            for (const auto& field : struct_type->getFields()) {
                Type* new_field_type = substituteType(field.second, type_mapping);
                new_fields.push_back({field.first, new_field_type});
                if (new_field_type != field.second) changed = true;
            }
            
            if (changed) {
                // Create new StructType
                return new StructType(struct_type->getName(), new_fields);
            }
            break;
        }
        
        default:
            // Base types, no substitution needed
            break;
    }
    
    return type;
}

std::vector<Type*> MonomorphizationPass::inferTypeArgsFromCallExpr(
    CallExpr* call, FunctionDecl* generic_func) {
    
    // Infer type parameters from function call arguments
    // Example: identity(42) => T = i32
    
    std::vector<Type*> type_args;
    const auto& generic_params = generic_func->getGenericParams();
    const auto& func_params = generic_func->getParams();
    const auto& call_args = call->getArgs();
    
    if (generic_params.empty()) {
        return type_args;  // Non-generic function
    }
    
    // Build type parameter map
    std::map<std::string, Type*> type_param_map;
    
    // Traverse parameters, infer types
    for (size_t i = 0; i < func_params.size() && i < call_args.size(); ++i) {
        // Infer type from call argument
        Type* arg_type = inferTypeFromExpr(call_args[i].get());
        
        if (arg_type && func_params[i].type) {
            // If function parameter type is generic parameter, record inference results
            std::string param_type_name = func_params[i].type->toString();
            if (type_param_map.find(param_type_name) == type_param_map.end()) {
                type_param_map[param_type_name] = arg_type;
            }
        }
    }
    
    // Build type_args in generic parameter order
    for (const auto& param : generic_params) {
        auto it = type_param_map.find(param.name);
        if (it != type_param_map.end()) {
            type_args.push_back(it->second);
        } else {
            // Inference failure
            return std::vector<Type*>{};
        }
    }
    
    return type_args;
}

Type* MonomorphizationPass::inferTypeFromExpr(Expr* expr) {
    // fromexpressionliteralinfertypes
    if (!expr) return nullptr;
    
    auto* type_system = context_->getTypeSystem();
    
    // Integer literal
    if (auto* int_lit = dynamic_cast<IntLiteral*>(expr)) {
        return type_system->getI32Type();  // Default i32
    }
    
    // Floating-point literal
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(expr)) {
        return type_system->getF64Type();  // Default f64
    }
    
    // Boolean literal
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(expr)) {
        return type_system->getBoolType();
    }
    
    // Character literal
    if (auto* char_lit = dynamic_cast<CharLiteral*>(expr)) {
        return type_system->getCharType();
    }
    
    // String literal
    if (auto* str_lit = dynamic_cast<StringLiteral*>(expr)) {
        return type_system->getStringType();
    }
    
    // StructLiteral - process nested generics
    if (auto* struct_lit = dynamic_cast<StructLiteral*>(expr)) {
        // First ensure inner StructLiteral is processed
        struct_lit->accept(this);
        
        // Check if it's generic struct
        auto it = generic_structs_.find(struct_lit->getStructName());
        if (it != generic_structs_.end()) {
            // Infer type parameters
            auto type_args = inferTypeArgsFromStructLiteral(struct_lit, it->second);
            if (!type_args.empty()) {
                // Return monomorphized StructType
                std::string mono_name = generateMonomorphizedName(
                    struct_lit->getStructName(), type_args);
                
                // Lookup registered monomorphized type
                return type_system->lookupType(mono_name);
            }
        }
        
        // If already has type (been processed), use it
        if (expr->getType()) {
            return expr->getType();
        }
        
        // Lookup non-generic struct type
        return type_system->lookupType(struct_lit->getStructName());
    }
    
    // If already has type (set by TypeChecker), use it
    if (expr->getType()) {
        return expr->getType();
    }
    
    return nullptr;
}

StmtPtr MonomorphizationPass::cloneAndSubstitute(
    StructDecl* generic_decl, const std::vector<Type*>& type_args) {
    
    // Build type map: T -> i32, U -> string, etc.
    std::map<std::string, Type*> type_mapping;
    const auto& generic_params = generic_decl->getGenericParams();
    
    for (size_t i = 0; i < generic_params.size() && i < type_args.size(); ++i) {
        type_mapping[generic_params[i].name] = type_args[i];
    }
    
    // Generate new name
    std::string new_name = generateMonomorphizedName(generic_decl->getName(), type_args);
    
    // Clone fields and substitute types
    std::vector<StructDecl::Field> new_fields;
    for (const auto& field : generic_decl->getFields()) {
        Type* new_type = substituteType(field.second, type_mapping);
        new_fields.push_back({field.first, new_type});
    }
    
    // Create StructType and register to TypeSystem
    auto* type_system = context_->getTypeSystem();
    std::vector<StructType::Field> struct_type_fields;
    for (const auto& field : new_fields) {
        struct_type_fields.push_back({field.first, field.second});
    }
    
    auto* struct_type = new StructType(new_name, struct_type_fields);
    type_system->registerStruct(struct_type);
    
    // Create new StructDecl (without generic parameters)
    return std::make_unique<StructDecl>(
        new_name, 
        std::vector<GenericParam>{},  // No generic parameters after monomorphization
        std::move(new_fields)
    );
}

StmtPtr MonomorphizationPass::cloneAndSubstitute(
    EnumDecl* generic_decl, const std::vector<Type*>& type_args) {
    
    // Build type map
    std::map<std::string, Type*> type_mapping;
    const auto& generic_params = generic_decl->getGenericParams();
    
    for (size_t i = 0; i < generic_params.size() && i < type_args.size(); ++i) {
        type_mapping[generic_params[i].name] = type_args[i];
    }
    
    // Generate new name
    std::string new_name = generateMonomorphizedName(generic_decl->getName(), type_args);
    
    // Clone variants and substitute types
    std::vector<EnumVariant> new_variants;
    for (const auto& variant : generic_decl->getVariants()) {
        std::vector<Type*> new_data_types;
        for (Type* type : variant.data_types) {
            new_data_types.push_back(substituteType(type, type_mapping));
        }
        new_variants.push_back(EnumVariant(variant.name, new_data_types));
    }
    
    // Create new EnumDecl
    return std::make_unique<EnumDecl>(
        new_name,
        std::vector<GenericParam>{},  // No generic parameters after monomorphization
        std::move(new_variants)
    );
}

StmtPtr MonomorphizationPass::cloneAndSubstitute(
    FunctionDecl* generic_decl, const std::vector<Type*>& type_args) {
    
    // Build type map
    std::map<std::string, Type*> type_mapping;
    const auto& generic_params = generic_decl->getGenericParams();
    
    for (size_t i = 0; i < generic_params.size() && i < type_args.size(); ++i) {
        type_mapping[generic_params[i].name] = type_args[i];
    }
    
    // Generate new name
    std::string new_name = generateMonomorphizedName(generic_decl->getName(), type_args);
    
    // Clone parameters and substitute types
    std::vector<FunctionDecl::Param> new_params;
    for (const auto& param : generic_decl->getParams()) {
        Type* new_param_type = substituteType(param.type, type_mapping);
        new_params.push_back({param.name, new_param_type, param.is_mutable});
    }
    
    // Substitute return type
    Type* new_return_type = substituteType(generic_decl->getReturnType(), type_mapping);
    
    // Clone function body (deep clone AST)
    StmtPtr new_body = cloneStmt(generic_decl->getBody(), type_mapping);
    
    // Create new FunctionDecl
    return std::make_unique<FunctionDecl>(
        new_name,
        std::vector<GenericParam>{},  // No generic parameters after monomorphization
        std::move(new_params),
        new_return_type,
        std::move(new_body)
    );
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// AST cloning (for function body cloning)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

StmtPtr MonomorphizationPass::cloneStmt(Stmt* stmt, const std::map<std::string, Type*>& type_mapping) {
    if (!stmt) return nullptr;
    
    // BlockStmt
    if (auto* block = dynamic_cast<BlockStmt*>(stmt)) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& s : block->getStmts()) {
            new_stmts.push_back(cloneStmt(s.get(), type_mapping));
        }
        return std::make_unique<BlockStmt>(std::move(new_stmts));
    }
    
    // ReturnStmt
    if (auto* ret = dynamic_cast<ReturnStmt*>(stmt)) {
        return std::make_unique<ReturnStmt>(cloneExpr(ret->getValue(), type_mapping));
    }
    
    // ExprStmt
    if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
        return std::make_unique<ExprStmt>(cloneExpr(expr_stmt->getExpr(), type_mapping));
    }
    
    // VarDecl
    if (auto* var = dynamic_cast<VarDecl*>(stmt)) {
        Type* new_type = substituteType(var->getType(), type_mapping);
        return std::make_unique<VarDecl>(
            var->getName(), new_type, var->isMutable(),
            cloneExpr(var->getInit(), type_mapping)
        );
    }
    
    // IfStmt
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt)) {
        return std::make_unique<IfStmt>(
            cloneExpr(if_stmt->getCondition(), type_mapping),
            cloneStmt(if_stmt->getThenStmt(), type_mapping),
            cloneStmt(if_stmt->getElseStmt(), type_mapping)
        );
    }
    
    // LoopStmt
    if (auto* loop = dynamic_cast<LoopStmt*>(stmt)) {
        return std::make_unique<LoopStmt>(cloneStmt(loop->getBody(), type_mapping));
    }
    
    // BreakStmt
    if (dynamic_cast<BreakStmt*>(stmt)) {
        return std::make_unique<BreakStmt>();
    }
    
    // ContinueStmt
    if (dynamic_cast<ContinueStmt*>(stmt)) {
        return std::make_unique<ContinueStmt>();
    }
    
    // ━━━ Other statement types ━━━
    
    // StructDecl (non-generic or already instantiated)
    if (auto* struct_decl = dynamic_cast<StructDecl*>(stmt)) {
        std::vector<std::pair<std::string, Type*>> new_fields;
        for (const auto& field : struct_decl->getFields()) {
            Type* new_type = substituteType(field.second, type_mapping);
            new_fields.emplace_back(field.first, new_type);
        }
        return std::make_unique<StructDecl>(
            struct_decl->getName(),
            struct_decl->getGenericParams(),
            new_fields
        );
    }
    
    // EnumDecl (non-generic or already instantiated)
    if (auto* enum_decl = dynamic_cast<EnumDecl*>(stmt)) {
        std::vector<EnumVariant> new_variants;
        for (const auto& variant : enum_decl->getVariants()) {
            std::vector<Type*> new_data_types;
            for (Type* type : variant.data_types) {
                new_data_types.push_back(substituteType(type, type_mapping));
            }
            new_variants.emplace_back(variant.name, new_data_types);
        }
        return std::make_unique<EnumDecl>(
            enum_decl->getName(),
            enum_decl->getGenericParams(),
            new_variants
        );
    }
    
    // InterfaceDecl and SupportDecl don't need cloning (they won't be inlined into function body)
    
    return nullptr;
}

ExprPtr MonomorphizationPass::cloneExpr(Expr* expr, const std::map<std::string, Type*>& type_mapping) {
    if (!expr) return nullptr;
    
    // Literals
    if (auto* int_lit = dynamic_cast<IntLiteral*>(expr)) {
        return std::make_unique<IntLiteral>(int_lit->getValue());
    }
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(expr)) {
        return std::make_unique<FloatLiteral>(float_lit->getValue());
    }
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(expr)) {
        return std::make_unique<BoolLiteral>(bool_lit->getValue());
    }
    if (auto* char_lit = dynamic_cast<CharLiteral*>(expr)) {
        return std::make_unique<CharLiteral>(char_lit->getValue());
    }
    if (auto* str_lit = dynamic_cast<StringLiteral*>(expr)) {
        return std::make_unique<StringLiteral>(str_lit->getValue());
    }
    
    // IdentifierExpr
    if (auto* id = dynamic_cast<IdentifierExpr*>(expr)) {
        return std::make_unique<IdentifierExpr>(id->getName());
    }
    
    // BinaryExpr
    if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) {
        return std::make_unique<BinaryExpr>(
            bin->getOperator(),
            cloneExpr(bin->getLeft(), type_mapping),
            cloneExpr(bin->getRight(), type_mapping)
        );
    }
    
    // UnaryExpr
    if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
        return std::make_unique<UnaryExpr>(
            unary->getOperator(),
            cloneExpr(unary->getOperand(), type_mapping)
        );
    }
    
    // CallExpr
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        std::vector<ExprPtr> new_args;
        for (const auto& arg : call->getArgs()) {
            new_args.push_back(cloneExpr(arg.get(), type_mapping));
        }
        return std::make_unique<CallExpr>(
            cloneExpr(call->getCallee(), type_mapping),
            std::move(new_args)
        );
    }
    
    // MemberExpr
    if (auto* member = dynamic_cast<MemberExpr*>(expr)) {
        return std::make_unique<MemberExpr>(
            cloneExpr(member->getObject(), type_mapping),
            member->getMember()
        );
    }
    
    // ━━━ Other expression types ━━━
    
    // IndexExpr
    if (auto* index = dynamic_cast<IndexExpr*>(expr)) {
        return std::make_unique<IndexExpr>(
            cloneExpr(index->getObject(), type_mapping),
            cloneExpr(index->getIndex(), type_mapping)
        );
    }
    
    // TupleExpr
    if (auto* tuple = dynamic_cast<TupleExpr*>(expr)) {
        std::vector<ExprPtr> new_elements;
        for (const auto& elem : tuple->getElements()) {
            new_elements.push_back(cloneExpr(elem.get(), type_mapping));
        }
        return std::make_unique<TupleExpr>(std::move(new_elements));
    }
    
    // IfExpr
    if (auto* if_expr = dynamic_cast<IfExpr*>(expr)) {
        return std::make_unique<IfExpr>(
            cloneExpr(if_expr->getCondition(), type_mapping),
            cloneExpr(if_expr->getThenExpr(), type_mapping),
            cloneExpr(if_expr->getElseExpr(), type_mapping)
        );
    }
    
    // BlockExpr
    if (auto* block = dynamic_cast<BlockExpr*>(expr)) {
        std::vector<StmtPtr> new_stmts;
        for (const auto& stmt : block->getStmts()) {
            new_stmts.push_back(cloneStmt(stmt.get(), type_mapping));
        }
        return std::make_unique<BlockExpr>(std::move(new_stmts));
    }
    
    // RangeExpr
    if (auto* range = dynamic_cast<RangeExpr*>(expr)) {
        return std::make_unique<RangeExpr>(
            cloneExpr(range->getStart(), type_mapping),
            cloneExpr(range->getEnd(), type_mapping),
            range->isInclusive()
        );
    }
    
    // StaticAccessExpr
    if (auto* access = dynamic_cast<StaticAccessExpr*>(expr)) {
        auto new_expr = std::make_unique<StaticAccessExpr>(
            access->getTypeName(),
            access->getMember()
        );
        // Copy generic parameters (if any)
        if (access->getType()) {
            new_expr->setType(substituteType(access->getType(), type_mapping));
        }
        return new_expr;
    }
    
    // TryExpr
    if (auto* try_expr = dynamic_cast<TryExpr*>(expr)) {
        return std::make_unique<TryExpr>(
            cloneExpr(try_expr->getExpr(), type_mapping),
            try_expr->getOperator()
        );
    }
    
    // CastExpr
    if (auto* cast = dynamic_cast<CastExpr*>(expr)) {
        return std::make_unique<CastExpr>(
            cloneExpr(cast->getExpr(), type_mapping),
            substituteType(cast->getTargetType(), type_mapping)
        );
    }
    
    // NoneLiteral  
    if (dynamic_cast<NoneLiteral*>(expr)) {
        return std::make_unique<NoneLiteral>();
    }
    
    // SelfExpr
    if (dynamic_cast<SelfExpr*>(expr)) {
        return std::make_unique<SelfExpr>();
    }
    
    // StructLiteral (already processed above, this is fallback)
    if (auto* struct_lit = dynamic_cast<StructLiteral*>(expr)) {
        std::vector<FieldInit> new_fields;
        for (const auto& field : struct_lit->getFields()) {
            new_fields.push_back(FieldInit{
                field.name,
                cloneExpr(field.value.get(), type_mapping)
            });
        }
        return std::make_unique<StructLiteral>(
            struct_lit->getStructName(),
            std::move(new_fields)
        );
    }
    
    // ClosureExpr - Closure cloning is complex, simplified process for now
    if (auto* closure = dynamic_cast<ClosureExpr*>(expr)) {
        // Clone parameters
        std::vector<ClosureExpr::Param> new_params;
        for (const auto& param : closure->getParams()) {
            new_params.push_back(ClosureExpr::Param{
                param.name,
                substituteType(param.type, type_mapping),
                param.is_mutable
            });
        }
        
        // Clone body
        ExprPtr new_body = cloneExpr(closure->getBody(), type_mapping);
        Type* new_return_type = closure->getReturnType() ? 
                                substituteType(closure->getReturnType(), type_mapping) : nullptr;
        
        return std::make_unique<ClosureExpr>(
            std::move(new_params),
            new_return_type,
            std::move(new_body)
        );
    }
    
    // MatchExpr - match expression cloning
    if (auto* match = dynamic_cast<MatchExpr*>(expr)) {
        // Note: MatchExpr cloning requires deep copying arms (containing unique_ptr)
        // This implementation is complex, return nullptr for now
        // In practice, MatchExpr rarely appears in generic function bodies that need cloning
        // TODO: If needed, implement complete pattern and arm cloning
        return nullptr;
    }
    
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Visitor implementation - Statements
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void MonomorphizationPass::visit(ExprStmt* stmt) {
    if (stmt->getExpr()) {
        stmt->getExpr()->accept(this);
    }
}

void MonomorphizationPass::visit(VarDecl* decl) {
    if (decl->getInit()) {
        decl->getInit()->accept(this);
    }
}

void MonomorphizationPass::visit(DestructuringDecl* decl) {
    if (decl->getInit()) {
        decl->getInit()->accept(this);
    }
}

void MonomorphizationPass::visit(StructDestructuringDecl* decl) {
    if (decl->getInit()) {
        decl->getInit()->accept(this);
    }
}

void MonomorphizationPass::visit(FunctionDecl* decl) {
    // Already processed in collectGenericDefinitions
    // Traverse function body
    if (decl->getBody()) {
        decl->getBody()->accept(this);
    }
}

void MonomorphizationPass::visit(ReturnStmt* stmt) {
    if (stmt->getValue()) {
        stmt->getValue()->accept(this);
    }
}

void MonomorphizationPass::visit(IfStmt* stmt) {
    if (stmt->getCondition()) stmt->getCondition()->accept(this);
    if (stmt->getThenStmt()) stmt->getThenStmt()->accept(this);
    if (stmt->getElseStmt()) stmt->getElseStmt()->accept(this);
}

void MonomorphizationPass::visit(LoopStmt* stmt) {
    if (stmt->getBody()) stmt->getBody()->accept(this);
}

void MonomorphizationPass::visit(WhileStmt* stmt) {
    if (stmt->getCondition()) stmt->getCondition()->accept(this);
    if (stmt->getBody()) stmt->getBody()->accept(this);
}

void MonomorphizationPass::visit(ForStmt* stmt) {
    if (stmt->getIterator()) stmt->getIterator()->accept(this);
    if (stmt->getBody()) stmt->getBody()->accept(this);
}

void MonomorphizationPass::visit(BreakStmt*) {}
void MonomorphizationPass::visit(ContinueStmt*) {}

void MonomorphizationPass::visit(BlockStmt* stmt) {
    for (const auto& s : stmt->getStmts()) {
        s->accept(this);
    }
}

void MonomorphizationPass::visit(StructDecl*) {
    // Already processed in collectGenericDefinitions
}

void MonomorphizationPass::visit(EnumDecl*) {
    // Already processed in collectGenericDefinitions
}

void MonomorphizationPass::visit(InterfaceDecl*) {
    // Generic interfaces not processed for now
}

void MonomorphizationPass::visit(SupportDecl* decl) {
    // Traverse method implementations
    for (const auto& method : decl->getMethods()) {
        method->accept(this);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Visitor implementation - Expressions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void MonomorphizationPass::visit(IntLiteral*) {}
void MonomorphizationPass::visit(FloatLiteral*) {}
void MonomorphizationPass::visit(BoolLiteral*) {}
void MonomorphizationPass::visit(CharLiteral*) {}
void MonomorphizationPass::visit(StringLiteral*) {}
void MonomorphizationPass::visit(NoneLiteral*) {}

void MonomorphizationPass::visit(CastExpr* expr) {
    if (expr->getExpr()) {
        expr->getExpr()->accept(this);
    }
}

void MonomorphizationPass::visit(ClosureExpr* expr) {
    // Closure monomorphization: process generic usage in closure body
    if (expr->getBody()) {
        expr->getBody()->accept(this);
    }
}

void MonomorphizationPass::visit(ArrayLiteral* expr) {
    for (const auto& elem : expr->getElements()) {
        if (elem) elem->accept(this);
    }
}

void MonomorphizationPass::visit(TupleExpr* expr) {
    for (const auto& elem : expr->getElements()) {
        if (elem) elem->accept(this);
    }
}

void MonomorphizationPass::visit(StructLiteral* expr) {
    // Check if it's an instantiation of generic struct
    auto it = generic_structs_.find(expr->getStructName());
    if (it != generic_structs_.end()) {
        StructDecl* generic_decl = it->second;
        
        // infertypesparameter
        auto type_args = inferTypeArgsFromStructLiteral(expr, generic_decl);
        
        if (!type_args.empty()) {
            // Create instantiation request
            InstantiationRequest request{expr->getStructName(), type_args};
            
            // Get or generate monomorphized name
            std::string mono_name;
            
            // Check if already instantiated
            auto cache_it = instance_cache_.find(request);
            if (cache_it != instance_cache_.end()) {
                mono_name = cache_it->second;
            } else {
                // Generate monomorphized instance
                auto monomorphized = cloneAndSubstitute(generic_decl, type_args);
                
                // Get monomorphized name
                mono_name = generateMonomorphizedName(expr->getStructName(), type_args);
                
                // Record to cache
                instance_cache_[request] = mono_name;
                
                // Add to insertion list
                monomorphized_stmts_.push_back(std::move(monomorphized));
            }
            
            // [Key] Update StructLiteral's name to monomorphized name
            expr->setStructName(mono_name);
        }
    }
    
    // recursionprocessfieldvalue
    for (const auto& field : expr->getFields()) {
        if (field.value) {
            field.value->accept(this);
        }
    }
}

void MonomorphizationPass::visit(IdentifierExpr*) {}

void MonomorphizationPass::visit(BinaryExpr* expr) {
    if (expr->getLeft()) expr->getLeft()->accept(this);
    if (expr->getRight()) expr->getRight()->accept(this);
}

void MonomorphizationPass::visit(UnaryExpr* expr) {
    if (expr->getOperand()) expr->getOperand()->accept(this);
}

void MonomorphizationPass::visit(CallExpr* expr) {
    // Check if it's a generic enum constructor: Option::Some(42)
    if (auto* static_access = dynamic_cast<StaticAccessExpr*>(expr->getCallee())) {
        std::string enum_name = static_access->getTypeName();
        auto it = generic_enums_.find(enum_name);
        
        if (it != generic_enums_.end()) {
            EnumDecl* generic_enum = it->second;
            
            // First recursively process parameters
            for (const auto& arg : expr->getArgs()) {
                if (arg) arg->accept(this);
            }
            
            // fromparameterinferenumtypesparameter
            // Assume enum constructor's parameter directly corresponds to type parameter
            std::vector<Type*> type_args;
            if (!expr->getArgs().empty() && !generic_enum->getGenericParams().empty()) {
                // Infer type from first parameter
                auto first_arg = expr->getArgs()[0].get();
                Type* arg_type = inferTypeFromExpr(first_arg);
                if (arg_type) {
                    type_args.push_back(arg_type);
                }
            }
            
            if (!type_args.empty()) {
                // Create instantiation request
                InstantiationRequest request{enum_name, type_args};
                
                // Get or generate monomorphized name
                std::string mono_name;
                
                // Check cache first
                auto cache_it = instance_cache_.find(request);
                if (cache_it != instance_cache_.end()) {
                    mono_name = cache_it->second;
                } else {
                    // generationmonomorphizationenum
                    auto monomorphized = cloneAndSubstitute(generic_enum, type_args);
                    mono_name = generateMonomorphizedName(enum_name, type_args);
                    
                    // cache
                    instance_cache_[request] = mono_name;
                    
                    // Add to insertion list
                    monomorphized_stmts_.push_back(std::move(monomorphized));
                }
                
                // 【Key】UpdateStaticAccessExprof/thetypesnameis/asmonomorphizationname
                static_access->setTypeName(mono_name);
            }
        }
    }
    // Checkyesnois/asgenericfunctioncall（in/atprocessparameterbefore）
    else if (auto* callee_id = dynamic_cast<IdentifierExpr*>(expr->getCallee())) {
        auto it = generic_functions_.find(callee_id->getName());
        if (it != generic_functions_.end()) {
            FunctionDecl* generic_func = it->second;
            
            // First recursively process parameters（ensurenestedcallby/passive markerprocess）
            for (const auto& arg : expr->getArgs()) {
                if (arg) arg->accept(this);
            }
            
            // fromparameterinfertypesparameter
            auto type_args = inferTypeArgsFromCallExpr(expr, generic_func);
            
            if (!type_args.empty()) {
                // Create instantiation request
                InstantiationRequest request{callee_id->getName(), type_args};
                
                // Get or generate monomorphized name
                std::string mono_name;
                
                // Check if already instantiated
                auto cache_it = instance_cache_.find(request);
                if (cache_it != instance_cache_.end()) {
                    mono_name = cache_it->second;
                } else {
                    // Generate monomorphized instance
                    auto monomorphized = cloneAndSubstitute(generic_func, type_args);
                    
                    // Get monomorphized name
                    mono_name = generateMonomorphizedName(callee_id->getName(), type_args);
                    
                    // Record to cache
                    instance_cache_[request] = mono_name;
                    
                    // Add to insertion list
                    monomorphized_stmts_.push_back(std::move(monomorphized));
                }
                
                // 【Key】UpdateCallExprof/thecalleenameis/asmonomorphizationname
                callee_id->setName(mono_name);
            }
            
            return;  // alreadyprocess，directlyreturn
        }
    }
    
    // notgenericfunctioncall，positivenormallyprocess
    if (expr->getCallee()) expr->getCallee()->accept(this);
    for (const auto& arg : expr->getArgs()) {
        if (arg) arg->accept(this);
    }
}

void MonomorphizationPass::visit(MemberExpr* expr) {
    if (expr->getObject()) expr->getObject()->accept(this);
}

void MonomorphizationPass::visit(StaticAccessExpr* expr) {
    // staticvisit: Type::Variant
    // Checkyesnois/asgenericenumof/theconstructor
    auto it = generic_enums_.find(expr->getTypeName());
    if (it != generic_enums_.end()) {
        // ━━━ processgenericenuminstantiation ━━━
        EnumDecl* generic_enum = it->second;
        
        // 1. tryfromexpressiontypesinfertypesparameter
        // note：heresimplifyprocess，actualneedfromcontextorCallExprof/theparameterinfer
        std::vector<Type*> type_args;
        
        // 2. ifexpressionalreadyhastypesannotation（byTypeCheckerset），tryextract
        Type* expr_type = expr->getType();
        if (expr_type && expr_type->isEnum()) {
            auto* enum_type = static_cast<EnumType*>(expr_type);
            // Checkyesnoyesgenericinstantiation
            if (!enum_type->getName().empty()) {
                // simplifyimplementation：fromtypesnameinfer
                // actualshouldhas getTypeArgs() method
            }
        }
        
        // 3. ifnoway/cannotinfer，temporarilytime/whenskip
        // actualof/thegenericenuminstantiationwillin/atCallExprmiddle/centerprocess（like/such as Option::Some(42)）
        // note：instantiateGenericEnum in/atcompleteimplementationmiddle/centerwillcall，heresimplifyprocess
        if (!type_args.empty()) {
            // instantiateGenericEnum(generic_enum, type_args);
            // simplifyimplementation：typesinferbyTypeCheckercomplete
        }
    }
}

void MonomorphizationPass::visit(IndexExpr* expr) {
    if (expr->getObject()) expr->getObject()->accept(this);
    if (expr->getIndex()) expr->getIndex()->accept(this);
}

void MonomorphizationPass::visit(BlockExpr* expr) {
    for (const auto& stmt : expr->getStmts()) {
        stmt->accept(this);
    }
}

void MonomorphizationPass::visit(IfExpr* expr) {
    if (expr->getCondition()) expr->getCondition()->accept(this);
    if (expr->getThenExpr()) expr->getThenExpr()->accept(this);
    if (expr->getElseExpr()) expr->getElseExpr()->accept(this);
}

void MonomorphizationPass::visit(RangeExpr* expr) {
    if (expr->getStart()) expr->getStart()->accept(this);
    if (expr->getEnd()) expr->getEnd()->accept(this);
}

void MonomorphizationPass::visit(SelfExpr*) {}
void MonomorphizationPass::visit(TryExpr* expr) {
    if (expr->getExpr()) expr->getExpr()->accept(this);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Visitorimplementation - Patterns
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void MonomorphizationPass::visit(MatchExpr* expr) {
    if (expr->getScrutinee()) expr->getScrutinee()->accept(this);
    
    // ━━━ processmatch arms ━━━
    for (const auto& arm : expr->getArms()) {
        // traversepattern（possiblycontainsgenericstructbody/struct/enumof/thedestruct）
        if (arm.pattern) {
            arm.pattern->accept(this);
        }
        
        // traversearmexpression（possiblycontainsgeneric usage）
        if (arm.expression) {
            arm.expression->accept(this);
        }
    }
}

void MonomorphizationPass::visit(LiteralPattern*) {}
void MonomorphizationPass::visit(VariablePattern*) {}
void MonomorphizationPass::visit(WildcardPattern*) {}
void MonomorphizationPass::visit(TuplePattern*) {}
void MonomorphizationPass::visit(EnumPattern*) {}

void MonomorphizationPass::visit(StructPattern* pattern) {
    // structbody/structpattern：traversefieldpattern
    for (const auto& field : pattern->getFields()) {
        if (field.pattern) {
            field.pattern->accept(this);
        }
    }
}

void MonomorphizationPass::visit(ArrayPattern* pattern) {
    // arraypattern：traverseelementpattern
    for (const auto& elem : pattern->getElements()) {
        if (elem) {
            elem->accept(this);
        }
    }
}

void MonomorphizationPass::visit(SlicePattern* pattern) {
    // Slicepattern：traversefront/beforefix、back/afterfixandrest
    for (const auto& prefix : pattern->getPrefix()) {
        if (prefix) {
            prefix->accept(this);
        }
    }
    for (const auto& suffix : pattern->getSuffix()) {
        if (suffix) {
            suffix->accept(this);
        }
    }
    if (pattern->hasRest()) {
        pattern->getRest()->accept(this);
    }
}

void MonomorphizationPass::visit(RangePattern* pattern) {
    // Rangepattern：traversestartstart/beginandendpattern
    if (pattern->getStart()) {
        pattern->getStart()->accept(this);
    }
    if (pattern->getEnd()) {
        pattern->getEnd()->accept(this);
    }
}

void MonomorphizationPass::visit(OrPattern* pattern) {
    // ORpattern：traverseAllbranch
    for (const auto& alt : pattern->getAlternatives()) {
        if (alt) {
            alt->accept(this);
        }
    }
}

} // namespace pawc

