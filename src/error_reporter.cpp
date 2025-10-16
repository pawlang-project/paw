#include "pawc/error_reporter.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace pawc {

void ErrorReporter::setSourceCode(const std::string& filename, const std::string& code) {
    std::vector<std::string> lines;
    std::istringstream stream(code);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    source_lines_[filename] = lines;
}

std::string ErrorReporter::getCodeLine(const std::string& filename, int line) {
    auto it = source_lines_.find(filename);
    if (it != source_lines_.end() && line > 0 && line <= static_cast<int>(it->second.size())) {
        return it->second[line - 1];
    }
    return "";
}

std::string ErrorReporter::getLevelPrefix(ErrorLevel level) {
    switch (level) {
        case ErrorLevel::Error:   return "error";
        case ErrorLevel::Warning: return "warning";
        case ErrorLevel::Note:    return "note";
    }
    return "unknown";
}

std::string ErrorReporter::getLevelColor(ErrorLevel level) {
    switch (level) {
        case ErrorLevel::Error:   return Colors::RED;
        case ErrorLevel::Warning: return Colors::YELLOW;
        case ErrorLevel::Note:    return Colors::CYAN;
    }
    return Colors::RESET;
}

void ErrorReporter::reportError(const std::string& message, const SourceLocation& location,
                               const std::vector<ErrorHint>& hints) {
    error_count_++;
    
    DetailedError error(ErrorLevel::Error, message, location);
    error.hints = hints;
    error.code_snippet = getCodeLine(location.filename, location.line);
    
    printError(error);
}

void ErrorReporter::reportWarning(const std::string& message, const SourceLocation& location,
                                 const std::vector<ErrorHint>& hints) {
    warning_count_++;
    
    DetailedError warning(ErrorLevel::Warning, message, location);
    warning.hints = hints;
    warning.code_snippet = getCodeLine(location.filename, location.line);
    
    printError(warning);
}

void ErrorReporter::reportNote(const std::string& message, const SourceLocation& location) {
    DetailedError note(ErrorLevel::Note, message, location);
    if (!location.filename.empty()) {
        note.code_snippet = getCodeLine(location.filename, location.line);
    }
    
    printError(note);
}

void ErrorReporter::printError(const DetailedError& error) {
    std::string level_color = getLevelColor(error.level);
    std::string level_prefix = getLevelPrefix(error.level);
    
    // 打印错误头部
    std::cerr << level_color << Colors::BOLD << level_prefix << ": " << Colors::RESET
              << Colors::BOLD << error.message << Colors::RESET << std::endl;
    
    // 打印位置信息
    if (!error.location.filename.empty()) {
        std::cerr << Colors::BLUE << "  --> " << Colors::RESET
                  << error.location.filename << ":"
                  << error.location.line << ":"
                  << error.location.column << std::endl;
        
        // 打印代码片段
        if (error.code_snippet.has_value() && !error.code_snippet->empty()) {
            int line_num = error.location.line;
            int col_num = error.location.column;
            
            // 行号宽度
            int line_width = std::to_string(line_num).length();
            
            // 空行
            std::cerr << Colors::BLUE << std::string(line_width + 1, ' ') << "|" << Colors::RESET << std::endl;
            
            // 代码行
            std::cerr << Colors::BLUE << std::setw(line_width) << line_num << " | " << Colors::RESET
                      << *error.code_snippet << std::endl;
            
            // 指示符（^）
            std::cerr << Colors::BLUE << std::string(line_width + 1, ' ') << "| " << Colors::RESET
                      << level_color << std::string(col_num - 1, ' ') << "^"
                      << std::string(error.code_snippet->length() - col_num, '~') << Colors::RESET << std::endl;
        }
    }
    
    // 打印提示
    for (const auto& hint : error.hints) {
        std::cerr << Colors::GREEN << "  = help: " << Colors::RESET << hint.message << std::endl;
        
        if (hint.location.has_value() && !hint.location->filename.empty()) {
            std::string hint_line = getCodeLine(hint.location->filename, hint.location->line);
            if (!hint_line.empty()) {
                std::cerr << Colors::BLUE << "  --> " << Colors::RESET
                          << hint.location->filename << ":"
                          << hint.location->line << ":"
                          << hint.location->column << std::endl;
                std::cerr << Colors::BLUE << "   | " << Colors::RESET << hint_line << std::endl;
            }
        }
    }
    
    std::cerr << std::endl;  // 空行分隔
}

void ErrorReporter::clear() {
    error_count_ = 0;
    warning_count_ = 0;
}

void ErrorReporter::printSummary() {
    if (error_count_ > 0 || warning_count_ > 0) {
        std::cerr << std::endl;
        
        if (error_count_ > 0) {
            std::cerr << Colors::RED << Colors::BOLD << "error: " << Colors::RESET
                      << "could not compile due to " << error_count_ << " error"
                      << (error_count_ > 1 ? "s" : "") << std::endl;
        }
        
        if (warning_count_ > 0) {
            std::cerr << Colors::YELLOW << Colors::BOLD << "warning: " << Colors::RESET
                      << warning_count_ << " warning" << (warning_count_ > 1 ? "s" : "")
                      << " emitted" << std::endl;
        }
    }
}

} // namespace pawc

