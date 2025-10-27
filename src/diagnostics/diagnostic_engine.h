/**
 * @file diagnostic_engine.h
 * @brief Unified diagnostic reporting engine for PawLang compiler
 * 
 * Central error/warning/note reporting system with beautiful colored output.
 * 
 * @version 0.2.3
 * @date 2025-10-27
 */

#ifndef PAWC_DIAGNOSTIC_ENGINE_H
#define PAWC_DIAGNOSTIC_ENGINE_H

#include "diagnostic.h"
#include "source_manager.h"
#include <iostream>
#include <memory>

namespace pawc {

/**
 * 诊断引擎
 * 
 * 统一管理所有编译错误、警告和提示信息。
 * 特性：
 * - 彩色输出
 * - 源码位置显示
 * - 错误收集（可选择继续编译）
 * - 美观的格式化输出
 */
class DiagnosticEngine {
public:
    DiagnosticEngine(SourceManager* source_manager, std::ostream& out = std::cerr)
        : source_manager_(source_manager), 
          out_(out),
          error_count_(0),
          warning_count_(0) {}
    
    /**
     * 报告错误
     */
    void reportError(const std::string& message, const DiagnosticLocation& location);
    
    /**
     * 报告警告
     */
    void reportWarning(const std::string& message, const DiagnosticLocation& location);
    
    /**
     * 报告提示
     */
    void reportNote(const std::string& message, const DiagnosticLocation& location);
    
    /**
     * 报告致命错误（立即退出）
     */
    void reportFatal(const std::string& message, const DiagnosticLocation& location);
    
    /**
     * 添加诊断
     */
    void addDiagnostic(const Diagnostic& diag);
    
    /**
     * 是否有错误
     */
    bool hasErrors() const { return error_count_ > 0; }
    
    /**
     * 获取错误数量
     */
    int getErrorCount() const { return error_count_; }
    
    /**
     * 获取警告数量
     */
    int getWarningCount() const { return warning_count_; }
    
    /**
     * 打印所有诊断信息
     */
    void printAll() const;
    
    /**
     * 清空所有诊断
     */
    void clear() {
        diagnostics_.clear();
        error_count_ = 0;
        warning_count_ = 0;
    }
    
    /**
     * 设置是否使用颜色
     */
    void setUseColor(bool use_color) {
        use_color_ = use_color;
    }
    
private:
    SourceManager* source_manager_;
    std::ostream& out_;
    std::vector<Diagnostic> diagnostics_;
    int error_count_;
    int warning_count_;
    bool use_color_ = true;
    
    /**
     * 格式化并打印单个诊断
     */
    void formatDiagnostic(const Diagnostic& diag) const;
    
    /**
     * 获取级别名称（带颜色）
     */
    std::string getLevelName(DiagnosticLevel level) const;
    
    /**
     * 获取级别颜色代码
     */
    const char* getLevelColor(DiagnosticLevel level) const;
};

} // namespace pawc

#endif // PAWC_DIAGNOSTIC_ENGINE_H

