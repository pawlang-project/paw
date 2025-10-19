#include "pawc/colors.h"

#ifdef _WIN32
#include <io.h>      // for _isatty
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>  // for isatty
#endif

namespace pawc {

// ANSI颜色代码定义
const std::string Colors::RESET = "\033[0m";
const std::string Colors::BOLD = "\033[1m";
const std::string DIM = "\033[2m";

const std::string Colors::RED = "\033[31m";
const std::string Colors::GREEN = "\033[32m";
const std::string Colors::YELLOW = "\033[33m";
const std::string Colors::BLUE = "\033[34m";
const std::string Colors::MAGENTA = "\033[35m";
const std::string Colors::CYAN = "\033[36m";
const std::string Colors::WHITE = "\033[37m";

const std::string Colors::BRIGHT_RED = "\033[91m";
const std::string Colors::BRIGHT_GREEN = "\033[92m";
const std::string Colors::BRIGHT_YELLOW = "\033[93m";
const std::string Colors::BRIGHT_BLUE = "\033[94m";
const std::string Colors::BRIGHT_MAGENTA = "\033[95m";
const std::string Colors::BRIGHT_CYAN = "\033[96m";

// 橙色 (使用 256 色模式)
const std::string Colors::ORANGE = "\033[38;5;208m";

// 检测是否是终端（支持彩色输出）
bool Colors::isTerminal() {
    return isatty(fileno(stderr)) != 0;
}

// 格式化辅助函数
std::string Colors::error(const std::string& msg) {
    if (!isTerminal()) return msg;
    return BOLD + BRIGHT_RED + msg + RESET;
}

std::string Colors::warning(const std::string& msg) {
    if (!isTerminal()) return msg;
    return BOLD + BRIGHT_YELLOW + msg + RESET;
}

std::string Colors::success(const std::string& msg) {
    if (!isTerminal()) return msg;
    return BOLD + GREEN + msg + RESET;  // 使用普通绿色，更柔和
}

std::string Colors::info(const std::string& msg) {
    if (!isTerminal()) return msg;
    return BOLD + CYAN + msg + RESET;  // 使用青色，比蓝色更温和
}

std::string Colors::highlight(const std::string& msg) {
    if (!isTerminal()) return msg;
    return BOLD + YELLOW + msg + RESET;  // 使用黄色，比洋红色柔和
}

std::string Colors::dimmed(const std::string& msg) {
    if (!isTerminal()) return msg;
    return DIM + msg + RESET;
}

std::string Colors::orange(const std::string& msg) {
    if (!isTerminal()) return msg;
    return BOLD + ORANGE + msg + RESET;
}

} // namespace pawc

