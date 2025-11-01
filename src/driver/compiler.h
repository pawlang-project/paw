//===--- compiler.h - Compiler Main Class -----------------------*- C++ -*-===//
//
// PawLang Compiler - Main Compiler Class
//
//===----------------------------------------------------------------------===//

#ifndef PAW_COMPILER_H
#define PAW_COMPILER_H

#include "options.h"
#include "diagnostics/diagnostic_engine.h"
#include "middleend/types/type_system.h"
#include "middleend/symbol/symbol_table.h"
#include "pass/pass_manager.h"
#include "pass/pass_context.h"

#include <memory>
#include <string>

namespace pawc {

/// Compiler - 编译器主类
///
/// 负责协调整个编译流程，使用PassManager管理所有Pass
class Compiler {
public:
    explicit Compiler(const CompilerOptions& options);
    ~Compiler();
    
    /// 编译源文件
    /// \param source_file 源文件路径
    /// \return true表示成功
    bool compile(const std::string& source_file);
    
    /// 获取诊断引擎
    DiagnosticEngine* getDiagnostics() { return diagnostics_.get(); }
    
    /// 获取类型系统
    TypeSystem* getTypeSystem() { return type_system_.get(); }
    
    /// 获取符号表
    SymbolTable* getSymbolTable() { return symbol_table_.get(); }
    
private:
    CompilerOptions options_;
    
    // 核心组件
    std::unique_ptr<DiagnosticEngine> diagnostics_;
    std::unique_ptr<TypeSystem> type_system_;
    std::unique_ptr<SymbolTable> symbol_table_;
    std::unique_ptr<PassContext> pass_context_;
    std::unique_ptr<PassManager> pass_manager_;
    
    // 初始化编译器组件
    void initialize();
    
    // 配置Pass流程
    void setupPasses();
    
    // 读取源文件
    std::string readSourceFile(const std::string& filename);
    
    // 处理编译结果
    bool processResults();
};

} // namespace pawc

#endif // PAW_COMPILER_H
