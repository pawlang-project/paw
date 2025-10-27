/**
 * @file semantic_pass.h
 * @brief 语义分析 Pass
 * 
 * @version 0.3.0-dev (Phase 4)
 * @date 2025-10-27
 */

#ifndef PAWC_SEMANTIC_PASS_H
#define PAWC_SEMANTIC_PASS_H

#include "compiler_pass.h"
#include "../sema/semantic_analyzer.h"

namespace pawc {

/**
 * 语义分析 Pass
 * 
 * 执行语义分析，包括：
 * - 接口实现验证
 * - 类型检查（未来）
 * - 名称解析（未来）
 */
class SemanticAnalysisPass : public CompilerPass {
public:
    bool run(CompilationUnit* unit) override {
        SemanticAnalyzer sema(
            unit->type_system,
            unit->diagnostics,
            unit->symbol_table
        );
        return sema.analyze(*unit->program);
    }
    
    const char* getName() const override {
        return "SemanticAnalysis";
    }
    
    bool isAnalysisPass() const override { return true; }
};

} // namespace pawc

#endif // PAWC_SEMANTIC_PASS_H

