#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "module/module_compiler.h"
#include "pawc/colors.h"
#include "pawc/error_reporter.h"
#include "llvm/Support/TargetSelect.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <input-file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>       Write output to <file> (default: executable)\n";
    std::cout << "  --emit-llvm     Emit LLVM IR instead of executable\n";
    std::cout << "  --emit-obj      Emit object file (.o) instead of executable\n";
    std::cout << "  --print-ast     Print the Abstract Syntax Tree\n";
    std::cout << "  --print-ir      Print LLVM IR to stdout\n";
    std::cout << "  -h, --help      Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " program.paw              # Generate executable ./a.out\n";
    std::cout << "  " << program_name << " program.paw -o hello    # Generate executable ./hello\n";
    std::cout << "  " << program_name << " program.paw --emit-obj  # Generate object file output.o\n";
    std::cout << "  " << program_name << " program.paw --emit-llvm # Generate LLVM IR output.ll\n";
}

int main(int argc, char* argv[]) {
    // Initialize LLVM native target (must be initialized before use)
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();
    
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string input_file;
    std::string output_file;
    bool emit_llvm = false;
    bool emit_obj = false;
    bool print_ast = false;
    bool print_ir = false;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--emit-llvm") {
            emit_llvm = true;
        } else if (arg == "--emit-obj") {
            emit_obj = true;
        } else if (arg == "--print-ast") {
            print_ast = true;
        } else if (arg == "--print-ir") {
            print_ir = true;
        } else if (arg[0] != '-') {
            input_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return 1;
        }
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: No input file specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Read input file
    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file: " << input_file << std::endl;
        return 1;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();
    
    std::cout << pawc::Colors::info("Compiling ") << input_file << "..." << std::endl;
    
    // Create error reporter
    pawc::ErrorReporter error_reporter;
    error_reporter.setSourceCode(input_file, source);
    
    // Lexical analysis
    pawc::Lexer lexer(source, input_file);
    std::vector<pawc::Token> tokens = lexer.tokenize();
    std::cout << pawc::Colors::success("  ✓ Lexer: ") << tokens.size() << " tokens" << std::endl;
    
    // Parsing (using ErrorReporter)
    pawc::Parser parser(tokens, &error_reporter);
    pawc::Program program = parser.parse();
    
    // Check for parse errors
    if (error_reporter.hasErrors()) {
        std::cerr << std::endl;  // 空行
        error_reporter.printSummary();
        return 1;
    }
    
    // Backward compatibility: check old errors
    if (!program.errors.empty()) {
        std::cerr << pawc::Colors::error("\n✗ Parse errors:\n") << std::endl;
        for (const auto& error : program.errors) {
            std::cerr << pawc::Colors::error("  error: ") << error.message << std::endl;
            std::cerr << pawc::Colors::info("   --> ") << error.location.filename << ":"
                      << error.location.line << ":" << error.location.column << std::endl;
        }
        return 1;
    }
    
    std::cout << pawc::Colors::success("  ✓ Parser: ") << program.statements.size() << " statements" << std::endl;
    
    // Check for import statements (determine if module compilation is needed)
    bool has_imports = false;
    for (const auto& stmt : program.statements) {
        if (stmt->kind == pawc::Stmt::Kind::Import) {
            has_imports = true;
            break;
        }
    }
    
    // Use module compiler or single-file compiler
    if (has_imports) {
        std::cout << pawc::Colors::info("  → Mode: ") << "Multi-module compilation" << std::endl;
        
        // Get file directory
        std::filesystem::path input_path(input_file);
        std::string base_dir = input_path.parent_path().string();
        if (base_dir.empty()) base_dir = ".";
        
        // Determine output filename
        if (output_file.empty()) {
            output_file = "a.out";
        }
        
        // Use module compiler
        pawc::ModuleCompiler compiler(base_dir);
        if (!compiler.compile(input_file, output_file)) {
            std::cerr << pawc::Colors::error("\n✗ Module compilation failed") << std::endl;
            return 1;
        }
        
        std::cout << pawc::Colors::success("\n✓ Compilation successful!") << std::endl;
        return 0;
    }
    
    // Single-file compilation mode
    std::cout << pawc::Colors::info("  → Mode: ") << "Single-file compilation" << std::endl;
    
    // Code generation
    pawc::CodeGenerator codegen("pawc_module");
    if (!codegen.generate(program)) {
        std::cerr << pawc::Colors::error("\n✗ Code generation failed") << std::endl;
        return 1;
    }
    
    std::cout << pawc::Colors::success("  ✓ CodeGen: ") << "Success" << std::endl;
    
    // Print IR if requested
    if (print_ir) {
        std::cout << "\n========== LLVM IR ==========\n" << std::endl;
        codegen.printIR();
        std::cout << "\n============================\n" << std::endl;
    }
    
    // Generate output
    if (emit_llvm) {
        // Generate LLVM IR
        if (output_file.empty()) {
            output_file = "output.ll";
        }
        codegen.saveIR(output_file);
        std::cout << "Generated: " << output_file << std::endl;
    } else if (emit_obj) {
        // Generate object file
        if (output_file.empty()) {
            output_file = "output.o";
        }
        if (codegen.compileToObject(output_file)) {
            std::cout << "Generated: " << output_file << std::endl;
        } else {
            std::cerr << "Failed to generate object file" << std::endl;
            return 1;
        }
    } else {
        // Generate executable (default)
        std::string obj_file = "temp_output.o";
        if (!codegen.compileToObject(obj_file)) {
            std::cerr << "Failed to generate object file" << std::endl;
            return 1;
        }
        
        // Determine executable filename
        if (output_file.empty()) {
            output_file = "a.out";
        }
        
        // ========== Linker Selection and Configuration ==========
        // Use system C++ compiler for linking (cross-platform compatible)
        std::string compiler;
        
        // 1. Prefer project-local clang++ (if built)
        std::string local_clang = std::filesystem::current_path().parent_path().string() + "/cmake-build-release/Release/bin/clang++.exe";
        if (std::filesystem::exists(local_clang)) {
            compiler = local_clang;
            std::cout << pawc::Colors::info("  → Using project clang++") << std::endl;
        }
        // 2. Prefer environment variable (CMake/build system setting)
        else if (const char* cxx_env = std::getenv("CXX"); cxx_env && strlen(cxx_env) > 0) {
            compiler = cxx_env;
            // Use compiler from environment variable
        }
        // 3. Windows: Try cl.exe (MSVC) or g++ (MinGW)
#if defined(_WIN32) || defined(_WIN64)
        else if (system("where cl.exe >nul 2>&1") == 0) {
            compiler = "cl.exe";
        } else if (system("where clang++ >nul 2>&1") == 0) {
            compiler = "clang++";
        } else if (system("where g++ >nul 2>&1") == 0) {
            compiler = "g++";
        }
#else
        // 3. Unix-like systems: c++, clang++, g++
        else if (system("command -v c++ > /dev/null 2>&1") == 0) {
            compiler = "c++";
        } else if (system("command -v clang++ > /dev/null 2>&1") == 0) {
            compiler = "clang++";
        } else if (system("command -v g++ > /dev/null 2>&1") == 0) {
            compiler = "g++";
        }
#endif
        else {
            std::cerr << pawc::Colors::error("Error: No C++ compiler found!") << std::endl;
            std::cerr << "Tried: ";
#if defined(_WIN32) || defined(_WIN64)
            std::cerr << "$CXX, cl.exe, g++, clang++" << std::endl;
            std::cerr << "Please install Visual Studio or MinGW-w64" << std::endl;
#else
            std::cerr << "$CXX, c++, clang++, g++" << std::endl;
            std::cerr << "Please install a C++ compiler (gcc or clang)" << std::endl;
#endif
            return 1;
        }
        
        // ========== Build Link Command ==========
        std::string link_cmd = compiler + " " + obj_file;
        
        // Platform-specific link options (ensure compatibility)
#ifdef __APPLE__
        // ===== macOS (x86_64 / ARM64) =====
        // 1. SDK path (optional, enhanced compatibility)
        if (std::filesystem::exists("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")) {
            link_cmd += " -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk";
        } else if (std::filesystem::exists("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")) {
            link_cmd += " -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
        }
        // 2. macOS automatically links system libraries, no extra flags needed
        
#elif defined(_WIN32) || defined(_WIN64)
        // ===== Windows (x86 / x86_64 / ARM64) =====
        // MSVC vs MinGW need different flags
        if (compiler.find("cl.exe") != std::string::npos || compiler.find("cl ") != std::string::npos) {
            // MSVC linker
            link_cmd = compiler + " /Fe:" + output_file + " " + obj_file;
            link_cmd += " /link /SUBSYSTEM:CONSOLE";
            // MSVC uses different command format, return command directly
        } else {
            // MinGW/GCC format
            link_cmd += " -static-libgcc -static-libstdc++";  // Statically link runtime
        }
        
#elif defined(__linux__)
        // ===== Linux (multi-architecture support) =====
        // x86_64, ARM64, ARM, RISC-V, PowerPC, LoongArch, s390x
        
        // 1. Math library (needed for some architectures)
        link_cmd += " -lm";
        
        // 2. Thread library (if multithreading is used)
        // link_cmd += " -lpthread";
        
        // 3. Dynamic linker (usually handled automatically)
        // Linux libc is automatically linked
        
#elif defined(__FreeBSD__)
        // ===== FreeBSD =====
        link_cmd += " -lm";
        
#elif defined(__OpenBSD__)
        // ===== OpenBSD =====
        link_cmd += " -lm";
        
#elif defined(__NetBSD__)
        // ===== NetBSD =====
        link_cmd += " -lm";
        
#else
        // ===== Other Unix-like systems =====
        link_cmd += " -lm";  // Conservative strategy: add math library
        
#endif
        
        // For non-MSVC, add output file parameter
        if (compiler.find("cl.exe") == std::string::npos && compiler.find("cl ") == std::string::npos) {
            link_cmd += " -o " + output_file;
        }
        
        std::cout << pawc::Colors::info("  → Linking: ") << output_file << std::endl;
        int ret = system(link_cmd.c_str());
        
        // Delete temporary .o file
        std::remove(obj_file.c_str());
        
        if (ret != 0) {
            std::cerr << pawc::Colors::error("\n✗ Linking failed") << std::endl;
            return 1;
        }
        
        std::cout << pawc::Colors::highlight("  Generated: ") << output_file << " (executable)" << std::endl;
    }
    
    std::cout << pawc::Colors::success("\n✓ Compilation successful!") << std::endl;
    return 0;
}

