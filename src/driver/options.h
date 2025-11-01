//===--- options.h - Compiler Options ---------------------------*- C++ -*-===//
//
// PawLang Compiler - Compilation Options
//
//===----------------------------------------------------------------------===//

#ifndef PAW_OPTIONS_H
#define PAW_OPTIONS_H

#include <string>
#include <vector>

namespace pawc {

/// CompilerOptions - 编译选项
struct CompilerOptions {
    // 输入/输出
    std::string input_file;
    std::string output_file = "a.out";
    
    // 优化级别 (0-3)
    int opt_level = 0;
    
    // 编译模式
    bool compile_only = false;      // -c: 仅编译到对象文件
    bool emit_llvm_ir = false;      // -S: 输出LLVM IR
    bool emit_llvm_file = false;    // -emit-llvm: 输出.ll文件
    bool emit_ast = false;          // -ast-dump: 输出AST
    
    // 调试选项
    bool verbose = false;           // -v: 详细输出
    bool debug_info = false;        // -g: 生成调试信息
    
    // 库和路径
    std::vector<std::string> library_paths;  // -L
    std::vector<std::string> libraries;      // -l
    std::vector<std::string> include_paths;  // -I
    
    // 警告和错误
    bool warnings_as_errors = false;  // -Werror
    bool no_warnings = false;         // -w
    
    // 目标平台
    std::string target_triple;  // -target
};

} // namespace pawc

#endif // PAW_OPTIONS_H
