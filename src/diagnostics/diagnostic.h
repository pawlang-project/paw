/**
 * @file diagnostic.h
 * @brief Diagnostic message system for PawLang compiler
 * 
 * Provides unified error, warning, and note reporting with source location tracking.
 * 
 * @version 0.2.3
 * @date 2025-10-27
 */

#ifndef PAWC_DIAGNOSTIC_H
#define PAWC_DIAGNOSTIC_H

#include "pawc/common.h"
#include <string>
#include <vector>
#include <memory>

namespace pawc {

/**
 * 诊断级别
 */
enum class DiagnosticLevel {
    Note,       // 提示信息（蓝色）
    Warning,    // 警告（黄色）
    Error,      // 错误（红色）
    Fatal       // 致命错误（红色加粗）
};

/**
 * 诊断位置（扩展 SourceLocation 添加长度信息）
 */
struct DiagnosticLocation : public SourceLocation {
    int length;            // 标记长度（用于下划线显示）
    
    DiagnosticLocation() 
        : SourceLocation(), length(0) {}
    
    DiagnosticLocation(const std::string& f, int l, int c, int len = 1)
        : SourceLocation(f, l, c), length(len) {}
    
    // 从 SourceLocation 转换
    DiagnosticLocation(const SourceLocation& loc, int len = 1)
        : SourceLocation(loc), length(len) {}
    
    bool isValid() const {
        return line > 0 && column > 0;
    }
};

/**
 * 诊断附加说明
 */
struct DiagnosticNote {
    std::string message;
    DiagnosticLocation location;
    
    DiagnosticNote(const std::string& msg, const DiagnosticLocation& loc)
        : message(msg), location(loc) {}
};

/**
 * 诊断消息
 */
class Diagnostic {
public:
    Diagnostic(DiagnosticLevel level, 
              const std::string& message,
              const DiagnosticLocation& location)
        : level_(level), message_(message), location_(location) {}
    
    /**
     * 添加附加说明
     */
    void addNote(const std::string& note, const DiagnosticLocation& loc) {
        notes_.emplace_back(note, loc);
    }
    
    /**
     * 获取诊断级别
     */
    DiagnosticLevel getLevel() const { return level_; }
    
    /**
     * 获取消息
     */
    const std::string& getMessage() const { return message_; }
    
    /**
     * 获取位置
     */
    const DiagnosticLocation& getLocation() const { return location_; }
    
    /**
     * 获取所有附加说明
     */
    const std::vector<DiagnosticNote>& getNotes() const { return notes_; }
    
private:
    DiagnosticLevel level_;
    std::string message_;
    DiagnosticLocation location_;
    std::vector<DiagnosticNote> notes_;
};

} // namespace pawc

#endif // PAWC_DIAGNOSTIC_H

