/**
 * @file diagnostic_engine.cpp
 * @brief Implementation of diagnostic engine
 */

#include "diagnostic_engine.h"
#include <iomanip>
#include <sstream>
#include <cstdlib>

namespace pawc {

// ANSI 颜色代码
namespace Color {
    const char* Reset = "\033[0m";
    const char* Bold = "\033[1m";
    const char* Red = "\033[31m";
    const char* Yellow = "\033[33m";
    const char* Blue = "\033[34m";
    const char* Cyan = "\033[36m";
    const char* BoldRed = "\033[1;31m";
    const char* BoldYellow = "\033[1;33m";
    const char* BoldBlue = "\033[1;34m";
}

void DiagnosticEngine::reportError(const std::string& message, const DiagnosticLocation& location) {
    Diagnostic diag(DiagnosticLevel::Error, message, location);
    addDiagnostic(diag);
}

void DiagnosticEngine::reportWarning(const std::string& message, const DiagnosticLocation& location) {
    Diagnostic diag(DiagnosticLevel::Warning, message, location);
    addDiagnostic(diag);
}

void DiagnosticEngine::reportNote(const std::string& message, const DiagnosticLocation& location) {
    Diagnostic diag(DiagnosticLevel::Note, message, location);
    addDiagnostic(diag);
}

void DiagnosticEngine::reportFatal(const std::string& message, const DiagnosticLocation& location) {
    Diagnostic diag(DiagnosticLevel::Fatal, message, location);
    formatDiagnostic(diag);
    std::exit(1);
}

void DiagnosticEngine::addDiagnostic(const Diagnostic& diag) {
    if (diag.getLevel() == DiagnosticLevel::Error || 
        diag.getLevel() == DiagnosticLevel::Fatal) {
        error_count_++;
    } else if (diag.getLevel() == DiagnosticLevel::Warning) {
        warning_count_++;
    }
    
    diagnostics_.push_back(diag);
    
    // 立即打印（以便用户实时看到错误）
    formatDiagnostic(diag);
}

void DiagnosticEngine::printAll() const {
    for (const auto& diag : diagnostics_) {
        formatDiagnostic(diag);
    }
}

std::string DiagnosticEngine::getLevelName(DiagnosticLevel level) const {
    const char* color = use_color_ ? getLevelColor(level) : "";
    const char* reset = use_color_ ? Color::Reset : "";
    
    switch (level) {
        case DiagnosticLevel::Note:
            return std::string(color) + "note" + reset;
        case DiagnosticLevel::Warning:
            return std::string(color) + "warning" + reset;
        case DiagnosticLevel::Error:
            return std::string(color) + "error" + reset;
        case DiagnosticLevel::Fatal:
            return std::string(color) + "fatal error" + reset;
    }
    return "unknown";
}

const char* DiagnosticEngine::getLevelColor(DiagnosticLevel level) const {
    switch (level) {
        case DiagnosticLevel::Note:
            return Color::BoldBlue;
        case DiagnosticLevel::Warning:
            return Color::BoldYellow;
        case DiagnosticLevel::Error:
        case DiagnosticLevel::Fatal:
            return Color::BoldRed;
    }
    return Color::Reset;
}

void DiagnosticEngine::formatDiagnostic(const Diagnostic& diag) const {
    const DiagnosticLocation& loc = diag.getLocation();
    
    // 1. 打印主要错误信息
    if (loc.isValid()) {
        out_ << getLevelName(diag.getLevel()) << ": " 
             << diag.getMessage() << "\n";
        
        // 2. 打印位置信息
        const char* cyan = use_color_ ? Color::Cyan : "";
        const char* reset = use_color_ ? Color::Reset : "";
        
        out_ << " " << cyan << "-->" << reset 
             << " " << loc.filename << ":" << loc.line << ":" << loc.column << "\n";
        
        // 3. 打印源码行
        if (source_manager_) {
            std::string source_line = source_manager_->getSourceLine(loc.filename, loc.line);
            if (!source_line.empty()) {
                // 行号宽度
                int line_width = std::to_string(loc.line).length();
                
                // 打印空白行
                out_ << std::string(line_width + 1, ' ') << cyan << "|" << reset << "\n";
                
                // 打印行号和源码
                out_ << cyan << std::setw(line_width) << loc.line << " |" << reset 
                     << " " << source_line << "\n";
                
                // 打印指示符（^^^）
                std::string indicator_line(line_width + 1, ' ');
                indicator_line += cyan;
                indicator_line += "|";
                indicator_line += reset;
                indicator_line += std::string(loc.column, ' ');
                
                const char* indicator_color = use_color_ ? getLevelColor(diag.getLevel()) : "";
                indicator_line += indicator_color;
                
                // 使用 ^ 指示错误位置
                int length = (loc.length > 0) ? loc.length : 1;
                indicator_line += std::string(length, '^');
                indicator_line += reset;
                
                out_ << indicator_line << "\n";
            }
        }
    } else {
        // 无位置信息的错误
        out_ << getLevelName(diag.getLevel()) << ": " 
             << diag.getMessage() << "\n";
    }
    
    // 4. 打印附加说明
    for (const auto& note : diag.getNotes()) {
        out_ << "\n";
        const char* blue = use_color_ ? Color::BoldBlue : "";
        const char* reset = use_color_ ? Color::Reset : "";
        
        out_ << blue << "note" << reset << ": " << note.message << "\n";
        
        if (note.location.isValid()) {
            const char* cyan = use_color_ ? Color::Cyan : "";
            out_ << " " << cyan << "-->" << reset 
                 << " " << note.location.filename << ":" 
                 << note.location.line << ":" << note.location.column << "\n";
        }
    }
    
    out_ << "\n";
}

} // namespace pawc

