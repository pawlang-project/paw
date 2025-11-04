//===--- type_checker.cpp - Type Checker Implementation ----------*- C++ -*-===//

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

void TypeChecker::visit(NoneLiteral* node) {
    // none字面量的类型需要从上下文推导
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
    // self表达式：使用self参数的实际类型（可能是引用类型）
    if (current_self_param_type_) {
        // 🔧 解析Self类型：如果是 &Self，解析为 &Point
        Type* resolved_type = resolveSelfType(current_self_param_type_);
        node->setType(resolved_type);
    } else if (current_self_type_) {
        // Fallback: 使用current_self_type_（可能不准确）
        node->setType(current_self_type_);
    } else {
        node->setType(nullptr);
    }
}

void TypeChecker::visit(IdentifierExpr* node) {
    const std::string& name = node->getName();
    
    // 特殊处理: ok/err/some builtin（作为CallExpr的callee时不需要类型）
    if (name == "ok" || name == "err" || name == "some") {
        // 类型由包含它的CallExpr确定
        node->setType(nullptr);  // 占位符
        return;
    }
    
    Symbol* sym = symbols_->lookup(name);
    if (!sym) {
        // 🔧 特殊处理: self标识符（如果在符号表中找不到）
        if (name == "self" && current_self_type_) {
            // self的类型是当前support的类型
            // 但如果是引用参数，需要返回引用类型
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
    
    // 比较运算符（返回bool）
    if (op == TokenType::EQ_EQ || op == TokenType::NOT_EQ ||
        op == TokenType::LT || op == TokenType::LESS_EQ ||
        op == TokenType::GT || op == TokenType::GREATER_EQ) {
        
        // 特殊处理: Optional<T> 与 null 的比较
        // null 的类型是 Optional<void>，但可以与任何 Optional<T> 比较
        bool is_optional_null_comparison = false;
        if ((left_type->isOptional() && right_type->isOptional())) {
            auto* left_opt = static_cast<OptionalType*>(left_type);
            auto* right_opt = static_cast<OptionalType*>(right_type);
            // 如果其中一个是 Optional<void> (null)，允许比较
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
    // === 🔧 Bug Fix: 检查是否是接口方法调用 ===
    // 方法调用形式: obj.method() 被解析为 CallExpr(MemberExpr(obj, "method"), args)
    if (auto* member_expr = dynamic_cast<MemberExpr*>(node->getCallee())) {
        Expr* object = member_expr->getObject();
        const std::string& method_name = member_expr->getMember();
        
        // 先检查object的类型
        object->accept(this);
        Type* obj_type = object->getType();
        
        // 🔧 处理引用类型：如果是引用，获取pointee类型
        if (obj_type && obj_type->isReference()) {
            auto* ref_type = static_cast<ReferenceType*>(obj_type);
            obj_type = ref_type->getPointeeType();
        }
        
        if (obj_type && obj_type->isStruct()) {
            auto* struct_type = static_cast<StructType*>(obj_type);
            
            // 检查是否是字段（而不是方法）
            const auto& fields = struct_type->getFields();
            bool is_field = false;
            for (const auto& field : fields) {
                if (field.first == method_name) {
                    is_field = true;
                    break;
                }
            }
            
            // 如果不是字段，查找接口方法
            if (!is_field) {
                // 🔧 尝试多种命名方式
                std::string full_method_name1 = struct_type->getName() + "_" + method_name;   // 旧式
                std::string full_method_name2 = struct_type->getName() + "::" + method_name;  // 新式
                
                // 从符号表查找方法
                Symbol* method_symbol = symbols_->lookup(full_method_name1);
                std::string full_method_name = full_method_name1;
                
                if (!method_symbol) {
                    method_symbol = symbols_->lookup(full_method_name2);
                    full_method_name = full_method_name2;
                }
                
                if (method_symbol && method_symbol->getKind() == Symbol::Kind::Function) {
                    // 找到接口方法！标记为方法调用
                    std::cerr << "[TypeChecker] Found method: " << full_method_name << std::endl;
                    node->setIsMethodCall(true);
                    node->setReceiver(object);
                    node->setMethodName(method_name);
                    node->setMethodTarget(full_method_name);
                    
                    // 类型检查参数
                    std::vector<Type*> arg_types;
                    for (const auto& arg : node->getArgs()) {
                        arg->accept(this);
                        arg_types.push_back(arg->getType());
                    }
                    
                    // 验证方法签名
                    auto* func_symbol = static_cast<FunctionSymbol*>(method_symbol);
                    FunctionType* func_type = func_symbol->getType();
                    const auto& func_params = func_type->getParamTypes();
                    
                    // 🔧 Self参数支持：方法第一个参数是self，用户调用时不传递
                    // 所以arg_types的数量应该比func_params少1（跳过self）
                    size_t expected_args = func_params.empty() ? 0 : func_params.size() - 1;
                    if (arg_types.size() != expected_args) {
                        diag_->reportError(
                            "Method call argument count mismatch: expected " +
                            std::to_string(expected_args) + ", got " +
                            std::to_string(arg_types.size()),
                            SourceLocation()
                        );
                    }
                    
                    // 设置返回类型
                    node->setType(func_type->getReturnType());
                    return;
                }
            }
        }
    }
    
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
        
        // 🔧 泛型函数调用处理
        if (node->hasTypeArgs()) {
            // 这是泛型函数调用: identity<i32>(42)
            auto* tmpl = types_->lookupGenericTemplate(func_name);
            if (tmpl && tmpl->kind == GenericTemplate::FUNCTION) {
                // ✅ 检查 where 约束
                FunctionDecl* func_decl = tmpl->func_def;
                if (func_decl) {
                    const auto& where_clauses = func_decl->getWhereClauses();
                    
                    if (!where_clauses.empty()) {
                        std::cerr << "[WhereClause] Checking constraints for generic call: " 
                                  << func_name << std::endl;
                        
                        // 构建类型替换映射
                        std::map<std::string, Type*> type_sub;
                        const auto& type_args = node->getTypeArgs();
                        for (size_t i = 0; i < tmpl->type_params.size() && i < type_args.size(); ++i) {
                            type_sub[tmpl->type_params[i]] = type_args[i];
                        }
                        
                        // 验证 where 约束
                        if (!checkWhereConstraintsSatisfied(where_clauses, type_sub)) {
                            node->setType(types_->getVoidType());
                            return;
                        }
                    }
                }
                
                // 实例化函数
                Type* return_type = instantiateFunctionReturnType(tmpl, node->getTypeArgs());
                if (return_type) {
                    node->setType(return_type);
                    return;
                }
            }
        }
        
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
        
        // 特殊处理: some(value) - Optional构造器
        if (func_name == "some" && arg_types.size() == 1) {
            // some(value) 返回 Optional<T>，T是value的类型
            Type* value_type = arg_types[0];
            node->setType(types_->getOptionalType(value_type));
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
        
        // 特殊处理: println/print/to_string - 支持所有基本类型
        if ((func_name == "println" || func_name == "print" || func_name == "to_string") 
            && arg_types.size() == 1) {
            // 验证参数类型是基本类型或已知类型
            Type* arg_type = arg_types[0];
            if (arg_type) {
                // println/print 返回 void
                if (func_name == "println" || func_name == "print") {
                    node->setType(types_->getVoidType());
                    return;
                }
                // to_string 返回 string
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
    // 静态访问: Type::Variant 或 Type::method
    // 🔧 M7: 支持泛型模板静态访问
    
    // 首先尝试查找具体类型
    Type* type = types_->lookupType(node->getTypeName());
    
    // 如果没找到，检查是否是泛型模板
    if (!type) {
        auto* tmpl = types_->lookupGenericTemplate(node->getTypeName());
        if (tmpl && expected_type_ && expected_type_->isEnum()) {
            // 从expected_type_获取实例化类型
            type = expected_type_;
        }
    }
    
    if (!type) {
        diag_->reportError("Unknown type: " + node->getTypeName(), node->getLocation());
        node->setType(types_->getVoidType());
        return;
    }
    
    // 🔧 M7: 处理枚举构造器 Option::Some
    if (type->isEnum()) {
        EnumType* enum_type = static_cast<EnumType*>(type);
        
        // 查找variant
        bool found = false;
        for (const auto& variant : enum_type->getVariants()) {
            if (variant.first == node->getMember()) {
                // 找到variant！创建一个特殊的函数类型表示构造器
                if (variant.second) {
                    // 有关联数据的variant
                    std::vector<Type*> param_types;
                    
                    // 🔧 检查是否是元组类型（多参数variant）
                    if (variant.second->isTuple()) {
                        // 多参数：展开元组类型为多个参数
                        // Move((i32, i32)) -> Move(i32, i32)
                        TupleType* tuple_type = static_cast<TupleType*>(variant.second);
                        param_types = tuple_type->getElementTypes();
                    } else {
                        // 单参数
                        param_types = {variant.second};
                    }
                    
                    Type* constructor_type = types_->getFunctionType(param_types, enum_type);
                    node->setType(constructor_type);
                } else {
                    // 无关联数据的variant，直接是enum类型
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
    
    // TODO: 处理其他静态访问（接口方法等）
    node->setType(types_->getVoidType());
}

void TypeChecker::visit(MemberExpr* node) {
    node->getObject()->accept(this);
    
    Type* obj_type = node->getObject()->getType();
    if (!obj_type) {
        node->setType(types_->getVoidType());
        return;
    }
    
    // 🔧 引用类型处理：如果对象是引用类型，获取它的pointee类型
    if (obj_type->isReference()) {
        auto* ref_type = static_cast<ReferenceType*>(obj_type);
        obj_type = ref_type->getPointeeType();
        // 🔧 Self类型解析：如果pointee是Self，解析为实际类型
        obj_type = resolveSelfType(obj_type);
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
            // 🔧 Bug Fix: 如果不是字段，可能是接口方法
            // 尝试多种方法命名方式
            std::string method_name1 = struct_type->getName() + "_" + member;  // 旧式
            std::string method_name2 = struct_type->getName() + "::" + member; // 新式（support方法）
            
            Symbol* method_symbol = symbols_->lookup(method_name1);
            if (!method_symbol) {
                method_symbol = symbols_->lookup(method_name2);
            }
            
            if (method_symbol && method_symbol->getKind() == Symbol::Kind::Function) {
                // 这是方法！设置为方法的返回类型
                // 注意：MemberExpr单独出现时，实际上会被包在CallExpr中
                // 这里设置类型主要是为了防止报错
                auto* func_symbol = static_cast<FunctionSymbol*>(method_symbol);
                node->setType(func_symbol->getType()->getReturnType());
            } else {
                // 既不是字段也不是方法，才报错
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
    // 结构体字面量类型检查 Point { x: 10, y: 20 } 或 Box { value: 42 }
    
    // 🔧 泛型Struct支持：查找struct类型定义（包括实例化类型）
    Type* struct_type = types_->lookupType(node->getStructName());
    
    // 如果没找到，检查是否是泛型模板，从expected_type_推导
    if (!struct_type) {
        auto* tmpl = types_->lookupGenericTemplate(node->getStructName());
        if (tmpl && expected_type_ && expected_type_->isStruct()) {
            // 从expected_type_获取实例化类型
            struct_type = expected_type_;
        }
    }
    
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
        Type* decl_type = nullptr;
        
        // 🔧 类型推导：如果没有显式类型，尝试从初始化表达式推导
        if (node->getType()) {
            // 有显式类型，使用它
            decl_type = node->getType();
        } else if (init_type) {
            // 无显式类型，使用推导的类型
            decl_type = init_type;
            std::cerr << "[TypeInference] Inferred type for variable '" 
                      << node->getName() << "': " << decl_type->toString() << std::endl;
        } else {
            // 无法推导，报错
            diag_->reportError(
                "Cannot infer type for variable '" + node->getName() + 
                "' - please provide explicit type annotation",
                node->getLocation()
            );
            return;
        }
        
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
        // 🔧 Fix: 解析Self类型为实际类型
        param_types.push_back(resolveSelfType(param.type));
    }
    
    // 🔧 Fix: 解析返回类型中的Self
    Type* resolved_return_type = resolveSelfType(node->getReturnType());
    FunctionType* func_type = types_->getFunctionType(param_types, resolved_return_type);
    
    // 🔧 Self类型支持：保存解析后的类型到AST节点，供CodeGen使用
    node->setResolvedTypes(param_types, resolved_return_type);
    
    // 🔧 Bug Fix: 如果在接口实现上下文中（current_self_type_非空），
    // 使用修饰后的方法名 TypeName_methodName
    std::string func_name = node->getName();
    if (current_self_type_) {
        func_name = current_self_type_->toString() + "_" + node->getName();
    }
    
    symbols_->defineFunction(func_name, func_type);
    
    // 验证where约束
    validateWhereConstraints(node->getWhereClauses(), node->getGenericParams());
    
    // 检查函数体
    symbols_->enterScope();
    // 🔧 Fix: 使用解析后的返回类型
    current_function_return_type_ = resolved_return_type;
    
    // 添加参数到作用域
    for (size_t i = 0; i < node->getParams().size(); ++i) {
        const auto& param = node->getParams()[i];
        // 🔧 Fix: 使用解析后的参数类型
        symbols_->defineVariable(param.name, param_types[i], param.is_mutable);
        
        // 🔧 Self参数类型记录：如果参数名是self，记录它的实际类型（可能是引用）
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
        
        // 检查守卫条件 (if guard)
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
    // 闭包类型检查：完整实现 + 类型推导
    // 1. 从expected_type_推导参数类型（如果需要）
    // 2. 检查参数类型
    // 3. 推导返回类型（如果未指定）
    // 4. 创建FunctionType
    
    // 🔧 类型推导：从expected_type_推导参数类型
    if (expected_type_ && expected_type_->isFunction()) {
        auto* expected_fn_type = static_cast<FunctionType*>(expected_type_);
        const auto& expected_params = expected_fn_type->getParamTypes();
        auto& closure_params = const_cast<std::vector<ClosureExpr::Param>&>(node->getParams());
        
        // 推导无类型的参数
        for (size_t i = 0; i < closure_params.size() && i < expected_params.size(); i++) {
            if (!closure_params[i].type) {
                closure_params[i].type = expected_params[i];  // 推导！
            }
        }
        
        // 推导返回类型（如果未指定）
        if (!node->getReturnType()) {
            node->setReturnType(expected_fn_type->getReturnType());
        }
    }
    
    // 进入闭包作用域
    symbols_->enterScope();
    
    // 添加参数到作用域
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
    // 接口定义：Parser阶段已经注册，这里跳过类型检查
    
    // 🔧 接口默认方法的 body 不在这里检查，原因：
    // - 接口定义时，Self 是抽象类型，不知道具体的字段
    // - 默认方法的类型检查应该在每个 support 时进行，此时知道具体类型
    // - 当前简化：跳过默认方法 body 的类型检查（仅在 CodeGen 时使用）
    
    // 未来优化：可以在 support 时，用具体类型替换 Self，然后检查默认方法 body
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
    
    // 🔧 泛型接口支持：创建泛型参数替换映射
    // 如果接口是泛型的（如Comparable<T>），且有实例化参数（如<Number>）
    // 需要将T替换为Number
    std::map<std::string, Type*> generic_substitution;
    
    if (iface->isGeneric() && node->isInterfaceGeneric()) {
        const auto& interface_def_param_names = iface->getGenericParamNames();
        const auto& interface_inst_params = node->getInterfaceGenericParams();
        
        std::cerr << "[GenericInterface] Building substitution map:" << std::endl;
        std::cerr << "  Interface: " << node->getInterfaceName() << std::endl;
        std::cerr << "  Generic params: " << interface_def_param_names.size() << std::endl;
        std::cerr << "  Instance params: " << interface_inst_params.size() << std::endl;
        
        // 构建替换映射：T -> Number
        for (size_t i = 0; i < interface_def_param_names.size() && i < interface_inst_params.size(); ++i) {
            // 查找实例化的类型
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
                
                // 检查参数类型（应用泛型参数替换）
                for (size_t i = 0; i < impl_params.size() && i < required.param_types.size(); ++i) {
                    Type* expected_type = required.param_types[i];
                    
                    // 🔧 泛型接口：替换泛型参数
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
                
                // 检查返回类型（应用泛型参数替换）
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
            // 🔧 检查接口方法是否有默认实现
            if (!required.has_default_impl) {
            diag_->reportError(
                "Method '" + required.name + "' not implemented for interface '" +
                node->getInterfaceName() + "'",
                SourceLocation()
            );
            }
            // 如果有默认实现，不报错（将使用默认实现）
        }
    }
    
    // 5. 检查所有实现的方法
    for (const auto& impl_method : implemented_methods) {
        // 在self参数作用域下检查方法体
        // TODO: 设置self类型到符号表
        impl_method->accept(this);
    }
    
    // 6. ✅ 验证 where 约束（条件实现）
    if (!node->getWhereClauses().empty()) {
        std::cerr << "[SupportDecl] Validating where clauses for " 
                  << node->getTypeName() << " with " << node->getInterfaceName() << std::endl;
        
        // 验证 where 约束语法（检查接口存在性和泛型参数有效性）
        validateWhereConstraints(node->getWhereClauses(), node->getTypeGenericParams());
        
        // 注意：实际的约束满足性检查会在单态化时进行
        // 因为泛型参数在定义时是抽象的
    }
    
    // 7. 🔧 为有默认实现但未实现的方法注册函数符号并检查 body
    for (const auto& required : required_methods) {
        if (required.has_default_impl) {
            // 检查是否已实现
            bool found = false;
            for (const auto& impl_method : implemented_methods) {
                if (impl_method->getName() == required.name) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::cerr << "[TypeChecker] Processing default method: " << required.name << std::endl;
                
                // 注册默认方法的函数符号
                std::string method_name = node->getTypeName() + "::" + required.name;
                
                // 构建函数类型
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
                
                // 🔧 关键修复：对默认方法的 body 进行类型检查
                // 此时 current_self_type_ 已经是具体类型（target_type）
                if (required.default_body) {
                    std::cerr << "[TypeChecker] Checking default method body with concrete type: " 
                              << current_self_type_->toString() << std::endl;
                    
                    // 进入方法作用域
                    symbols_->enterScope();
                    
                    // 绑定参数（包括 self）- 使用具体类型 + 泛型替换！
                    for (size_t i = 0; i < required.param_types.size(); ++i) {
                        Type* param_type = required.param_types[i];
                        std::string param_name = (i < required.param_names.size()) 
                                               ? required.param_names[i] 
                                               : ("arg" + std::to_string(i));
                        
                        // 🔧 泛型接口：应用泛型参数替换（T -> Number）
                        if (!generic_substitution.empty()) {
                            param_type = substituteGenericType(param_type, generic_substitution);
                            std::cerr << "[GenericInterface] Substituted param " << param_name 
                                      << " type to " << param_type->toString() << std::endl;
                        }
                        
                        // 🔧 检查是否是 Self 或 &Self
                        if (param_type->getKind() == Type::Kind::SelfType) {
                            // Self (值) - 用具体类型替换
                            symbols_->defineVariable(param_name, current_self_type_, false);
                        } else if (param_type->getKind() == Type::Kind::Reference) {
                            auto* ref_type = static_cast<ReferenceType*>(param_type);
                            if (ref_type->getPointeeType()->getKind() == Type::Kind::SelfType) {
                                // &Self (引用) - 创建具体类型的引用
                                Type* concrete_ref = types_->getReferenceType(current_self_type_, ref_type->isMutable());
                                symbols_->defineVariable(param_name, concrete_ref, false);
                            } else {
                                // 普通参数（已应用泛型替换）
                                symbols_->defineVariable(param_name, param_type, false);
                            }
                        } else {
                            // 其他参数（已应用泛型替换）
                            symbols_->defineVariable(param_name, param_type, false);
                        }
                    }
                    
                    // 访问 body 进行类型检查（现在 Self 是具体类型，泛型参数已替换）
                    required.default_body->accept(this);
                    
                    // 退出作用域
                    symbols_->exitScope();
                    
                    std::cerr << "[TypeChecker] Default method body checked successfully" << std::endl;
                }
                
                std::cerr << "[TypeChecker] Registered default method: " << method_name << std::endl;
            }
        }
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
    
    // 查找variant定义
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
    
    // 无数据的variant
    if (!variant_data_type) {
        if (!inner_patterns.empty()) {
        diag_->reportError(
                "Variant '" + variant_name + "' takes no arguments",
            SourceLocation()
        );
        }
        return;
    }
    
    // 🔧 多参数支持：检查是否是元组类型
    if (variant_data_type->isTuple()) {
        // 多参数variant：展开元组为多个类型
        TupleType* tuple_type = static_cast<TupleType*>(variant_data_type);
        const auto& element_types = tuple_type->getElementTypes();
        
        // 检查pattern数量是否匹配
        if (inner_patterns.size() != element_types.size()) {
        diag_->reportError(
                "Variant '" + variant_name + "' expects " + 
                std::to_string(element_types.size()) + " arguments, got " +
                std::to_string(inner_patterns.size()),
            SourceLocation()
        );
        return;
    }
    
        // 为每个内部pattern设置正确的类型
        Type* saved_expected = expected_type_;
        for (size_t i = 0; i < inner_patterns.size(); ++i) {
            expected_type_ = element_types[i];
            inner_patterns[i]->accept(this);
        }
        expected_type_ = saved_expected;
    } else {
        // 单参数variant
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
// Where约束验证
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TypeChecker::validateWhereConstraints(
    const std::vector<WhereClause>& where_clauses,
    const std::vector<GenericParam>& generic_params) {
    
    // 对于每个where约束
    for (const auto& clause : where_clauses) {
        // 1. 检查type_param是否是泛型参数
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
        
        // 2. ✅ 检查接口是否存在
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

// ✅ 检查类型是否实现了接口
bool TypeChecker::typeImplementsInterface(Type* type, Type* interface_type) {
    if (!type || !interface_type) {
        return false;
    }
    
    if (interface_type->getKind() != Type::Kind::Interface) {
        return false;
    }
    
    auto* iface = static_cast<InterfaceType*>(interface_type);
    std::string interface_name = iface->getName();
    
    // 检查类型是否有对应的 support 实现
    // 通过检查符号表中是否存在 TypeName::MethodName 形式的方法
    
    if (!type->isStruct() && !type->isEnum()) {
        return false;  // 只有 struct 和 enum 可以实现接口
    }
    
    std::string type_name;
    if (type->isStruct()) {
        type_name = static_cast<StructType*>(type)->getName();
    } else if (type->isEnum()) {
        type_name = static_cast<EnumType*>(type)->getName();
    }
    
    // 检查接口的所有方法是否都被实现
    const auto& methods = iface->getMethods();
    for (const auto& method : methods) {
        // 尝试两种命名方式
        std::string method_name1 = type_name + "_" + method.name;
        std::string method_name2 = type_name + "::" + method.name;
        
        Symbol* method_symbol = symbols_->lookup(method_name1);
        if (!method_symbol) {
            method_symbol = symbols_->lookup(method_name2);
        }
        
        // 如果方法没有实现且没有默认实现，则不满足
        if (!method_symbol && !method.has_default_impl) {
            return false;
        }
    }
    
    return true;
}

// ✅ 验证where约束是否满足
bool TypeChecker::checkWhereConstraintsSatisfied(
    const std::vector<WhereClause>& where_clauses,
    const std::map<std::string, Type*>& type_substitution) {
    
    for (const auto& clause : where_clauses) {
        // 获取实际类型（从替换映射中）
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
        
        // 检查类型是否实现了接口
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
        case Type::Kind::Reference: {
            // 🔧 引用类型：解析pointee中的Self
            auto* ref = static_cast<ReferenceType*>(type);
            Type* pointee = resolveSelfType(ref->getPointeeType());
            return types_->getReferenceType(pointee, ref->isMutable());
        }
        default:
            // 基础类型，不需要替换
            return type;
    }
}

// 泛型参数替换辅助函数
Type* TypeChecker::substituteGenericType(Type* type, const std::map<std::string, Type*>& substitution) {
    if (!type) return nullptr;
    
    // 如果是泛型类型，查找替换
    if (type->getKind() == Type::Kind::Generic) {
        auto* generic = static_cast<GenericType*>(type);
        auto it = substitution.find(generic->getName());
        if (it != substitution.end()) {
            return it->second;  // 返回替换后的类型
        }
        return type;  // 没有找到替换，保持原样
    }
    
    // 递归处理复合类型
    // TODO: 如果需要支持更复杂的情况（如Pair<T, U>），需要递归处理
    
    return type;
}

void TypeChecker::visit(ArrayPattern* node) {
    // 数组模式匹配 - 完整实现
    
    // 1. 检查expected_type_是否为ArrayType
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
    
    // 2. 检查数组大小是否匹配
    if (array_type->getSize() != node->getExpectedSize()) {
        diag_->reportError(
            "Array pattern size mismatch: expected " + 
            std::to_string(array_type->getSize()) + 
            ", got " + std::to_string(node->getExpectedSize()),
            SourceLocation()
        );
        return;
    }
    
    // 3. 递归检查每个元素模式
    Type* elem_type = array_type->getElementType();
    for (const auto& elem_pattern : node->getElements()) {
        Type* saved_expected = expected_type_;
        expected_type_ = elem_type;
        elem_pattern->accept(this);
        expected_type_ = saved_expected;
    }
}

void TypeChecker::visit(SlicePattern* node) {
    // Slice模式匹配 - 支持 Array 和 Slice 类型
    
    if (!expected_type_) {
        diag_->reportError(
            "Slice pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    Type* elem_type = nullptr;
    Type* rest_type = nullptr;
    
    // 支持 Array 和 Slice 两种类型
    if (expected_type_->getKind() == Type::Kind::Array) {
        auto* array_type = static_cast<ArrayType*>(expected_type_);
        elem_type = array_type->getElementType();
        
        // 检查前缀数量不超过数组大小
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
        
        // rest 部分是 Slice 类型
        rest_type = types_->getSliceType(elem_type);
        
    } else if (expected_type_->getKind() == Type::Kind::Slice) {
        auto* slice_type = static_cast<SliceType*>(expected_type_);
        elem_type = slice_type->getElementType();
        rest_type = slice_type;  // rest 也是 Slice 类型
        
    } else {
        diag_->reportError(
            "Cannot match slice pattern against non-array/slice type: " + 
            expected_type_->toString(),
            SourceLocation()
        );
        return;
    }
    
    // 检查前缀元素
    for (const auto& prefix_pattern : node->getPrefix()) {
        Type* saved_expected = expected_type_;
        expected_type_ = elem_type;
        prefix_pattern->accept(this);
        expected_type_ = saved_expected;
    }
    
    // 检查后缀元素
    for (const auto& suffix_pattern : node->getSuffix()) {
        Type* saved_expected = expected_type_;
        expected_type_ = elem_type;
        suffix_pattern->accept(this);
        expected_type_ = saved_expected;
    }
    
    // 检查数组大小是否足够（如果是 Array 类型）
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
    
    // 检查 rest 部分
    if (node->hasRest()) {
        Type* saved_expected = expected_type_;
        expected_type_ = rest_type;
        node->getRest()->accept(this);
        expected_type_ = saved_expected;
    }
}

void TypeChecker::visit(RangePattern* node) {
    // 范围模式：验证起始和结束值类型一致
    
    if (!expected_type_) {
        diag_->reportError(
            "Range pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    // 范围模式只支持整数和字符类型
    if (!expected_type_->isInteger() && expected_type_->getKind() != Type::Kind::Char) {
        diag_->reportError(
            "Range pattern only supports integer and char types, got: " + 
            expected_type_->toString(),
            SourceLocation()
        );
        return;
    }
    
    // 检查起始和结束模式
    if (node->getStart()) {
        Type* saved = expected_type_;
        expected_type_ = expected_type_;
        node->getStart()->accept(this);
        expected_type_ = saved;
    }
    
    if (node->getEnd()) {
        Type* saved = expected_type_;
        expected_type_ = expected_type_;
        node->getEnd()->accept(this);
        expected_type_ = saved;
    }
}

void TypeChecker::visit(OrPattern* node) {
    // OR模式：所有分支必须类型一致
    
    if (!expected_type_) {
        diag_->reportError(
            "Or pattern requires a type context",
            SourceLocation()
        );
        return;
    }
    
    // 检查每个分支
    for (const auto& alt : node->getAlternatives()) {
        Type* saved = expected_type_;
        expected_type_ = expected_type_;
        alt->accept(this);
        expected_type_ = saved;
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

// 泛型函数返回类型实例化
Type* TypeChecker::instantiateFunctionReturnType(GenericTemplate* tmpl, 
                                                  const std::vector<Type*>& type_args) {
    if (!tmpl || tmpl->kind != GenericTemplate::FUNCTION) {
        return nullptr;
    }
    
    FunctionDecl* func_def = tmpl->func_def;
    
    // 创建类型替换映射
    std::unordered_map<std::string, Type*> substitution;
    for (size_t i = 0; i < tmpl->type_params.size() && i < type_args.size(); i++) {
        substitution[tmpl->type_params[i]] = type_args[i];
    }
    
    // 替换返回类型中的类型参数
    return types_->substituteType(func_def->getReturnType(), substitution);
}

} // namespace pawc
