//===--- monomorphization_pass.cpp - Monomorphization Pass ------*- C++ -*-===//

#include "monomorphization_pass.h"
#include "backend/codegen/generic/mangling.h"  // 使用新的Mangling模块
#include "pass/pass_context.h"
#include "middleend/types/type_system.h"
#include "frontend/parser/ast/pattern.h"
#include <iostream>

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pass主入口
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PassResult MonomorphizationPass::runImpl(PassContext* context) {
    context_ = context;
    
    auto* ast = context->getAST();
    if (!ast || ast->empty()) {
        return PassResult{true, "No AST for monomorphization"};
    }
    
    // 第一遍：收集所有泛型定义
    collectGenericDefinitions(*ast);
    
    // 第二遍：遍历AST，找到泛型使用点并生成单态化实例
    for (const auto& stmt : *ast) {
        stmt->accept(this);
    }
    
    // 【新增】将单态化实例注册（TypeChecker会处理）
    // 注意：单态化的函数和类型会在后续TypeChecker/CodeGen中被处理
    // 因为它们已经被添加到monomorphized_stmts_，后续会被遍历
    
    // 记录生成的实例数到context
    if (!monomorphized_stmts_.empty()) {
        context->cacheAnalysisResult("monomorphized_instances", 
                                     static_cast<int>(monomorphized_stmts_.size()));
    }
    
    // 调试信息
    std::string result_msg = "Monomorphization completed, generated " + 
                             std::to_string(monomorphized_stmts_.size()) + " instances";
    if (!instance_cache_.empty()) {
        result_msg += " (cache size: " + std::to_string(instance_cache_.size()) + ")";
    }
    
    return PassResult{true, result_msg};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 收集泛型定义
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
// 辅助函数
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::vector<Type*> MonomorphizationPass::inferTypeArgsFromStructLiteral(
    StructLiteral* lit, StructDecl* generic_decl) {
    
    // 从字段值的类型推导泛型参数
    // 例如: Box { value: 42 } => T = i32
    
    std::vector<Type*> type_args;
    const auto& generic_params = generic_decl->getGenericParams();
    const auto& generic_fields = generic_decl->getFields();
    
    if (generic_params.empty()) {
        return type_args;  // 非泛型结构体
    }
    
    // 建立类型参数映射
    std::map<std::string, Type*> type_param_map;
    
    // 遍历字段，尝试从字面量推导类型
    for (const auto& field_init : lit->getFields()) {
        // 找到对应的字段声明
        for (const auto& field_decl : generic_fields) {
            if (field_decl.first == field_init.name) {
                // 从字段值推导类型
                Type* inferred_type = inferTypeFromExpr(field_init.value.get());
                
                if (inferred_type && field_decl.second) {
                    // 如果字段类型是泛型参数，记录推导结果
                    std::string field_type_name = field_decl.second->toString();
                    
                    if (type_param_map.find(field_type_name) == type_param_map.end()) {
                        type_param_map[field_type_name] = inferred_type;
                    }
                }
                break;
            }
        }
    }
    
    // 按泛型参数顺序构建type_args
    for (const auto& param : generic_params) {
        auto it = type_param_map.find(param.name);
        if (it != type_param_map.end()) {
            type_args.push_back(it->second);
        } else {
            // 如果某个泛型参数未能推导，返回空（推导失败）
            // TODO: 可以考虑默认类型或报错
            return std::vector<Type*>{};
        }
    }
    
    return type_args;
}

std::string MonomorphizationPass::generateMonomorphizedName(
    const std::string& base_name, const std::vector<Type*>& type_args) {
    
    // 生成单态化名称: Box<i32> -> Box_i32
    std::string name = base_name;
    for (auto* type : type_args) {
        name += "_" + type->toString();
    }
    return name;
}

Type* MonomorphizationPass::substituteType(
    Type* type, const std::map<std::string, Type*>& type_mapping) {
    
    if (!type) return nullptr;
    
    // 如果是泛型参数类型（通过名称匹配），替换为具体类型
    auto it = type_mapping.find(type->toString());
    if (it != type_mapping.end()) {
        return it->second;
    }
    
    // 处理复合类型（递归替换）
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
            auto* result_type = static_cast<ResultType*>(type);
            Type* new_ok = substituteType(result_type->getOkType(), type_mapping);
            if (new_ok != result_type->getOkType()) {
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
            // 对于StructType，检查是否是泛型结构体的未单态化版本
            // 例如：Box<T> 需要递归替换T
            auto* struct_type = static_cast<StructType*>(type);
            std::vector<StructType::Field> new_fields;
            bool changed = false;
            
            for (const auto& field : struct_type->getFields()) {
                Type* new_field_type = substituteType(field.second, type_mapping);
                new_fields.push_back({field.first, new_field_type});
                if (new_field_type != field.second) changed = true;
            }
            
            if (changed) {
                // 创建新的StructType
                return new StructType(struct_type->getName(), new_fields);
            }
            break;
        }
        
        default:
            // 基础类型，不需要替换
            break;
    }
    
    return type;
}

std::vector<Type*> MonomorphizationPass::inferTypeArgsFromCallExpr(
    CallExpr* call, FunctionDecl* generic_func) {
    
    // 从函数调用参数推导类型参数
    // 例如: identity(42) => T = i32
    
    std::vector<Type*> type_args;
    const auto& generic_params = generic_func->getGenericParams();
    const auto& func_params = generic_func->getParams();
    const auto& call_args = call->getArgs();
    
    if (generic_params.empty()) {
        return type_args;  // 非泛型函数
    }
    
    // 建立类型参数映射
    std::map<std::string, Type*> type_param_map;
    
    // 遍历参数，推导类型
    for (size_t i = 0; i < func_params.size() && i < call_args.size(); ++i) {
        // 从调用参数推导类型
        Type* arg_type = inferTypeFromExpr(call_args[i].get());
        
        if (arg_type && func_params[i].type) {
            // 如果函数参数类型是泛型参数，记录推导结果
            std::string param_type_name = func_params[i].type->toString();
            if (type_param_map.find(param_type_name) == type_param_map.end()) {
                type_param_map[param_type_name] = arg_type;
            }
        }
    }
    
    // 按泛型参数顺序构建type_args
    for (const auto& param : generic_params) {
        auto it = type_param_map.find(param.name);
        if (it != type_param_map.end()) {
            type_args.push_back(it->second);
        } else {
            // 推导失败
            return std::vector<Type*>{};
        }
    }
    
    return type_args;
}

Type* MonomorphizationPass::inferTypeFromExpr(Expr* expr) {
    // 从表达式字面量推导类型
    if (!expr) return nullptr;
    
    auto* type_system = context_->getTypeSystem();
    
    // 整数字面量
    if (auto* int_lit = dynamic_cast<IntLiteral*>(expr)) {
        return type_system->getI32Type();  // 默认i32
    }
    
    // 浮点字面量
    if (auto* float_lit = dynamic_cast<FloatLiteral*>(expr)) {
        return type_system->getF64Type();  // 默认f64
    }
    
    // 布尔字面量
    if (auto* bool_lit = dynamic_cast<BoolLiteral*>(expr)) {
        return type_system->getBoolType();
    }
    
    // 字符字面量
    if (auto* char_lit = dynamic_cast<CharLiteral*>(expr)) {
        return type_system->getCharType();
    }
    
    // 字符串字面量
    if (auto* str_lit = dynamic_cast<StringLiteral*>(expr)) {
        return type_system->getStringType();
    }
    
    // 【新增】StructLiteral - 处理嵌套泛型
    if (auto* struct_lit = dynamic_cast<StructLiteral*>(expr)) {
        // 先确保内层的StructLiteral被处理
        struct_lit->accept(this);
        
        // 检查是否是泛型结构体
        auto it = generic_structs_.find(struct_lit->getStructName());
        if (it != generic_structs_.end()) {
            // 推导类型参数
            auto type_args = inferTypeArgsFromStructLiteral(struct_lit, it->second);
            if (!type_args.empty()) {
                // 返回单态化后的StructType
                std::string mono_name = generateMonomorphizedName(
                    struct_lit->getStructName(), type_args);
                
                // 查找已注册的单态化类型
                return type_system->lookupType(mono_name);
            }
        }
        
        // 如果已经有类型（被处理过），使用它
        if (expr->getType()) {
            return expr->getType();
        }
        
        // 查找非泛型结构体类型
        return type_system->lookupType(struct_lit->getStructName());
    }
    
    // 如果已经有类型（被TypeChecker设置），使用它
    if (expr->getType()) {
        return expr->getType();
    }
    
    return nullptr;
}

StmtPtr MonomorphizationPass::cloneAndSubstitute(
    StructDecl* generic_decl, const std::vector<Type*>& type_args) {
    
    // 构建类型映射: T -> i32, U -> string等
    std::map<std::string, Type*> type_mapping;
    const auto& generic_params = generic_decl->getGenericParams();
    
    for (size_t i = 0; i < generic_params.size() && i < type_args.size(); ++i) {
        type_mapping[generic_params[i].name] = type_args[i];
    }
    
    // 生成新的名称
    std::string new_name = generateMonomorphizedName(generic_decl->getName(), type_args);
    
    // 克隆字段并替换类型
    std::vector<StructDecl::Field> new_fields;
    for (const auto& field : generic_decl->getFields()) {
        Type* new_type = substituteType(field.second, type_mapping);
        new_fields.push_back({field.first, new_type});
    }
    
    // 创建StructType并注册到TypeSystem
    auto* type_system = context_->getTypeSystem();
    std::vector<StructType::Field> struct_type_fields;
    for (const auto& field : new_fields) {
        struct_type_fields.push_back({field.first, field.second});
    }
    
    auto* struct_type = new StructType(new_name, struct_type_fields);
    type_system->registerStruct(struct_type);
    
    // 创建新的StructDecl（不带泛型参数）
    return std::make_unique<StructDecl>(
        new_name, 
        std::vector<GenericParam>{},  // 单态化后不再有泛型参数
        std::move(new_fields)
    );
}

StmtPtr MonomorphizationPass::cloneAndSubstitute(
    EnumDecl* generic_decl, const std::vector<Type*>& type_args) {
    
    // 构建类型映射
    std::map<std::string, Type*> type_mapping;
    const auto& generic_params = generic_decl->getGenericParams();
    
    for (size_t i = 0; i < generic_params.size() && i < type_args.size(); ++i) {
        type_mapping[generic_params[i].name] = type_args[i];
    }
    
    // 生成新的名称
    std::string new_name = generateMonomorphizedName(generic_decl->getName(), type_args);
    
    // 克隆变体并替换类型
    std::vector<EnumVariant> new_variants;
    for (const auto& variant : generic_decl->getVariants()) {
        std::vector<Type*> new_data_types;
        for (Type* type : variant.data_types) {
            new_data_types.push_back(substituteType(type, type_mapping));
        }
        new_variants.push_back(EnumVariant(variant.name, new_data_types));
    }
    
    // 创建新的EnumDecl
    return std::make_unique<EnumDecl>(
        new_name,
        std::vector<GenericParam>{},  // 单态化后不再有泛型参数
        std::move(new_variants)
    );
}

StmtPtr MonomorphizationPass::cloneAndSubstitute(
    FunctionDecl* generic_decl, const std::vector<Type*>& type_args) {
    
    // 构建类型映射
    std::map<std::string, Type*> type_mapping;
    const auto& generic_params = generic_decl->getGenericParams();
    
    for (size_t i = 0; i < generic_params.size() && i < type_args.size(); ++i) {
        type_mapping[generic_params[i].name] = type_args[i];
    }
    
    // 生成新的名称
    std::string new_name = generateMonomorphizedName(generic_decl->getName(), type_args);
    
    // 克隆参数并替换类型
    std::vector<FunctionDecl::Param> new_params;
    for (const auto& param : generic_decl->getParams()) {
        Type* new_param_type = substituteType(param.type, type_mapping);
        new_params.push_back({param.name, new_param_type, param.is_mutable});
    }
    
    // 替换返回类型
    Type* new_return_type = substituteType(generic_decl->getReturnType(), type_mapping);
    
    // 克隆函数体（深度克隆AST）
    StmtPtr new_body = cloneStmt(generic_decl->getBody(), type_mapping);
    
    // 创建新的FunctionDecl
    return std::make_unique<FunctionDecl>(
        new_name,
        std::vector<GenericParam>{},  // 单态化后不再有泛型参数
        std::move(new_params),
        new_return_type,
        std::move(new_body)
    );
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// AST克隆（用于函数体克隆）
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
    
    // ━━━ 其他语句类型 ━━━
    
    // StructDecl（非泛型或已实例化的）
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
    
    // EnumDecl（非泛型或已实例化的）
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
    
    // InterfaceDecl 和 SupportDecl 暂不需要克隆（它们不会被内联到函数体中）
    
    return nullptr;
}

ExprPtr MonomorphizationPass::cloneExpr(Expr* expr, const std::map<std::string, Type*>& type_mapping) {
    if (!expr) return nullptr;
    
    // 字面量
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
    
    // ━━━ 其他表达式类型 ━━━
    
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
        // 复制泛型参数（如果有）
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
    
    // StructLiteral（已在上面处理过，这里作为fallback）
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
    
    // ClosureExpr - 闭包克隆比较复杂，暂时简化处理
    if (auto* closure = dynamic_cast<ClosureExpr*>(expr)) {
        // 克隆参数
        std::vector<ClosureExpr::Param> new_params;
        for (const auto& param : closure->getParams()) {
            new_params.push_back(ClosureExpr::Param{
                param.name,
                substituteType(param.type, type_mapping),
                param.is_mutable
            });
        }
        
        // 克隆body
        ExprPtr new_body = cloneExpr(closure->getBody(), type_mapping);
        Type* new_return_type = closure->getReturnType() ? 
                                substituteType(closure->getReturnType(), type_mapping) : nullptr;
        
        return std::make_unique<ClosureExpr>(
            std::move(new_params),
            new_return_type,
            std::move(new_body)
        );
    }
    
    // MatchExpr - match表达式克隆
    if (auto* match = dynamic_cast<MatchExpr*>(expr)) {
        // 注：MatchExpr 克隆需要深度复制 arms（包含 unique_ptr）
        // 这个实现比较复杂，暂时返回 nullptr
        // 实际使用中，MatchExpr 通常不会出现在需要克隆的泛型函数体中
        // TODO: 如果需要支持，需要实现完整的 pattern 和 arm 克隆
        return nullptr;
    }
    
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Visitor实现 - Statements
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
    // 已在collectGenericDefinitions中处理
    // 遍历函数体
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
    // 已在collectGenericDefinitions中处理
}

void MonomorphizationPass::visit(EnumDecl*) {
    // 已在collectGenericDefinitions中处理
}

void MonomorphizationPass::visit(InterfaceDecl*) {
    // 泛型接口暂时不处理
}

void MonomorphizationPass::visit(SupportDecl* decl) {
    // 遍历方法实现
    for (const auto& method : decl->getMethods()) {
        method->accept(this);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Visitor实现 - Expressions
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
    // 闭包单态化：处理闭包体中的泛型使用
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
    // 检查是否为泛型结构体的实例化
    auto it = generic_structs_.find(expr->getStructName());
    if (it != generic_structs_.end()) {
        StructDecl* generic_decl = it->second;
        
        // 推导类型参数
        auto type_args = inferTypeArgsFromStructLiteral(expr, generic_decl);
        
        if (!type_args.empty()) {
            // 创建实例化请求
            InstantiationRequest request{expr->getStructName(), type_args};
            
            // 获取或生成单态化名称
            std::string mono_name;
            
            // 检查是否已经实例化
            auto cache_it = instance_cache_.find(request);
            if (cache_it != instance_cache_.end()) {
                mono_name = cache_it->second;
            } else {
                // 生成单态化实例
                auto monomorphized = cloneAndSubstitute(generic_decl, type_args);
                
                // 获取单态化名称
                mono_name = generateMonomorphizedName(expr->getStructName(), type_args);
                
                // 记录到缓存
                instance_cache_[request] = mono_name;
                
                // 添加到待插入列表
                monomorphized_stmts_.push_back(std::move(monomorphized));
            }
            
            // 【关键】更新StructLiteral的名称为单态化名称
            expr->setStructName(mono_name);
        }
    }
    
    // 递归处理字段值
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
    // 检查是否为泛型枚举构造器: Option::Some(42)
    if (auto* static_access = dynamic_cast<StaticAccessExpr*>(expr->getCallee())) {
        std::string enum_name = static_access->getTypeName();
        auto it = generic_enums_.find(enum_name);
        
        if (it != generic_enums_.end()) {
            EnumDecl* generic_enum = it->second;
            
            // 先递归处理参数
            for (const auto& arg : expr->getArgs()) {
                if (arg) arg->accept(this);
            }
            
            // 从参数推导枚举类型参数
            // 假设枚举构造器的参数直接对应类型参数
            std::vector<Type*> type_args;
            if (!expr->getArgs().empty() && !generic_enum->getGenericParams().empty()) {
                // 从第一个参数推导类型
                auto first_arg = expr->getArgs()[0].get();
                Type* arg_type = inferTypeFromExpr(first_arg);
                if (arg_type) {
                    type_args.push_back(arg_type);
                }
            }
            
            if (!type_args.empty()) {
                // 创建实例化请求
                InstantiationRequest request{enum_name, type_args};
                
                // 获取或生成单态化名称
                std::string mono_name;
                
                // 检查缓存
                auto cache_it = instance_cache_.find(request);
                if (cache_it != instance_cache_.end()) {
                    mono_name = cache_it->second;
                } else {
                    // 生成单态化枚举
                    auto monomorphized = cloneAndSubstitute(generic_enum, type_args);
                    mono_name = generateMonomorphizedName(enum_name, type_args);
                    
                    // 缓存
                    instance_cache_[request] = mono_name;
                    
                    // 添加到待插入列表
                    monomorphized_stmts_.push_back(std::move(monomorphized));
                }
                
                // 【关键】更新StaticAccessExpr的类型名为单态化名称
                static_access->setTypeName(mono_name);
            }
        }
    }
    // 检查是否为泛型函数调用（在处理参数之前）
    else if (auto* callee_id = dynamic_cast<IdentifierExpr*>(expr->getCallee())) {
        auto it = generic_functions_.find(callee_id->getName());
        if (it != generic_functions_.end()) {
            FunctionDecl* generic_func = it->second;
            
            // 先递归处理参数（确保嵌套调用被处理）
            for (const auto& arg : expr->getArgs()) {
                if (arg) arg->accept(this);
            }
            
            // 从参数推导类型参数
            auto type_args = inferTypeArgsFromCallExpr(expr, generic_func);
            
            if (!type_args.empty()) {
                // 创建实例化请求
                InstantiationRequest request{callee_id->getName(), type_args};
                
                // 获取或生成单态化名称
                std::string mono_name;
                
                // 检查是否已经实例化
                auto cache_it = instance_cache_.find(request);
                if (cache_it != instance_cache_.end()) {
                    mono_name = cache_it->second;
                } else {
                    // 生成单态化实例
                    auto monomorphized = cloneAndSubstitute(generic_func, type_args);
                    
                    // 获取单态化名称
                    mono_name = generateMonomorphizedName(callee_id->getName(), type_args);
                    
                    // 记录到缓存
                    instance_cache_[request] = mono_name;
                    
                    // 添加到待插入列表
                    monomorphized_stmts_.push_back(std::move(monomorphized));
                }
                
                // 【关键】更新CallExpr的callee名称为单态化名称
                callee_id->setName(mono_name);
            }
            
            return;  // 已处理，直接返回
        }
    }
    
    // 非泛型函数调用，正常处理
    if (expr->getCallee()) expr->getCallee()->accept(this);
    for (const auto& arg : expr->getArgs()) {
        if (arg) arg->accept(this);
    }
}

void MonomorphizationPass::visit(MemberExpr* expr) {
    if (expr->getObject()) expr->getObject()->accept(this);
}

void MonomorphizationPass::visit(StaticAccessExpr* expr) {
    // 静态访问: Type::Variant
    // 检查是否为泛型枚举的构造器
    auto it = generic_enums_.find(expr->getTypeName());
    if (it != generic_enums_.end()) {
        // ━━━ 处理泛型枚举实例化 ━━━
        EnumDecl* generic_enum = it->second;
        
        // 1. 尝试从表达式类型推导类型参数
        // 注：这里简化处理，实际需要从上下文或CallExpr的参数推导
        std::vector<Type*> type_args;
        
        // 2. 如果表达式已有类型注解（由TypeChecker设置），尝试提取
        Type* expr_type = expr->getType();
        if (expr_type && expr_type->isEnum()) {
            auto* enum_type = static_cast<EnumType*>(expr_type);
            // 检查是否是泛型实例化
            if (!enum_type->getName().empty()) {
                // 简化实现：从类型名推导
                // 实际应该有 getTypeArgs() 方法
            }
        }
        
        // 3. 如果无法推导，暂时跳过
        // 实际的泛型枚举实例化会在CallExpr中处理（如 Option::Some(42)）
        // 注：instantiateGenericEnum 在完整实现中会调用，这里简化处理
        if (!type_args.empty()) {
            // instantiateGenericEnum(generic_enum, type_args);
            // 简化实现：类型推导由TypeChecker完成
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
// Visitor实现 - Patterns
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void MonomorphizationPass::visit(MatchExpr* expr) {
    if (expr->getScrutinee()) expr->getScrutinee()->accept(this);
    
    // ━━━ 处理match arms ━━━
    for (const auto& arm : expr->getArms()) {
        // 遍历模式（可能包含泛型结构体/枚举的解构）
        if (arm.pattern) {
            arm.pattern->accept(this);
        }
        
        // 遍历arm表达式（可能包含泛型使用）
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
    // 结构体模式：遍历字段模式
    for (const auto& field : pattern->getFields()) {
        if (field.pattern) {
            field.pattern->accept(this);
        }
    }
}

void MonomorphizationPass::visit(ArrayPattern* pattern) {
    // 数组模式：遍历元素模式
    for (const auto& elem : pattern->getElements()) {
        if (elem) {
            elem->accept(this);
        }
    }
}

void MonomorphizationPass::visit(SlicePattern* pattern) {
    // Slice模式：遍历前缀、后缀和rest
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
    // Range模式：遍历起始和结束模式
    if (pattern->getStart()) {
        pattern->getStart()->accept(this);
    }
    if (pattern->getEnd()) {
        pattern->getEnd()->accept(this);
    }
}

void MonomorphizationPass::visit(OrPattern* pattern) {
    // OR模式：遍历所有分支
    for (const auto& alt : pattern->getAlternatives()) {
        if (alt) {
            alt->accept(this);
        }
    }
}

} // namespace pawc

