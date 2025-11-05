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

/// CompilerOptions - compile/compilationoption
struct CompilerOptions {
    // input/output
    std::string input_file;
    std::string output_file = "a.out";
    
    // optimizationlevel (0-3)
    int opt_level = 0;
    
    // compile/compilationpattern
    bool compile_only = false;      // -c: compile only to object fileoobjectfile
    bool emit_llvm_ir = false;      // -S: outputLLVM IR
    bool emit_llvm_file = false;    // -emit-llvm: output.llfile
    bool emit_ast = false;          // -ast-dump: outputAST
    
    // debugoption
    bool verbose = false;           // -v: detailedoutput
    bool debug_info = false;        // -g: generatedebuginfo
    
    // libraryandpath
    std::vector<std::string> library_paths;  // -L
    std::vector<std::string> libraries;      // -l
    std::vector<std::string> include_paths;  // -I
    
    // warninganderror
    bool warnings_as_errors = false;  // -Werror
    bool no_warnings = false;         // -w
    
    // Target platform
    std::string target_triple;  // -target
};

} // namespace pawc

#endif // PAW_OPTIONS_H
