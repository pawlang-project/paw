//===--- driver.h - Command Line Driver -------------------------*- C++ -*-===//
//
// PawLang Compiler - Command Line Driver
//
//===----------------------------------------------------------------------===//

#ifndef PAW_DRIVER_H
#define PAW_DRIVER_H

#include "options.h"
#include <string>

namespace pawc {

/// Driver - 命令行驱动
///
/// 负责解析命令行参数并调用Compiler
class Driver {
public:
    Driver();
    
    /// 运行编译器
    /// \param argc 参数个数
    /// \param argv 参数数组
    /// \return 退出码（0表示成功）
    int run(int argc, char** argv);
    
private:
    CompilerOptions options_;
    
    /// 解析命令行参数
    /// \return true表示成功
    bool parseArguments(int argc, char** argv);
    
    /// 打印帮助信息
    void printHelp();
    
    /// 打印版本信息
    void printVersion();
};

} // namespace pawc

#endif // PAW_DRIVER_H
