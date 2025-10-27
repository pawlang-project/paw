/**
 * @file compiler_pass.h
 * @brief 编译器 Pass 基类和编译单元定义
 * 
 * 提供 Pass-Based 编译架构的基础抽象。
 * 
 * @version 0.3.0-dev (Phase 4)
 * @date 2025-10-27
 */

#ifndef PAWC_COMPILER_PASS_H
#define PAWC_COMPILER_PASS_H

#include "../parser/ast.h"
#include "../types/type_system.h"
#include "../diagnostics/diagnostic_engine.h"
#include "../module/symbol_table.h"
#include "llvm/IR/Module.h"
#include <string>

namespace pawc {

// 前向声明
class CodeGenerator;

/**
 * 编译单元
 * 
 * 包含编译过程中的所有上下文信息。
 * Pass 之间通过 CompilationUnit 传递数据。
 */
struct CompilationUnit {
    std::string filename;                    ///< 源文件名
    Program* program;                        ///< AST 程序节点
    TypeSystem* type_system;                 ///< 类型系统
    DiagnosticEngine* diagnostics;           ///< 诊断引擎
    SymbolTable* symbol_table;               ///< 符号表（可选）
    llvm::Module* ir_module;                 ///< 生成的 LLVM IR（CodeGen 后）
    CodeGenerator* code_generator;           ///< CodeGenerator 实例
    
    CompilationUnit(const std::string& file,
                   Program* prog,
                   TypeSystem* ts,
                   DiagnosticEngine* diag,
                   SymbolTable* symtab = nullptr)
        : filename(file),
          program(prog),
          type_system(ts),
          diagnostics(diag),
          symbol_table(symtab),
          ir_module(nullptr),
          code_generator(nullptr) {}
};

/**
 * 编译器 Pass 基类
 * 
 * 所有编译阶段都实现此接口。
 * Pass 可以是分析型（只读）或转换型（修改 AST/IR）。
 */
class CompilerPass {
public:
    virtual ~CompilerPass() = default;
    
    /**
     * 运行 Pass
     * 
     * @param unit 编译单元（包含所有上下文）
     * @return true 成功，false 失败
     */
    virtual bool run(CompilationUnit* unit) = 0;
    
    /**
     * 获取 Pass 名称
     * 
     * @return Pass 名称（用于日志和调试）
     */
    virtual const char* getName() const = 0;
    
    /**
     * 是否是分析 Pass（不修改 AST/IR）
     * 
     * @return true 是分析 Pass，false 是转换 Pass
     */
    virtual bool isAnalysisPass() const { return false; }
};

} // namespace pawc

#endif // PAWC_COMPILER_PASS_H

