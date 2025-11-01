//===--- driver.cpp - Command Line Driver Implementation --------*- C++ -*-===//

#include "driver.h"
#include "compiler.h"

#include <iostream>
#include <cstring>

namespace pawc {

Driver::Driver() {}

int Driver::run(int argc, char** argv) {
    // 解析命令行参数
    if (!parseArguments(argc, argv)) {
        return 1;
    }
    
    // 检查是否有输入文件
    if (options_.input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        std::cerr << "Use 'pawc --help' for usage information\n";
        return 1;
    }
    
    // 创建编译器并执行编译
    Compiler compiler(options_);
    
    if (!compiler.compile(options_.input_file)) {
        return 1;
    }
    
    return 0;
}

bool Driver::parseArguments(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // 帮助信息
        if (arg == "-h" || arg == "--help") {
            printHelp();
            std::exit(0);
        }
        
        // 版本信息
        if (arg == "-v" || arg == "--version") {
            printVersion();
            std::exit(0);
        }
        
        // 输出文件
        if (arg == "-o") {
            if (i + 1 < argc) {
                options_.output_file = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                return false;
            }
            continue;
        }
        
        // 优化级别
        if (arg == "-O0") { options_.opt_level = 0; continue; }
        if (arg == "-O1") { options_.opt_level = 1; continue; }
        if (arg == "-O2") { options_.opt_level = 2; continue; }
        if (arg == "-O3") { options_.opt_level = 3; continue; }
        if (arg.size() == 3 && arg[0] == '-' && arg[1] == 'O' && 
            arg[2] >= '0' && arg[2] <= '3') {
            options_.opt_level = arg[2] - '0';
            continue;
        }
        
        // 编译选项
        if (arg == "-c") {
            options_.compile_only = true;
            continue;
        }
        
        if (arg == "-S") {
            options_.emit_llvm_ir = true;
            continue;
        }
        
        if (arg == "-emit-llvm") {
            options_.emit_llvm_file = true;
            continue;
        }
        
        if (arg == "-ast-dump") {
            options_.emit_ast = true;
            continue;
        }
        
        // 调试选项
        if (arg == "--verbose") {
            options_.verbose = true;
            continue;
        }
        
        if (arg == "-g") {
            options_.debug_info = true;
            continue;
        }
        
        // 库路径
        if (arg.size() > 2 && arg.substr(0, 2) == "-L") {
            options_.library_paths.push_back(arg.substr(2));
            continue;
        }
        
        // 库
        if (arg.size() > 2 && arg.substr(0, 2) == "-l") {
            options_.libraries.push_back(arg.substr(2));
            continue;
        }
        
        // 包含路径
        if (arg.size() > 2 && arg.substr(0, 2) == "-I") {
            options_.include_paths.push_back(arg.substr(2));
            continue;
        }
        
        // 警告选项
        if (arg == "-Werror") {
            options_.warnings_as_errors = true;
            continue;
        }
        
        if (arg == "-w") {
            options_.no_warnings = true;
            continue;
        }
        
        // 目标平台
        if (arg == "-target") {
            if (i + 1 < argc) {
                options_.target_triple = argv[++i];
            } else {
                std::cerr << "Error: -target requires an argument\n";
                return false;
            }
            continue;
        }
        
        // 未知选项
        if (arg[0] == '-') {
            std::cerr << "Warning: Unknown option: " << arg << "\n";
            continue;
        }
        
        // 输入文件
        if (options_.input_file.empty()) {
            options_.input_file = arg;
        } else {
            std::cerr << "Error: Multiple input files not supported\n";
            return false;
        }
    }
    
    return true;
}

void Driver::printHelp() {
    std::cout << "PawLang Compiler v1.4.0\n\n";
    std::cout << "Usage: pawc [options] <file.paw>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>        输出文件名 (默认: a.out)\n";
    std::cout << "  -O0, -O1, -O2, -O3  优化级别 (默认: -O0)\n";
    std::cout << "  -c               仅编译到对象文件，不链接\n";
    std::cout << "  -S               输出LLVM IR\n";
    std::cout << "  -emit-llvm       输出LLVM IR到.ll文件\n";
    std::cout << "  -ast-dump        输出抽象语法树\n";
    std::cout << "  -g               生成调试信息\n";
    std::cout << "  --verbose        详细输出\n";
    std::cout << "  -L<path>         添加库搜索路径\n";
    std::cout << "  -l<lib>          链接库\n";
    std::cout << "  -I<path>         添加包含路径\n";
    std::cout << "  -Werror          将警告视为错误\n";
    std::cout << "  -w               禁用所有警告\n";
    std::cout << "  -target <triple> 指定目标平台\n";
    std::cout << "  -h, --help       显示此帮助信息\n";
    std::cout << "  -v, --version    显示版本信息\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  pawc hello.paw                  # 编译hello.paw\n";
    std::cout << "  pawc hello.paw -o hello         # 指定输出文件\n";
    std::cout << "  pawc hello.paw -O2              # 启用O2优化\n";
    std::cout << "  pawc hello.paw -c               # 仅编译，不链接\n";
    std::cout << "  pawc hello.paw -S               # 输出LLVM IR\n";
}

void Driver::printVersion() {
    std::cout << "PawLang Compiler v1.4.0\n";
    std::cout << "Pass-Based Architecture\n";
    std::cout << "Built with LLVM 21\n";
}

} // namespace pawc
