//===--- driver.cpp - Command Line Driver Implementation --------*- C++ -*-===//
/// @file driver.cpp
/// @brief Implementation file

#include "driver.h"
#include "compiler.h"

#include <iostream>
#include <cstring>

namespace pawc {

Driver::Driver() {}

int Driver::run(int argc, char** argv) {
    // Parsing command line parameters
    if (!parseArguments(argc, argv)) {
        return 1;
    }
    
    // Checkyesnohasinputfile
    if (options_.input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        std::cerr << "Use 'pawc --help' for usage information\n";
        return 1;
    }
    
    // Create compiler and execute compilation
    Compiler compiler(options_);
    
    if (!compiler.compile(options_.input_file)) {
        return 1;
    }
    
    return 0;
}

bool Driver::parseArguments(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Help information
        if (arg == "-h" || arg == "--help") {
            printHelp();
            std::exit(0);
        }
        
        // Version information
        if (arg == "-v" || arg == "--version") {
            printVersion();
            std::exit(0);
        }
        
        // outputfile
        if (arg == "-o") {
            if (i + 1 < argc) {
                options_.output_file = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                return false;
            }
            continue;
        }
        
        // optimizationlevel
        if (arg == "-O0") { options_.opt_level = 0; continue; }
        if (arg == "-O1") { options_.opt_level = 1; continue; }
        if (arg == "-O2") { options_.opt_level = 2; continue; }
        if (arg == "-O3") { options_.opt_level = 3; continue; }
        if (arg.size() == 3 && arg[0] == '-' && arg[1] == 'O' && 
            arg[2] >= '0' && arg[2] <= '3') {
            options_.opt_level = arg[2] - '0';
            continue;
        }
        
        // compile/compilationoption
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
        
        // debugoption
        if (arg == "--verbose") {
            options_.verbose = true;
            continue;
        }
        
        if (arg == "-g") {
            options_.debug_info = true;
            continue;
        }
        
        // librarypath
        if (arg.size() > 2 && arg.substr(0, 2) == "-L") {
            options_.library_paths.push_back(arg.substr(2));
            continue;
        }
        
        // library
        if (arg.size() > 2 && arg.substr(0, 2) == "-l") {
            options_.libraries.push_back(arg.substr(2));
            continue;
        }
        
        // containspath
        if (arg.size() > 2 && arg.substr(0, 2) == "-I") {
            options_.include_paths.push_back(arg.substr(2));
            continue;
        }
        
        // warningoption
        if (arg == "-Werror") {
            options_.warnings_as_errors = true;
            continue;
        }
        
        if (arg == "-w") {
            options_.no_warnings = true;
            continue;
        }
        
        // Target platform
        if (arg == "-target") {
            if (i + 1 < argc) {
                options_.target_triple = argv[++i];
            } else {
                std::cerr << "Error: -target requires an argument\n";
                return false;
            }
            continue;
        }
        
        // Unknown option
        if (arg[0] == '-') {
            std::cerr << "Warning: Unknown option: " << arg << "\n";
            continue;
        }
        
        // inputfile
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
    std::cout << "  -o <file>        Output file name (default: a.out)\n";
    std::cout << "  -O0, -O1, -O2, -O3  Optimization level (default: -O0)\n";
    std::cout << "  -c               Compile only to object file\n";
    std::cout << "  -S               Output LLVM IR\n";
    std::cout << "  -emit-llvm       Output LLVM IR to .ll file\n";
    std::cout << "  -ast-dump        Output abstract syntax tree\n";
    std::cout << "  -g               Generate debug info\n";
    std::cout << "  --verbose        Detailed output\n";
    std::cout << "  -L<path>         Add library search path\n";
    std::cout << "  -l<lib>          Link library\n";
    std::cout << "  -I<path>         Add include path\n";
    std::cout << "  -Werror          Treat warnings as errors\n";
    std::cout << "  -w               Disable all warnings\n";
    std::cout << "  -target <triple> Specify target platform\n";
    std::cout << "  -h, --help       Display this help information\n";
    std::cout << "  -v, --version    Display version information\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  pawc hello.paw                  # Compile hello.paw\n";
    std::cout << "  pawc hello.paw -o hello         # Specify output file\n";
    std::cout << "  pawc hello.paw -O2              # Enable O2 optimization\n";
    std::cout << "  pawc hello.paw -c               # Compile only\n";
    std::cout << "  pawc hello.paw -S               # Output LLVM IR\n";
}

void Driver::printVersion() {
    std::cout << "PawLang Compiler v1.4.0\n";
    std::cout << "Pass-Based Architecture\n";
    std::cout << "Built with LLVM 21\n";
}

} // namespace pawc
