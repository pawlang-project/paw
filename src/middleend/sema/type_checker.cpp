//===--- type_checker.cpp - Type Checker Implementation ----------*- C++ -*-===//

#include "type_checker.h"
#include "capture_analyzer.h"
#include "frontend/parser/ast/pattern.h"
#include <set>

namespace pawc {

TypeChecker::TypeChecker(TypeSystem* type_system, SymbolTable* symbol_table,
                         DiagnosticEngine* diag_engine)
    : types_(type_system), symbols_(symbol_table), diag_(diag_engine) {}

void TypeChecker::check(const std::vector<StmtPtr>& stmts) {
    // 两遍遍历：
    // 第一遍：注册所有类型和函数声明（不检查函数体）
    // 第二遍：检查所有函数体和表达式
    
    // 第一遍：收集声明
    for (const auto& stmt : stmts) {
        if (auto* func = dynamic_cast<FunctionDecl*>(stmt.get())) {
            // 仅注册函数，不检查函数体
            std::vector<Type*> param_types;
            for (const auto& param : func->getParams()) {
                param_types.push_back(param.type);
            }
            FunctionType* func_type = types_->getFunctionType(param_types, func->getReturnType());
            symbols_->defineFunction(func->getName(), func_type);
        }
        else if (auto* struct_decl = dynamic_cast<StructDecl*>(stmt.get())) {
            // 结构体已在单态化时注册，跳过
        }
        else if (auto* enum_decl = dynamic_cast<EnumDecl*>(stmt.get())) {
            // 枚举声明
            stmt->accept(this);
        }
        else if (auto* interface_decl = dynamic_cast<InterfaceDecl*>(stmt.get())) {
            // 接口声明
            stmt->accept(this);
        }
    }
    
    // 第二遍：完整类型检查
    for (const auto& stmt : stmts) {
        stmt->accept(this);
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 表达式类型检查
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::visit(IntLiteral* node) {
    // 如果有expected type，使用它；否则默认i32
    if (expected_type_ && expected_type_->isInteger()) {
        node->setType(expected_type_);
    } else {
        node->setType(types_->getI32Type());  // 默认i32
    }
}

void TypeChecker::visit(FloatLiteral* node) {
    // 如果有expected type，使用它；否则默认f64
    if (expected_type_ && expected_type_->isFloat()) {
        node->setType(expected_type_);
    } else {
        node->setType(types_->getF64Type());  // 默认f64
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

void TypeChecker::visit(NullLiteral* node) {
    // null字面量的类型需要从上下文推导
    // 如果在Optional<T>的上下文中，类型应该是Optional<T>
    // 否则默认为Optional<void>
    
    // TODO: 从上下文推导（当前简化为Optional<void>）
    // 实际应该在ReturnStmt或赋值时根据期望类型设置
    node->setType(types_->getOptionalType(types_->getVoidType()));
}

void TypeChecker::visit(CastExpr* node) {
    // as类型转换: expr as TargetType
    node->getExpr()->accept(this);
    
    Type* source_type = node->getExpr()->getType();
    Type* target_type = node->getTargetType();
    
    if (!source_type || !target_type) {
        node->setType(types_->getVoidType());
        return;
    }
    
    // 验证类型转换的合法性
    // 根据文档，支持：数值类型之间、数组到切片
    bool is_valid = false;
    
    // 1. 数值类型之间的转换
    if (source_type->isNumeric() && target_type->isNumeric()) {
        is_valid = true;
    }
    // 2. 数组到切片
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
    // self表达式：类型由当前support块的目标类型决定
    // TODO: 从当前上下文获取self类型
    // 暂时设置为nullptr，需要在SupportDecl处理时设置上下文
    node->setType(nullptr);
}

void TypeChecker::visit(IdentifierExpr* node) {
    const std::string& name = node->getName();
    
    // 特殊处理: ok/err builtin（作为CallExpr的callee时不需要类型）
    if (name == "ok" || name == "err") {
        // 类型由包含它的CallExpr确定
        node->setType(nullptr);  // 占位符
        return;
    }
    
    Symbol* sym = symbols_->lookup(name);
    if (!sym) {
        diag_->reportError("Undefined identifier: " + name,
                          node->getLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    if (sym->getKind() == Symbol::Kind::Variable) {
        auto* var_sym = static_cast<VariableSymbol*>(sym);
        node->setType(var_sym->getType());
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
    
    // 比较运算符（返回bool）
    if (op == TokenType::EQ_EQ || op == TokenType::NOT_EQ ||
        op == TokenType::LT || op == TokenType::LESS_EQ ||
        op == TokenType::GT || op == TokenType::GREATER_EQ) {
        
        if (!types_->equals(left_type, right_type)) {
            diag_->reportError("Type mismatch in comparison", node->getLocation());
        }
        node->setType(types_->getBoolType());
        return;
    }
    
    // 逻辑运算符（要求bool，返回bool）
    if (op == TokenType::AND_AND || op == TokenType::OR_OR) {
        if (!left_type->isBool() || !right_type->isBool()) {
            diag_->reportError("Logical operators require bool operands",
                              node->getLocation());
        }
        node->setType(types_->getBoolType());
        return;
    }
    
    // 算术运算符（返回操作数类型）
    if (op == TokenType::PLUS || op == TokenType::MINUS ||
        op == TokenType::STAR || op == TokenType::SLASH ||
        op == TokenType::PERCENT) {
        
        // 检查类型兼容性
        if (!types_->equals(left_type, right_type)) {
            diag_->reportError("Type mismatch in arithmetic expression",
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // 检查是否为数值类型
        if (!left_type->isNumeric()) {
            diag_->reportError("Arithmetic operators require numeric types",
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        node->setType(left_type);
        return;
    }
    
    // 位运算符（整数类型）
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
    
    // 默认：要求类型相等，返回操作数类型
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
    node->getCallee()->accept(this);
    
    // 收集参数类型
    std::vector<Type*> arg_types;
    for (const auto& arg : node->getArgs()) {
        arg->accept(this);
        arg_types.push_back(arg->getType());
    }
    
    // Case 1: 闭包调用 - callee是一个FunctionType的变量
    Type* callee_type = node->getCallee()->getType();
    if (callee_type && callee_type->getKind() == Type::Kind::Function) {
        auto* func_type = static_cast<FunctionType*>(callee_type);
        
        // 验证参数数量
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
        
        // 验证参数类型匹配
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
        
        // 设置返回类型
        node->setType(func_type->getReturnType());
        return;
    }
    
    // Case 2: 普通函数调用 - callee是IdentifierExpr
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node->getCallee())) {
        const std::string& func_name = ident->getName();
        
        // 特殊处理: ok(value) - Result构造器
        if (func_name == "ok" && arg_types.size() == 1) {
            // ok(value) 返回 Result<T>
            Type* value_type = arg_types[0];
            node->setType(types_->getResultType(value_type));
            return;
        }
        
        // 特殊处理: err(message) - Result构造器
        if (func_name == "err" && arg_types.size() == 1) {
            // err(message) 返回 Result<T>，T从当前函数返回类型推导
            if (current_function_return_type_ &&
                current_function_return_type_->getKind() == Type::Kind::Result) {
                // 使用函数的返回类型
                node->setType(current_function_return_type_);
                return;
            }
            
            // 如果无法推导，默认为Result<void>
            node->setType(types_->getResultType(types_->getVoidType()));
            return;
        }
        
        // 特殊处理: len(array/slice) - 支持泛型容器
        if (func_name == "len" && arg_types.size() == 1) {
            Type* arg_type = arg_types[0];
            if (arg_type && (arg_type->isString() || arg_type->isArray() || 
                             arg_type->getKind() == Type::Kind::Slice)) {
                // len返回u64
                node->setType(types_->getU64Type());
                return;
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
    // 静态访问: Type::Variant 或 Type::method
    // 查找类型
    Type* type = types_->lookupType(node->getTypeName());
    if (!type) {
        diag_->reportError("Unknown type: " + node->getTypeName(), node->getLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    // TODO: 处理枚举构造器 Option::Some
    // 暂时设置为未知类型
    node->setType(types_->getVoidType());
}

void TypeChecker::visit(MemberExpr* node) {
    node->getObject()->accept(this);
    
    Type* obj_type = node->getObject()->getType();
    if (!obj_type) {
        node->setType(types_->getVoidType());
        return;
    }
    
    const std::string& member = node->getMember();
    
    // 检查是否是元组字段访问（成员名是数字）
    bool is_tuple_access = !member.empty() && std::isdigit(member[0]);
    
    if (is_tuple_access && obj_type->isTuple()) {
        // 元组字段访问: tuple.0, tuple.1 等
        auto* tuple_type = static_cast<TupleType*>(obj_type);
        
        // 解析字段索引
        int field_idx = std::stoi(member);
        
        // 验证索引有效性
        const auto& element_types = tuple_type->getElementTypes();
        if (field_idx < 0 || field_idx >= static_cast<int>(element_types.size())) {
            diag_->reportError("Tuple index out of range: " + member,
                              node->getLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // 设置字段类型
        node->setType(element_types[field_idx]);
        
    } else if (obj_type->isStruct()) {
        // 结构体成员访问: struct.field_name
        auto* struct_type = static_cast<StructType*>(obj_type);
        Type* field_type = struct_type->getFieldType(member);
        if (field_type) {
            node->setType(field_type);
        } else {
            diag_->reportError("Struct has no member: " + member,
                              node->getLocation());
            node->setType(types_->getVoidType());
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
    
    // 检查索引类型必须是整数
    if (!index_type || !index_type->isInteger()) {
        diag_->reportError("Array index must be integer type",
                          node->getLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    // 检查被索引对象类型
    if (obj_type && obj_type->isArray()) {
        auto* array_type = static_cast<ArrayType*>(obj_type);
        node->setType(array_type->getElementType());
    } else if (obj_type && obj_type->isString()) {
        // string[i] 返回 char
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
    // 返回类型推导规则：
    // 1. 如果有return语句，使用return的类型
    // 2. 如果最后是表达式语句（无分号），使用该表达式的类型
    // 3. 否则返回void
    
    symbols_->enterScope();
    
    Type* block_type = types_->getVoidType();
    bool has_return = false;
    
    const auto& stmts = node->getStmts();
    for (size_t i = 0; i < stmts.size(); ++i) {
        stmts[i]->accept(this);
        
        // 检查是否有return语句
        if (auto* return_stmt = dynamic_cast<ReturnStmt*>(stmts[i].get())) {
            if (return_stmt->getValue()) {
                block_type = return_stmt->getValue()->getType();
                has_return = true;
            }
        }
        
        // 如果最后一个语句是表达式语句（且没有return），其类型作为block的类型
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
    // 数组字面量类型推断：所有元素必须为同一类型
    if (node->getElements().empty()) {
        // 空数组，无法推断类型，需要上下文信息
        // 暂时设置为unknown或i32数组作为默认
        node->setType(types_->getArrayType(types_->getI32Type(), 0));
        return;
    }
    
    // 检查第一个元素的类型
    auto& elements = node->getElements();
    elements[0]->accept(this);
    Type* element_type = elements[0]->getType();
    
    if (!element_type) {
        diag_->reportError("Cannot infer array element type", SourceLocation());
        node->setType(nullptr);
        return;
    }
    
    // 验证所有元素类型一致
    for (size_t i = 1; i < elements.size(); ++i) {
        elements[i]->accept(this);
        Type* elem_type = elements[i]->getType();
        
        if (!elem_type || !types_->equals(element_type, elem_type)) {
            diag_->reportError("Array elements must have the same type", SourceLocation());
            node->setType(nullptr);
            return;
        }
    }
    
    // 设置数组类型 [T; N]
    node->setType(types_->getArrayType(element_type, elements.size()));
}

void TypeChecker::visit(TupleExpr* node) {
    // 元组类型推断：推断每个元素的类型
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
    
    // 设置元组类型 (T1, T2, ...)
    node->setType(types_->getTupleType(element_types));
}

void TypeChecker::visit(RangeExpr* node) {
    // Range表达式类型检查
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
    
    // 验证start和end类型一致（如果都存在）
    if (start_type && end_type) {
        if (!types_->equals(start_type, end_type)) {
            diag_->reportError("Range start and end must have the same type", SourceLocation());
            node->setType(nullptr);
            return;
        }
    }
    
    // Range的元素类型是start或end的类型
    Type* range_element_type = start_type ? start_type : end_type;
    if (!range_element_type) {
        range_element_type = types_->getI32Type(); // 默认i32
    }
    
    // TODO: 设置Range类型（当前Type系统可能没有Range类型）
    // 暂时使用Slice类型表示
    node->setType(types_->getSliceType(range_element_type));
}

void TypeChecker::visit(StructLiteral* node) {
    // 结构体字面量类型检查 Point { x: 10, y: 20 }
    
    // 查找struct类型定义
    Type* struct_type = types_->lookupType(node->getStructName());
    if (!struct_type) {
        diag_->reportError("Unknown struct type: " + node->getStructName(), 
                          SourceLocation());
        node->setType(nullptr);
        return;
    }
    
    // 确保是StructType
    if (struct_type->getKind() != Type::Kind::Struct) {
        diag_->reportError(node->getStructName() + " is not a struct type", 
                          SourceLocation());
        node->setType(nullptr);
        return;
    }
    
    StructType* stype = static_cast<StructType*>(struct_type);
    const auto& struct_fields = stype->getFields();
    
    // 检查每个初始化字段
    std::set<std::string> initialized_fields;
    
    for (auto& field_init : node->getFields()) {
        // 检查字段是否存在
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
        
        // 检查字段初始化值的类型
        field_init.value->accept(this);
        Type* value_type = field_init.value->getType();
        
        if (!value_type || !types_->equals(expected_type, value_type)) {
            diag_->reportError("Type mismatch for field " + field_init.name,
                              SourceLocation());
        }
        
        initialized_fields.insert(field_init.name);
    }
    
    // 检查是否所有字段都已初始化
    for (const auto& [field_name, field_type] : struct_fields) {
        if (initialized_fields.find(field_name) == initialized_fields.end()) {
            diag_->reportError("Missing initialization for field " + field_name,
                              SourceLocation());
        }
    }
    
    // 设置struct类型
    node->setType(struct_type);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 语句类型检查
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::visit(ExprStmt* node) {
    node->getExpr()->accept(this);
}

void TypeChecker::visit(VarDecl* node) {
    if (node->getInit()) {
        // 关键修复：如果有显式类型声明，设置expected_type
        Type* saved_expected = expected_type_;
        if (node->getType()) {
            expected_type_ = node->getType();
        }
        
        // 访问初始化表达式（现在会使用expected_type）
        node->getInit()->accept(this);
        
        // 恢复expected_type
        expected_type_ = saved_expected;
        
        Type* init_type = node->getInit()->getType();
        Type* decl_type = node->getType() ? node->getType() : init_type;
        
        // 设置VarDecl的类型（CodeGen需要使用）
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
    // 元组解构: let (a, b) = tuple;
    if (!node->getInit()) {
        diag_->reportError("Destructuring declaration must have initializer",
                          SourceLocation());
        return;
    }
    
    // 检查初始化表达式的类型
    node->getInit()->accept(this);
    Type* init_type = node->getInit()->getType();
    
    if (!init_type || !init_type->isTuple()) {
        diag_->reportError("Destructuring requires tuple type",
                          SourceLocation());
        return;
    }
    
    auto* tuple_type = static_cast<TupleType*>(init_type);
    const auto& element_types = tuple_type->getElementTypes();
    
    // 验证变量数量与元组元素数量匹配
    if (node->getNames().size() != element_types.size()) {
        diag_->reportError("Destructuring pattern size mismatch",
                          SourceLocation());
        return;
    }
    
    // 为每个变量定义符号
    for (size_t i = 0; i < node->getNames().size(); ++i) {
        symbols_->defineVariable(node->getNames()[i], element_types[i], node->isMutable());
    }
}

void TypeChecker::visit(StructDestructuringDecl* node) {
    // 结构体解构: let Point { x, y } = p;
    if (!node->getInit()) {
        diag_->reportError("Struct destructuring declaration must have initializer",
                          SourceLocation());
        return;
    }
    
    // 检查初始化表达式的类型
    node->getInit()->accept(this);
    Type* init_type = node->getInit()->getType();
    
    if (!init_type || !init_type->isStruct()) {
        diag_->reportError("Struct destructuring requires struct type",
                          SourceLocation());
        return;
    }
    
    auto* struct_type = static_cast<StructType*>(init_type);
    
    // 验证结构体名称匹配
    if (struct_type->getName() != node->getStructName()) {
        diag_->reportError("Struct name mismatch in destructuring: expected " + 
                          node->getStructName() + ", got " + struct_type->getName(),
                          SourceLocation());
        return;
    }
    
    // 为每个字段定义变量
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

void TypeChecker::visit(FunctionDecl* node) {
    // 创建函数类型
    std::vector<Type*> param_types;
    for (const auto& param : node->getParams()) {
        param_types.push_back(param.type);
    }
    
    FunctionType* func_type = types_->getFunctionType(param_types, node->getReturnType());
    symbols_->defineFunction(node->getName(), func_type);
    
    // 验证where约束
    validateWhereConstraints(node->getWhereClauses(), node->getGenericParams());
    
    // 检查函数体
    symbols_->enterScope();
    current_function_return_type_ = node->getReturnType();
    
    // 添加参数到作用域
    for (const auto& param : node->getParams()) {
        symbols_->defineVariable(param.name, param.type, param.is_mutable);
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
            // === 自动包装Optional和Result ===
            // 1. 如果函数返回Optional<T>，返回值是T，允许（自动包装）
            if (current_function_return_type_->getKind() == Type::Kind::Optional) {
                auto* optional_type = static_cast<OptionalType*>(current_function_return_type_);
                Type* inner_type = optional_type->getInnerType();
                
                // 如果返回T（非Optional），允许自动包装
                if (return_type == inner_type) {
                    return;  // OK，CodeGen会自动包装
                }
                // 如果返回Optional<T>，也允许
                if (return_type == current_function_return_type_) {
                    return;  // OK，直接返回
                }
                // 特殊：如果返回null（Optional<void>），也允许
                if (return_type->getKind() == Type::Kind::Optional) {
                    auto* ret_optional = static_cast<OptionalType*>(return_type);
                    if (ret_optional->getInnerType() == types_->getVoidType()) {
                        return;  // OK，null可以用于任何Optional<T>
                    }
                }
            }
            
            // 2. 如果函数返回Result<T>，返回值是T，不允许（必须显式ok/err）
            // Result需要显式构造，不自动包装
            
            // 3. 特殊处理：Result类型需要深度比较
            if (!return_type || !types_->equals(return_type, current_function_return_type_)) {
                // 如果两个都是Result类型，检查ok_type是否匹配
                if (return_type && 
                    return_type->getKind() == Type::Kind::Result &&
                    current_function_return_type_->getKind() == Type::Kind::Result) {
                    auto* ret_result = static_cast<ResultType*>(return_type);
                    auto* func_result = static_cast<ResultType*>(current_function_return_type_);
                    if (types_->equals(ret_result->getOkType(), func_result->getOkType())) {
                        return;  // Result<T>类型匹配
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
    // for循环类型检查 for i in 0..10 { ... }
    
    // 检查iterator表达式
    node->getIterator()->accept(this);
    Type* iter_type = node->getIterator()->getType();
    
    if (!iter_type) {
        diag_->reportError("Cannot determine iterator type", SourceLocation());
        return;
    }
    
    // 确定循环变量类型
    Type* var_type = nullptr;
    
    // 如果是Range类型，循环变量类型是Range的元素类型
    if (iter_type->getKind() == Type::Kind::Slice) {
        // Range暂时用Slice表示，元素类型是Slice的元素类型
        SliceType* slice = static_cast<SliceType*>(iter_type);
        var_type = slice->getElementType();
    } else if (iter_type->getKind() == Type::Kind::Array) {
        // 数组迭代，元素类型是数组的元素类型
        ArrayType* arr = static_cast<ArrayType*>(iter_type);
        var_type = arr->getElementType();
    } else {
        // 默认i32（用于Range）
        var_type = types_->getI32Type();
    }
    
    // 在新作用域中添加循环变量
    symbols_->enterScope();
    symbols_->defineVariable(node->getVarName(), var_type, false);
    
    // 检查循环体
    node->getBody()->accept(this);
    
    symbols_->exitScope();
}

void TypeChecker::visit(StructDecl* node) {
    // 结构体定义：Parser阶段已经注册，这里可以做额外的语义检查
    
    // 类型已在Parser阶段注册，避免重复注册
    // 这里可以做：
    // - 字段类型的深度验证
    // - 泛型约束检查
    // - 循环引用检查
    
    // 当前：跳过（Parser已处理）
}

void TypeChecker::visit(EnumDecl* node) {
    // 枚举定义：Parser阶段已经注册，这里可以做额外的语义检查
    
    // 类型已在Parser阶段注册，避免重复注册
    // 这里可以做：
    // - 变体类型的深度验证
    // - 泛型约束检查
    
    // 当前：跳过（Parser已处理）
}

void TypeChecker::visit(MatchExpr* node) {
    // match表达式类型检查 - 完整实现
    
    // 1. 检查被匹配的表达式
    node->getScrutinee()->accept(this);
    Type* scrutinee_type = node->getScrutinee()->getType();
    
    if (!scrutinee_type) {
        diag_->reportError("Match scrutinee has no type", SourceLocation());
        return;
    }
    
    // 2. 检查每个分支
    Type* result_type = nullptr;
    bool has_wildcard = false;
    
    const auto& arms = node->getArms();
    
    if (arms.empty()) {
        diag_->reportError("Match expression must have at least one arm", SourceLocation());
        return;
    }
    
    for (size_t i = 0; i < arms.size(); ++i) {
        const auto& arm = arms[i];
        
        // 进入新作用域（arm的变量绑定只在arm内有效）
        symbols_->enterScope();
        
        // 设置expected_type_，让pattern知道要匹配什么类型
        Type* saved_expected = expected_type_;
        expected_type_ = scrutinee_type;
        
        // 检查pattern（会自动绑定变量）
        arm.pattern->accept(this);
        
        // 恢复expected_type_
        expected_type_ = saved_expected;
        
        // 检查是否有通配符模式
        if (dynamic_cast<WildcardPattern*>(arm.pattern.get()) ||
            dynamic_cast<VariablePattern*>(arm.pattern.get())) {
            has_wildcard = true;
        }
        
        // 检查分支表达式（变量已经在作用域中）
        arm.expression->accept(this);
        Type* arm_type = arm.expression->getType();
        
        if (!arm_type) {
            diag_->reportError("Match arm expression has no type", SourceLocation());
            symbols_->exitScope();
            continue;
        }
        
        // 所有分支必须返回相同类型
        if (!result_type) {
            result_type = arm_type;
        } else if (!types_->equals(result_type, arm_type)) {
            diag_->reportError(
                "Match arm types must be compatible: expected " + 
                result_type->toString() + ", got " + arm_type->toString(),
                SourceLocation()
            );
        }
        
        // 退出arm作用域
        symbols_->exitScope();
    }
    
    // 3. 穷尽性检查
    if (!has_wildcard && !isExhaustive(arms, scrutinee_type)) {
        diag_->reportWarning(
            "Match expression is not exhaustive, consider adding a wildcard '_' pattern",
            SourceLocation()
        );
    }
    
    // 4. 设置match表达式的类型
    if (result_type) {
        node->setType(result_type);
    }
}

void TypeChecker::visit(ClosureExpr* node) {
    // 闭包类型检查：完整实现
    // 1. 检查参数类型
    // 2. 推导返回类型（如果未指定）
    // 3. 创建FunctionType
    // 4. 分析捕获变量
    
    // 进入闭包作用域
    symbols_->enterScope();
    
    // 添加参数到作用域
    for (const auto& param : node->getParams()) {
        if (!param.type) {
            diag_->reportError("Closure parameter must have type annotation", SourceLocation());
            continue;
        }
        symbols_->defineVariable(param.name, param.type, param.is_mutable);
    }
    
    // === 关键修复：设置current_function_return_type_ ===
    // 保存旧的返回类型
    Type* saved_return_type = current_function_return_type_;
    
    // 如果闭包有显式返回类型，设置为当前函数返回类型
    // 这样ReturnStmt可以正确验证类型
    if (node->getReturnType()) {
        current_function_return_type_ = node->getReturnType();
    }
    
    // 检查闭包体
    node->getBody()->accept(this);
    Type* body_type = node->getBody()->getType();
    
    // 恢复返回类型
    current_function_return_type_ = saved_return_type;
    
    // 如果body_type为nullptr，默认为void
    if (!body_type) {
        body_type = types_->getVoidType();
    }
    
    // 推导返回类型（如果未指定）
    if (!node->getReturnType()) {
        node->setReturnType(body_type);
    }
    
    // 验证返回类型匹配
    Type* declared_return = node->getReturnType();
    if (declared_return && body_type) {
        // 使用类型系统的相等性检查而不是指针比较
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
    
    // === 捕获变量分析 ===
    // 注意：必须在退出闭包作用域之前进行，这样才能从符号表查找外部变量类型
    CaptureAnalyzer analyzer(symbols_);
    auto captured = analyzer.analyze(node->getBody(), node->getParams());
    
    // 添加捕获变量到ClosureExpr
    for (const auto& var : captured) {
        node->addCapturedVar(var);
    }
    
    // 退出闭包作用域
    symbols_->exitScope();
    
    // 创建FunctionType
    std::vector<Type*> param_types;
    for (const auto& param : node->getParams()) {
        param_types.push_back(param.type);
    }
    
    Type* return_type = node->getReturnType() ? 
        node->getReturnType() : 
        types_->getVoidType();
    
    FunctionType* fn_type = types_->getFunctionType(param_types, return_type);
    
    // 设置捕获标记（用于CodeGen判断闭包调用方式）
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
    
    // === 根据操作符区分处理 ===
    if (node->isResultTry()) {
        // expr! - Result<T> unwrap
        if (expr_type->getKind() != Type::Kind::Result) {
            diag_->reportError("! operator can only be used on Result types", SourceLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // ! 操作符返回Result<T>的T
        auto* result_type = static_cast<ResultType*>(expr_type);
        node->setType(result_type->getOkType());
    }
    else if (node->isOptionalTry()) {
        // expr? - Optional<T> unwrap
        if (expr_type->getKind() != Type::Kind::Optional) {
            diag_->reportError("? operator can only be used on Optional types", SourceLocation());
            node->setType(types_->getVoidType());
            return;
        }
        
        // ? 操作符返回Optional<T>的T
        auto* optional_type = static_cast<OptionalType*>(expr_type);
        node->setType(optional_type->getInnerType());
    }
    else {
        diag_->reportError("Unknown try operator", SourceLocation());
        node->setType(types_->getVoidType());
    }
}

// 辅助方法：检查模式类型兼容性
void TypeChecker::checkPatternType(Pattern* pattern, Type* expected_type) {
    if (auto* lit = dynamic_cast<LiteralPattern*>(pattern)) {
        // 检查字面量类型是否匹配
        Type* pattern_type = nullptr;
        
        switch (lit->getKind()) {
            case LiteralPattern::Kind::Int:
                // 检查是否为整数类型
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
                // 检查是否为浮点类型
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
    
    // WildcardPattern和VariablePattern匹配任何类型
    if (dynamic_cast<WildcardPattern*>(pattern) || 
        dynamic_cast<VariablePattern*>(pattern)) {
        return;
    }
    
    // TuplePattern: 检查元组元素类型
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
        
        // 递归检查每个元素
        for (size_t i = 0; i < elements.size(); ++i) {
            checkPatternType(elements[i].get(), element_types[i]);
        }
    }
    
    // EnumPattern: 检查枚举变体
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
        
        // 检查变体是否存在
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
        
        // 如果有内部模式，检查数据类型
        if (enum_pat->getInner()) {
            if (!variant_data_type) {
                diag_->reportError(
                    "Enum variant '" + enum_pat->getVariantName() + 
                    "' does not have associated data",
                    SourceLocation()
                );
                return;
            }
            
            // 递归检查内部模式
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

// 辅助方法：检查穷尽性
bool TypeChecker::isExhaustive(const std::vector<MatchArm>& arms, Type* scrutinee_type) {
    // 如果有通配符或变量绑定模式，就是穷尽的
    for (const auto& arm : arms) {
        if (dynamic_cast<WildcardPattern*>(arm.pattern.get()) ||
            dynamic_cast<VariablePattern*>(arm.pattern.get())) {
            return true;
        }
    }
    
    // 对于整数类型，难以检查穷尽性（范围太大）
    if (scrutinee_type->getKind() >= Type::Kind::I8 &&
        scrutinee_type->getKind() <= Type::Kind::U128) {
        return false;  // 保守：总是要求通配符
    }
    
    // 对于布尔类型，检查是否覆盖true和false
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
    
    // 对于枚举类型，检查是否覆盖所有变体
    if (scrutinee_type->getKind() == Type::Kind::Enum) {
        auto* enum_type = static_cast<EnumType*>(scrutinee_type);
        const auto& variants = enum_type->getVariants();
        
        // 收集所有匹配的变体
        std::set<std::string> matched_variants;
        
        for (const auto& arm : arms) {
            if (auto* enum_pat = dynamic_cast<EnumPattern*>(arm.pattern.get())) {
                matched_variants.insert(enum_pat->getVariantName());
            }
        }
        
        // 检查是否所有变体都被覆盖
        if (matched_variants.size() == variants.size()) {
            // 检查每个变体名是否都在
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
    
    // 其他类型保守处理
    return false;
}

void TypeChecker::visit(InterfaceDecl* node) {
    // 接口定义：Parser阶段已经注册，这里可以做额外的语义检查
    
    // 类型已在Parser阶段注册，避免重复注册
    // 这里可以做：
    // - 方法签名的深度验证
    // - 泛型约束检查
    // - 接口继承检查
    
    // 当前：跳过（Parser已处理）
}

void TypeChecker::visit(SupportDecl* node) {
    // support实现：验证接口方法完整性
    
    // 1. 查找目标类型
    Type* target_type = types_->lookupType(node->getTypeName());
    if (!target_type) {
        diag_->reportError(
            "Type '" + node->getTypeName() + "' not found",
            SourceLocation()
        );
        return;
    }
    
    // 设置Self类型上下文
    Type* saved_self_type = current_self_type_;
    current_self_type_ = target_type;
    
    // 2. 查找接口类型
    Type* interface_type = types_->lookupType(node->getInterfaceName());
    if (!interface_type || interface_type->getKind() != Type::Kind::Interface) {
        diag_->reportError(
            "Interface '" + node->getInterfaceName() + "' not found",
            SourceLocation()
        );
        return;
    }
    
    auto* iface = static_cast<InterfaceType*>(interface_type);
    
    // 3. 获取接口定义中的所有方法
    const auto& required_methods = iface->getMethods();
    const auto& implemented_methods = node->getMethods();
    
    // 4. 验证所有接口方法都被实现
    for (const auto& required : required_methods) {
        bool found = false;
        
        for (const auto& impl_method : implemented_methods) {
            if (impl_method->getName() == required.name) {
                found = true;
                
                // 验证方法签名
                const auto& impl_params = impl_method->getParams();
                
                // 检查参数数量（不包括self）
                if (impl_params.size() != required.param_types.size()) {
                    diag_->reportError(
                        "Method '" + required.name + "' has wrong number of parameters: " +
                        "expected " + std::to_string(required.param_types.size()) +
                        ", got " + std::to_string(impl_params.size()),
                        SourceLocation()
                    );
                }
                
                // 检查参数类型
                for (size_t i = 0; i < impl_params.size() && i < required.param_types.size(); ++i) {
                    if (!types_->equals(impl_params[i].type, required.param_types[i])) {
                        diag_->reportError(
                            "Method '" + required.name + "' parameter " + std::to_string(i) +
                            " type mismatch: expected " + required.param_types[i]->toString() +
                            ", got " + impl_params[i].type->toString(),
                            SourceLocation()
                        );
                    }
                }
                
                // 检查返回类型
                if (!types_->equals(impl_method->getReturnType(), required.return_type)) {
                    diag_->reportError(
                        "Method '" + required.name + "' return type mismatch: " +
                        "expected " + required.return_type->toString() +
                        ", got " + impl_method->getReturnType()->toString(),
                        SourceLocation()
                    );
                }
                
                break;
            }
        }
        
        if (!found) {
            diag_->reportError(
                "Method '" + required.name + "' not implemented for interface '" +
                node->getInterfaceName() + "'",
                SourceLocation()
            );
        }
    }
    
    // 5. 检查所有实现的方法
    for (const auto& impl_method : implemented_methods) {
        // 在self参数作用域下检查方法体
        // TODO: 设置self类型到符号表
        impl_method->accept(this);
    }
    
    // 恢复Self类型上下文
    current_self_type_ = saved_self_type;
}

// Pattern类型检查 - 这些方法在checkPatternType中调用
void TypeChecker::visit(LiteralPattern* node) {
    // 字面量模式：验证字面量与expected_type_匹配
    // 当前简化版本：不做深入检查
}

void TypeChecker::visit(WildcardPattern* node) {
    // 通配符模式：总是匹配，不需要类型检查
}

void TypeChecker::visit(VariablePattern* node) {
    // 变量绑定模式：将变量绑定到expected_type_
    if (expected_type_) {
        symbols_->defineVariable(node->getName(), expected_type_, false);
    } else {
        // 如果没有expected_type_，报错或使用void
        diag_->reportError(
            "Cannot infer type for pattern variable '" + node->getName() + "'",
            SourceLocation()
        );
    }
}

void TypeChecker::visit(TuplePattern* node) {
    // 元组模式：递归检查每个元素
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
    // 需要expected_type_来知道要匹配的类型
    if (!expected_type_) {
        diag_->reportError("Cannot infer pattern type", SourceLocation());
        return;
    }
    
    Type* scrutinee_type = expected_type_;
    
    // === 特殊处理 Result 类型 ===
    if (scrutinee_type->getKind() == Type::Kind::Result) {
        handleResultPattern(node, static_cast<ResultType*>(scrutinee_type));
        return;
    }
    
    // === 特殊处理 Optional 类型 ===
    if (scrutinee_type->getKind() == Type::Kind::Optional) {
        handleOptionalPattern(node, static_cast<OptionalType*>(scrutinee_type));
        return;
    }
    
    // === 处理用户定义的枚举类型 ===
    if (scrutinee_type->getKind() == Type::Kind::Enum) {
        handleEnumPattern(node, static_cast<EnumType*>(scrutinee_type));
        return;
    }
    
    // 错误：不是枚举类型
    diag_->reportError(
        "Cannot match enum pattern against non-enum type",
        SourceLocation()
    );
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 枚举模式处理辅助方法
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::handleResultPattern(EnumPattern* node, ResultType* result_type) {
    std::string variant = node->getVariantName();
    
    // Ok 变体
    if (variant == "Ok") {
        auto& inner_patterns = node->getInnerPatterns();
        
        // 检查参数数量
        if (inner_patterns.size() != 1) {
            diag_->reportError(
                "Result::Ok expects exactly 1 argument",
                SourceLocation()
            );
            return;
        }
        
        // 递归检查内部模式，期望类型为 result_type->getOkType()
        Type* saved_expected = expected_type_;
        expected_type_ = result_type->getOkType();
        inner_patterns[0]->accept(this);
        expected_type_ = saved_expected;
    }
    // Err 变体
    else if (variant == "Err") {
        auto& inner_patterns = node->getInnerPatterns();
        
        if (inner_patterns.size() != 1) {
            diag_->reportError(
                "Result::Err expects exactly 1 argument",
                SourceLocation()
            );
            return;
        }
        
        // Err的参数固定为string
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
        // None无参数
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
    
    // 查找匹配的变体
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
            "Unknown variant '" + variant_name + "' for enum " + enum_type->getName(),
            SourceLocation()
        );
        return;
    }
    
    // 检查参数
    if (variant_data_type && inner_patterns.size() != 1) {
        diag_->reportError(
            "Variant '" + variant_name + "' expects 1 argument",
            SourceLocation()
        );
        return;
    } else if (!variant_data_type && !inner_patterns.empty()) {
        diag_->reportError(
            "Variant '" + variant_name + "' takes no arguments",
            SourceLocation()
        );
        return;
    }
    
    // 检查参数类型
    if (variant_data_type && !inner_patterns.empty()) {
        Type* saved_expected = expected_type_;
        expected_type_ = variant_data_type;
        inner_patterns[0]->accept(this);
        expected_type_ = saved_expected;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Where约束验证
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::validateWhereConstraints(
    const std::vector<WhereClause>& where_clauses,
    const std::vector<GenericParam>& generic_params) {
    
    // 对于每个where约束
    for (const auto& clause : where_clauses) {
        // 检查type_param是否是泛型参数
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
        
        // TODO: 检查接口是否存在
        // Type* interface_type = types_->lookupType(clause.interface_name);
        // if (!interface_type) {
        //     diag_->reportError("Unknown interface in where clause: " + 
        //                       clause.interface_name, SourceLocation());
        // }
        
        // 注意：实际的约束验证在单态化时进行
        // 这里只做基本的语法验证
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Self类型解析
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Type* TypeChecker::resolveSelfType(Type* type) {
    if (!type) {
        return nullptr;
    }
    
    // 如果是SelfType，替换为当前的Self类型
    if (type->getKind() == Type::Kind::SelfType) {
        if (current_self_type_) {
            return current_self_type_;
        }
        // 如果没有当前Self类型，报错
        diag_->reportError("Self type used outside of support context", SourceLocation());
        return types_->getVoidType();
    }
    
    // 递归处理复合类型
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
        default:
            // 基础类型，不需要替换
            return type;
    }
}

void TypeChecker::visit(StructPattern* node) {
    // 结构体模式匹配 - 完整实现
    
    // 1. 检查expected_type_是否为StructType
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
    
    // 2. 检查结构体名称是否匹配
    if (struct_type->getName() != node->getStructName()) {
        diag_->reportError(
            "Struct pattern name mismatch: expected " + struct_type->getName() + 
            ", got " + node->getStructName(),
            SourceLocation()
        );
        return;
    }
    
    // 3. 检查每个字段模式
    for (const auto& field_pattern : node->getFields()) {
        const std::string& field_name = field_pattern.field_name;
        
        // 3.1 检查字段是否存在
        Type* field_type = struct_type->getFieldType(field_name);
        if (!field_type) {
            diag_->reportError(
                "Struct " + struct_type->getName() + " has no field named '" + field_name + "'",
                SourceLocation()
            );
            continue;
        }
        
        // 3.2 递归检查字段模式，期望类型为字段类型
        Type* saved_expected = expected_type_;
        expected_type_ = field_type;
        field_pattern.pattern->accept(this);
        expected_type_ = saved_expected;
    }
    
    // 注意：这里不检查是否所有字段都被匹配，因为结构体模式可以部分匹配
}

} // namespace pawc
