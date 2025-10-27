/**
 * @file semantic_analyzer.cpp
 * @brief 语义分析器实现
 */

#include "semantic_analyzer.h"

namespace pawc {

SemanticAnalyzer::SemanticAnalyzer(
    TypeSystem* type_system,
    DiagnosticEngine* diagnostics,
    SymbolTable* symbol_table)
    : type_system_(type_system),
      diagnostics_(diagnostics),
      symbol_table_(symbol_table) {}

bool SemanticAnalyzer::analyze(const Program& program) {
    // 第一遍：收集所有定义
    collectDefinitions(program);
    
    // 创建 InterfaceValidator
    interface_validator_ = std::make_unique<InterfaceValidator>(
        type_system_,
        diagnostics_,
        interface_defs_
    );
    
    // 第二遍：验证语义
    return validateSemantics(program);
}

void SemanticAnalyzer::collectDefinitions(const Program& program) {
    for (const auto& stmt : program.statements) {
        if (stmt->kind == Stmt::Kind::Interface) {
            const auto* interface_stmt = static_cast<const InterfaceStmt*>(stmt.get());
            interface_defs_[interface_stmt->name] = interface_stmt;
        }
    }
}

bool SemanticAnalyzer::validateSemantics(const Program& program) {
    bool all_valid = true;
    
    for (const auto& stmt : program.statements) {
        if (!analyzeStmt(stmt.get())) {
            all_valid = false;
        }
    }
    
    return all_valid;
}

bool SemanticAnalyzer::analyzeStmt(const Stmt* stmt) {
    switch (stmt->kind) {
        case Stmt::Kind::Struct:
            return analyzeStructStmt(static_cast<const StructStmt*>(stmt));
        
        case Stmt::Kind::Interface:
            return analyzeInterfaceStmt(static_cast<const InterfaceStmt*>(stmt));
        
        case Stmt::Kind::Support:
            return analyzeSupportStmt(static_cast<const SupportStmt*>(stmt));
        
        // 其他语句类型暂时不需要验证
        default:
            return true;
    }
}

bool SemanticAnalyzer::analyzeStructStmt(const StructStmt* stmt) {
    bool all_valid = true;
    
    // 验证所有内联接口实现
    for (const auto& interface_name : stmt->interfaces) {
        if (!interface_validator_->validateImpl(
            stmt->name,
            interface_name,
            stmt->methods,
            stmt->location)) {
            all_valid = false;
        }
    }
    
    return all_valid;
}

bool SemanticAnalyzer::analyzeInterfaceStmt(const InterfaceStmt* stmt) {
    // Interface 定义本身无需验证
    return true;
}

bool SemanticAnalyzer::analyzeSupportStmt(const SupportStmt* stmt) {
    // 验证外联接口实现
    return interface_validator_->validateImpl(
        stmt->type_name,
        stmt->interface_name,
        stmt->methods,
        stmt->location
    );
}

} // namespace pawc

