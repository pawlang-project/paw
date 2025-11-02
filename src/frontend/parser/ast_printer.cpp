//===--- ast_printer.cpp - AST Pretty Printer Implementation ----*- C++ -*-===//

#include "ast_printer.h"
#include "frontend/parser/ast/pattern.h"
#include <cstdio>
#include <sstream>

namespace pawc {

void ASTPrinter::print(ASTNode* node) {
    if (node) {
        node->accept(this);
    }
}

void ASTPrinter::printIndent() {
    for (int i = 0; i < indent_; ++i) {
        os_ << "  ";
    }
}

void ASTPrinter::printLine(const std::string& text) {
    printIndent();
    os_ << text << "\n";
}

std::string ASTPrinter::getTypeString(Type* type) {
    if (!type) return "<unknown>";
    
    switch (type->getKind()) {
        case Type::Kind::I8: return "i8";
        case Type::Kind::I16: return "i16";
        case Type::Kind::I32: return "i32";
        case Type::Kind::I64: return "i64";
        case Type::Kind::I128: return "i128";
        case Type::Kind::U8: return "u8";
        case Type::Kind::U16: return "u16";
        case Type::Kind::U32: return "u32";
        case Type::Kind::U64: return "u64";
        case Type::Kind::U128: return "u128";
        case Type::Kind::F8: return "f8";
        case Type::Kind::F16: return "f16";
        case Type::Kind::F32: return "f32";
        case Type::Kind::F64: return "f64";
        case Type::Kind::F128: return "f128";
        case Type::Kind::Bool: return "bool";
        case Type::Kind::Char: return "char";
        case Type::Kind::String: return "string";
        case Type::Kind::Void: return "void";
        default: return "<complex>";
    }
}

std::string ASTPrinter::getOpString(TokenType op) {
    switch (op) {
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::PERCENT: return "%";
        case TokenType::EQ_EQ: return "==";
        case TokenType::NOT_EQ: return "!=";
        case TokenType::LT: return "<";
        case TokenType::LESS_EQ: return "<=";
        case TokenType::GT: return ">";
        case TokenType::GREATER_EQ: return ">=";
        case TokenType::AND_AND: return "&&";
        case TokenType::OR_OR: return "||";
        case TokenType::BANG: return "!";
        default: return "?";
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 表达式
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ASTPrinter::visit(IntLiteral* node) {
    printLine("IntLiteral: " + node->getValue() + 
              " : " + getTypeString(node->getType()));
}

void ASTPrinter::visit(FloatLiteral* node) {
    printLine("FloatLiteral: " + node->getValue() + 
              " : " + getTypeString(node->getType()));
}

void ASTPrinter::visit(BoolLiteral* node) {
    printLine("BoolLiteral: " + std::string(node->getValue() ? "true" : "false") + 
              " : " + getTypeString(node->getType()));
}

void ASTPrinter::visit(CharLiteral* node) {
    printLine("CharLiteral: '" + std::string(1, node->getValue()) + "' : " + 
              getTypeString(node->getType()));
}

void ASTPrinter::visit(StringLiteral* node) {
    printLine("StringLiteral: \"" + node->getValue() + "\" : " + 
              getTypeString(node->getType()));
}

void ASTPrinter::visit(NullLiteral* node) {
    printLine("NullLiteral : " + getTypeString(node->getType()));
}

void ASTPrinter::visit(CastExpr* node) {
    printLine("CastExpr (as " + (node->getTargetType() ? node->getTargetType()->toString() : "unknown") + ") : " + getTypeString(node->getType()));
    indent_ += 1;
    if (node->getExpr()) node->getExpr()->accept(this);
    indent_ -= 1;
}

void ASTPrinter::visit(IdentifierExpr* node) {
    printLine("IdentifierExpr: " + node->getName() + " : " + 
              getTypeString(node->getType()));
}

void ASTPrinter::visit(SelfExpr* node) {
    printLine("SelfExpr: self : " + getTypeString(node->getType()));
}

void ASTPrinter::visit(BinaryExpr* node) {
    printLine("BinaryExpr: " + getOpString(node->getOperator()) + " : " + 
              getTypeString(node->getType()));
    indent_ += 1;
    printLine("left:");
    indent_ += 1;
    node->getLeft()->accept(this);
    indent_ -= 1;
    printLine("right:");
    indent_ += 1;
    node->getRight()->accept(this);
    indent_ -= 2;
}

void ASTPrinter::visit(UnaryExpr* node) {
    printLine("UnaryExpr: " + getOpString(node->getOperator()) + " : " + 
              getTypeString(node->getType()));
    indent_ += 1;
    node->getOperand()->accept(this);
    indent_ -= 1;
}

void ASTPrinter::visit(CallExpr* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getArgs().size());
    printLine("CallExpr: " + getTypeString(node->getType()));
    indent_ += 1;
    printLine("callee:");
    indent_ += 1;
    node->getCallee()->accept(this);
    indent_ -= 1;
    printLine("args: (" + std::string(buf) + ")");
    indent_ += 1;
    for (const auto& arg : node->getArgs()) {
        arg->accept(this);
    }
    indent_ -= 2;
}

void ASTPrinter::visit(StaticAccessExpr* node) {
    printLine("StaticAccessExpr: " + node->getTypeName() + "::" + 
              node->getMember() + " : " + getTypeString(node->getType()));
}

void ASTPrinter::visit(MemberExpr* node) {
    printLine("MemberExpr: ." + node->getMember() + " : " + 
              getTypeString(node->getType()));
    indent_ += 1;
    node->getObject()->accept(this);
    indent_ -= 1;
}

void ASTPrinter::visit(IndexExpr* node) {
    printLine("IndexExpr: [] : " + getTypeString(node->getType()));
    indent_ += 1;
    printLine("object:");
    indent_ += 1;
    node->getObject()->accept(this);
    indent_ -= 1;
    printLine("index:");
    indent_ += 1;
    node->getIndex()->accept(this);
    indent_ -= 2;
}

void ASTPrinter::visit(IfExpr* node) {
    printLine("IfExpr: " + getTypeString(node->getType()));
    indent_ += 1;
    printLine("condition:");
    indent_ += 1;
    node->getCondition()->accept(this);
    indent_ -= 1;
    printLine("then:");
    indent_ += 1;
    node->getThenExpr()->accept(this);
    indent_ -= 1;
    if (node->getElseExpr()) {
        printLine("else:");
        indent_ += 1;
        node->getElseExpr()->accept(this);
        indent_ -= 1;
    }
    indent_ -= 1;
}

void ASTPrinter::visit(BlockExpr* node) {
    printLine("BlockExpr: " + getTypeString(node->getType()));
    indent_ += 1;
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
    indent_ -= 1;
}

void ASTPrinter::visit(ArrayLiteral* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getElements().size());
    printLine("ArrayLiteral: [" + std::string(buf) + " elements] : " + 
              getTypeString(node->getType()));
    indent_ += 1;
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
    indent_ -= 1;
}

void ASTPrinter::visit(TupleExpr* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getElements().size());
    printLine("TupleExpr: (" + std::string(buf) + " elements) : " + 
              getTypeString(node->getType()));
    indent_ += 1;
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
    indent_ -= 1;
}

void ASTPrinter::visit(RangeExpr* node) {
    printLine("RangeExpr: " + std::string(node->isInclusive() ? "..=" : "..") + 
              " : " + getTypeString(node->getType()));
    indent_ += 1;
    if (node->getStart()) {
        printLine("start:");
        indent_ += 1;
        node->getStart()->accept(this);
        indent_ -= 1;
    }
    if (node->getEnd()) {
        printLine("end:");
        indent_ += 1;
        node->getEnd()->accept(this);
        indent_ -= 1;
    }
    indent_ -= 1;
}

void ASTPrinter::visit(StructLiteral* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getFields().size());
    printLine("StructLiteral: " + node->getStructName() + 
              " {" + std::string(buf) + " fields} : " + 
              getTypeString(node->getType()));
    indent_ += 1;
    for (const auto& field : node->getFields()) {
        printLine("field: " + field.name);
        indent_ += 1;
        field.value->accept(this);
        indent_ -= 1;
    }
    indent_ -= 1;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 语句
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ASTPrinter::visit(ExprStmt* node) {
    printLine("ExprStmt:");
    indent_ += 1;
    if (node->getExpr()) {
        node->getExpr()->accept(this);
    }
    indent_ -= 1;
}

void ASTPrinter::visit(VarDecl* node) {
    printLine("VarDecl: " + node->getName() + " : " + 
              getTypeString(node->getType()) + 
              (node->isMutable() ? " (mut)" : ""));
    if (node->getInit()) {
        indent_ += 1;
        printLine("init:");
        indent_ += 1;
        node->getInit()->accept(this);
        indent_ -= 2;
    }
}

void ASTPrinter::visit(DestructuringDecl* node) {
    std::string names_str = "(";
    for (size_t i = 0; i < node->getNames().size(); ++i) {
        if (i > 0) names_str += ", ";
        names_str += node->getNames()[i];
    }
    names_str += ")";
    printLine("DestructuringDecl: " + names_str + (node->isMutable() ? " (mut)" : ""));
    if (node->getInit()) {
        indent_ += 1;
        printLine("init:");
        indent_ += 1;
        node->getInit()->accept(this);
        indent_ -= 2;
    }
}

void ASTPrinter::visit(StructDestructuringDecl* node) {
    std::string fields_str = node->getStructName() + " { ";
    for (size_t i = 0; i < node->getFieldNames().size(); ++i) {
        if (i > 0) fields_str += ", ";
        fields_str += node->getFieldNames()[i];
    }
    fields_str += " }";
    printLine("StructDestructuringDecl: " + fields_str + (node->isMutable() ? " (mut)" : ""));
    if (node->getInit()) {
        indent_ += 1;
        printLine("init:");
        indent_ += 1;
        node->getInit()->accept(this);
        indent_ -= 2;
    }
}

void ASTPrinter::visit(FunctionDecl* node) {
    printLine("FunctionDecl: " + node->getName() + " : " + 
              getTypeString(node->getReturnType()));
    indent_ += 1;
    
    if (!node->getParams().empty()) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%zu", node->getParams().size());
        printLine("params: (" + std::string(buf) + ")");
        indent_ += 1;
        for (const auto& param : node->getParams()) {
            printLine(param.name + " : " + getTypeString(param.type));
        }
        indent_ -= 1;
    }
    
    if (node->getBody()) {
        printLine("body:");
        indent_ += 1;
        node->getBody()->accept(this);
        indent_ -= 1;
    }
    
    indent_ -= 1;
}

void ASTPrinter::visit(ReturnStmt* node) {
    printLine("ReturnStmt:");
    if (node->getValue()) {
        indent_ += 1;
        node->getValue()->accept(this);
        indent_ -= 1;
    }
}

void ASTPrinter::visit(IfStmt* node) {
    printLine("IfStmt:");
    indent_ += 1;
    printLine("condition:");
    indent_ += 1;
    node->getCondition()->accept(this);
    indent_ -= 1;
    printLine("then:");
    indent_ += 1;
    node->getThenStmt()->accept(this);
    indent_ -= 1;
    if (node->getElseStmt()) {
        printLine("else:");
        indent_ += 1;
        node->getElseStmt()->accept(this);
        indent_ -= 1;
    }
    indent_ -= 1;
}

void ASTPrinter::visit(LoopStmt* node) {
    printLine("LoopStmt:");
    if (node->getBody()) {
        indent_ += 1;
        node->getBody()->accept(this);
        indent_ -= 1;
    }
}

void ASTPrinter::visit(WhileStmt* node) {
    printLine("WhileStmt:");
    indent_ += 1;
    printLine("condition:");
    indent_ += 1;
    node->getCondition()->accept(this);
    indent_ -= 1;
    printLine("body:");
    indent_ += 1;
    node->getBody()->accept(this);
    indent_ -= 2;
}

void ASTPrinter::visit(BreakStmt* node) {
    printLine("BreakStmt");
}

void ASTPrinter::visit(ContinueStmt* node) {
    printLine("ContinueStmt");
}

void ASTPrinter::visit(BlockStmt* node) {
    printLine("BlockStmt: {");
    indent_ += 1;
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
    indent_ -= 1;
    printLine("}");
}

void ASTPrinter::visit(ForStmt* node) {
    printLine("ForStmt: loop " + node->getVarName() + " in");
    indent_ += 1;
    printLine("iterator:");
    indent_ += 1;
    node->getIterator()->accept(this);
    indent_ -= 1;
    printLine("body:");
    indent_ += 1;
    node->getBody()->accept(this);
    indent_ -= 2;
}

void ASTPrinter::visit(StructDecl* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getFields().size());
    printLine("StructDecl: type " + node->getName() + " = struct {" + 
              std::string(buf) + " fields}");
    indent_ += 1;
    for (const auto& [field_name, field_type] : node->getFields()) {
        printLine("field: " + field_name + " : " + 
                  (field_type ? field_type->toString() : "<unknown>"));
    }
    indent_ -= 1;
}

void ASTPrinter::visit(EnumDecl* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getVariants().size());
    printLine("EnumDecl: type " + node->getName() + " = enum {" + 
              std::string(buf) + " variants}");
    indent_ += 1;
    for (const auto& variant : node->getVariants()) {
        std::string var_str = "variant: " + variant.name;
        if (variant.data_type) {
            var_str += "(" + variant.data_type->toString() + ")";
        }
        printLine(var_str);
    }
    indent_ -= 1;
}

void ASTPrinter::visit(MatchExpr* node) {
    printLine("MatchExpr:");
    indent_ += 1;
    printLine("scrutinee:");
    indent_ += 1;
    node->getScrutinee()->accept(this);
    indent_ -= 1;
    printLine("arms:");
    indent_ += 1;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getArms().size());
    printLine(std::string(buf) + " arms");
    // TODO: 遍历打印每个arm
    indent_ -= 2;
}

void ASTPrinter::visit(ClosureExpr* node) {
    printLine("ClosureExpr:");
    indent_ += 2;
    
    // 打印参数
    std::stringstream params_ss;
    params_ss << "Params: [";
    bool first = true;
    for (const auto& param : node->getParams()) {
        if (!first) params_ss << ", ";
        params_ss << param.name << ": ";
        if (param.type) {
            params_ss << param.type->toString();
        } else {
            params_ss << "?";
        }
        first = false;
    }
    params_ss << "]";
    printLine(params_ss.str());
    
    // 打印返回类型
    std::string return_str = "ReturnType: ";
    if (node->getReturnType()) {
        return_str += node->getReturnType()->toString();
    } else {
        return_str += "(inferred)";
    }
    printLine(return_str);
    
    // 打印捕获变量
    if (!node->getCapturedVars().empty()) {
        std::stringstream captured_ss;
        captured_ss << "Captured: [";
        first = true;
        for (const auto& captured : node->getCapturedVars()) {
            if (!first) captured_ss << ", ";
            captured_ss << captured.name << ": " << captured.type->toString();
            if (captured.by_reference) {
                captured_ss << " (by ref)";
            }
            first = false;
        }
        captured_ss << "]";
        printLine(captured_ss.str());
    }
    
    // 打印闭包体
    printLine("Body:");
    indent_ += 2;
    node->getBody()->accept(this);
    indent_ -= 2;
    
    indent_ -= 2;
}

void ASTPrinter::visit(TryExpr* node) {
    printLine("TryExpr:");
    indent_ += 1;
    node->getExpr()->accept(this);
    indent_ -= 1;
}

// Pattern打印
void ASTPrinter::visit(LiteralPattern* node) {
    printLine("LiteralPattern: " + node->getValue());
}

void ASTPrinter::visit(WildcardPattern* node) {
    printLine("WildcardPattern: _");
}

void ASTPrinter::visit(VariablePattern* node) {
    printLine("VariablePattern: " + node->getName());
}

void ASTPrinter::visit(TuplePattern* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getElements().size());
    printLine("TuplePattern: " + std::string(buf) + " elements");
}

void ASTPrinter::visit(EnumPattern* node) {
    printLine("EnumPattern: " + node->getVariantName());
}

void ASTPrinter::visit(InterfaceDecl* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getMethods().size());
    printLine("InterfaceDecl: type " + node->getName() + " = interface {" +
              std::string(buf) + " methods}");
    indent_ += 1;
    for (const auto& method : node->getMethods()) {
        std::string method_str = "fn " + method.name + "(";
        char param_buf[16];
        std::snprintf(param_buf, sizeof(param_buf), "%zu", method.params.size());
        method_str += std::string(param_buf) + " params)";
        if (method.return_type) {
            method_str += " -> " + method.return_type->toString();
        }
        printLine(method_str);
    }
    indent_ -= 1;
}

void ASTPrinter::visit(SupportDecl* node) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%zu", node->getMethods().size());
    printLine("SupportDecl: support " + node->getTypeName() + 
              " with " + node->getInterfaceName() + " {" +
              std::string(buf) + " methods}");
    indent_ += 1;
    for (const auto& method : node->getMethods()) {
        printLine("method: fn " + method->getName());
    }
    indent_ -= 1;
}

void ASTPrinter::visit(StructPattern* node) {
    printLine("StructPattern: " + node->getStructName());
    indent_++;
    for (const auto& field : node->getFields()) {
        printLine("Field: " + field.field_name);
        indent_++;
        field.pattern->accept(this);
        indent_--;
    }
    indent_--;
}

} // namespace pawc

