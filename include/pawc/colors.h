#ifndef PAWC_COLORS_H
#define PAWC_COLORS_H

#include <string>

namespace pawc {

// ANSI颜色代码
class Colors {
public:
    // 基础颜色
    static const std::string RESET;
    static const std::string BOLD;
    
    // 前景色
    static const std::string RED;
    static const std::string GREEN;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string MAGENTA;
    static const std::string CYAN;
    static const std::string WHITE;
    
    // 亮色
    static const std::string BRIGHT_RED;
    static const std::string BRIGHT_GREEN;
    static const std::string BRIGHT_YELLOW;
    static const std::string BRIGHT_BLUE;
    static const std::string BRIGHT_MAGENTA;
    static const std::string BRIGHT_CYAN;
    
    // 橙色
    static const std::string ORANGE;
    
    // 辅助函数
    static bool isTerminal();  // 检测是否是终端
    static std::string error(const std::string& msg);
    static std::string warning(const std::string& msg);
    static std::string success(const std::string& msg);
    static std::string info(const std::string& msg);
    static std::string highlight(const std::string& msg);
    static std::string dimmed(const std::string& msg);
    static std::string orange(const std::string& msg);
};

} // namespace pawc

#endif // PAWC_COLORS_H

