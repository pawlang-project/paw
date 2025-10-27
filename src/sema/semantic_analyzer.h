/**
 * @file semantic_analyzer.h
 * @brief 语义分析器
 * 
 * 协调所有语义分析任务：类型检查、接口验证等。
 * 
 * @version 0.3.0-dev (Phase 3)
 * @date 2025-10-27
 */

#ifndef PAWC_SEMANTIC_ANALYZER_H
#define PAWC_SEMANTIC_ANALYZER_H

#include "../parser/ast.h"
#include "../types/type_system.h"
#include "../diagnostics/diagnostic_engine.h"
#include "../module/symbol_table.h"
#include "interface_validator.h"
#include <memory>

namespace pawc {

/**
 * 语义分析器
 * 
 * 在 Parser 和 CodeGen 之间的中间阶段。
 * 负责所有语义验证：
 * - 接口实现验证
 * - 类型检查（未来）
 * - 名称解析（未来）
 * - 借用检查（未来）
 */
class SemanticAnalyzer {
public:
    SemanticAnalyzer(TypeSystem* type_system,
                    DiagnosticEngine* diagnostics,
                    SymbolTable* symbol_table = nullptr);
    
    /**
     * 分析整个程序
     * 
     * @param program AST 程序节点
     * @return 是否通过所有语义检查
     */
    bool analyze(const Program& program);
    
private:
    TypeSystem* type_system_;
    DiagnosticEngine* diagnostics_;
    SymbolTable* symbol_table_;
    
    std::unique_ptr<InterfaceValidator> interface_validator_;
    
    // 接口定义映射（收集后传给 InterfaceValidator）
    std::map<std::string, const InterfaceStmt*> interface_defs_;
    
    /**
     * 第一遍：收集所有定义
     */
    void collectDefinitions(const Program& program);
    
    /**
     * 第二遍：验证语义
     */
    bool validateSemantics(const Program& program);
    
    /**
     * 分析单个语句
     */
    bool analyzeStmt(const Stmt* stmt);
    
    /**
     * 分析 Struct 定义
     */
    bool analyzeStructStmt(const StructStmt* stmt);
    
    /**
     * 分析 Interface 定义
     */
    bool analyzeInterfaceStmt(const InterfaceStmt* stmt);
    
    /**
     * 分析 Support 块
     */
    bool analyzeSupportStmt(const SupportStmt* stmt);
};

} // namespace pawc

#endif // PAWC_SEMANTIC_ANALYZER_H

