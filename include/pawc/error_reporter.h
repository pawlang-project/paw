#ifndef PAWC_ERROR_REPORTER_H
#define PAWC_ERROR_REPORTER_H

#include "common.h"
#include "colors.h"
#include <vector>
#include <optional>

namespace pawc {

// 错误级别
enum class ErrorLevel {
    Error,
    Warning,
    Note
};

// 错误提示
struct ErrorHint {
    std::string message;
    std::optional<SourceLocation> location;
    
    ErrorHint(const std::string& msg, std::optional<SourceLocation> loc = std::nullopt)
        : message(msg), location(loc) {}
};

// 详细错误信息
struct DetailedError {
    ErrorLevel level;
    std::string message;
    SourceLocation location;
    std::optional<std::string> code_snippet;  // 出错代码片段
    std::vector<ErrorHint> hints;             // 修复建议
    
    DetailedError(ErrorLevel lvl, const std::string& msg, const SourceLocation& loc)
        : level(lvl), message(msg), location(loc) {}
};

// 错误报告器
class ErrorReporter {
public:
    ErrorReporter() : error_count_(0), warning_count_(0) {}
    
    // 报告错误
    void reportError(const std::string& message, const SourceLocation& location,
                    const std::vector<ErrorHint>& hints = {});
    
    // 报告警告
    void reportWarning(const std::string& message, const SourceLocation& location,
                      const std::vector<ErrorHint>& hints = {});
    
    // 报告提示
    void reportNote(const std::string& message, const SourceLocation& location = SourceLocation());
    
    // 设置源代码（用于显示代码片段）
    void setSourceCode(const std::string& filename, const std::string& code);
    
    // 获取错误和警告数量
    int getErrorCount() const { return error_count_; }
    int getWarningCount() const { return warning_count_; }
    bool hasErrors() const { return error_count_ > 0; }
    
    // 清空错误记录
    void clear();
    
    // 打印所有错误
    void printSummary();
    
private:
    int error_count_;
    int warning_count_;
    std::map<std::string, std::vector<std::string>> source_lines_;  // filename -> lines
    
    // 内部辅助函数
    void printError(const DetailedError& error);
    std::string getCodeLine(const std::string& filename, int line);
    std::string getLevelPrefix(ErrorLevel level);
    std::string getLevelColor(ErrorLevel level);
};

} // namespace pawc

#endif // PAWC_ERROR_REPORTER_H

