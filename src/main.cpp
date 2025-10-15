#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "module/module_compiler.h"
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
    std::cout << "  " << program_name << " program.paw              # 生成可执行文件 ./a.out\n";
    std::cout << "  " << program_name << " program.paw -o hello    # 生成可执行文件 ./hello\n";
    std::cout << "  " << program_name << " program.paw --emit-obj  # 生成目标文件 output.o\n";
    std::cout << "  " << program_name << " program.paw --emit-llvm # 生成LLVM IR output.ll\n";
}

int main(int argc, char* argv[]) {
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
    
    std::cout << "Compiling " << input_file << "..." << std::endl;
    
    // Lexical analysis
    pawc::Lexer lexer(source, input_file);
    std::vector<pawc::Token> tokens = lexer.tokenize();
    std::cout << "  Lexer: " << tokens.size() << " tokens" << std::endl;
    
    // Parsing
    pawc::Parser parser(tokens);
    pawc::Program program = parser.parse();
    
    if (!program.errors.empty()) {
        std::cerr << "Parse errors:" << std::endl;
        for (const auto& error : program.errors) {
            std::cerr << "  " << error.location.filename << ":"
                      << error.location.line << ":" << error.location.column
                      << ": " << error.message << std::endl;
        }
        return 1;
    }
    
    std::cout << "  Parser: " << program.statements.size() << " statements" << std::endl;
    
    // 检查是否有import语句（判断是否需要模块编译）
    bool has_imports = false;
    for (const auto& stmt : program.statements) {
        if (stmt->kind == pawc::Stmt::Kind::Import) {
            has_imports = true;
            break;
        }
    }
    
    // 使用模块编译器或单文件编译器
    if (has_imports) {
        std::cout << "  Mode: Multi-module compilation" << std::endl;
        
        // 获取文件所在目录
        std::filesystem::path input_path(input_file);
        std::string base_dir = input_path.parent_path().string();
        if (base_dir.empty()) base_dir = ".";
        
        // 确定输出文件名
        if (output_file.empty()) {
            output_file = "a.out";
        }
        
        // 使用模块编译器
        pawc::ModuleCompiler compiler(base_dir);
        if (!compiler.compile(input_file, output_file)) {
            std::cerr << "Module compilation failed" << std::endl;
            return 1;
        }
        
        std::cout << "\nCompilation successful!" << std::endl;
        return 0;
    }
    
    // 单文件编译模式
    std::cout << "  Mode: Single-file compilation" << std::endl;
    
    // Code generation
    pawc::CodeGenerator codegen("pawc_module");
    if (!codegen.generate(program)) {
        std::cerr << "Code generation failed" << std::endl;
        return 1;
    }
    
    std::cout << "  CodeGen: Success" << std::endl;
    
    // Print IR if requested
    if (print_ir) {
        std::cout << "\n========== LLVM IR ==========\n" << std::endl;
        codegen.printIR();
        std::cout << "\n============================\n" << std::endl;
    }
    
    // Generate output
    if (emit_llvm) {
        // 生成LLVM IR
        if (output_file.empty()) {
            output_file = "output.ll";
        }
        codegen.saveIR(output_file);
        std::cout << "Generated: " << output_file << std::endl;
    } else if (emit_obj) {
        // 生成目标文件
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
        // 生成可执行文件（默认）
        std::string obj_file = "temp_output.o";
        if (!codegen.compileToObject(obj_file)) {
            std::cerr << "Failed to generate object file" << std::endl;
            return 1;
        }
        
        // 确定可执行文件名
        if (output_file.empty()) {
            output_file = "a.out";
        }
        
        // 使用LLVM自带的clang进行链接，并显式指定SDK路径
        std::string clang_path = "llvm/bin/clang";
        
        // macOS需要指定SDK路径
        std::string sdk_flags = " -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk";
        std::string link_cmd = clang_path + " " + obj_file + sdk_flags + " -o " + output_file;
        
        std::cout << "  Linking: " << output_file << std::endl;
        int ret = system(link_cmd.c_str());
        
        // 删除临时.o文件
        std::remove(obj_file.c_str());
        
        if (ret != 0) {
            std::cerr << "Linking failed" << std::endl;
            return 1;
        }
        
        std::cout << "Generated: " << output_file << " (executable)" << std::endl;
    }
    
    std::cout << "\nCompilation successful!" << std::endl;
    return 0;
}

