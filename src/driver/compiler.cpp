//===--- compiler.cpp - Compiler Implementation ------------------*- C++ -*-===//
/// @file compiler.cpp
/// @brief Implementation file

#include "compiler.h"
#include "linker.h"

#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include "frontend/parser/ast_printer.h"
#include "middleend/sema/type_checker.h"
#include "passes/transform/monomorphization_pass.h"
#include "passes/codegen/llvm_irgen_pass.h"
#include "passes/optimization/llvm_optimization_pass.h"
#include "passes/codegen/object_gen_pass.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace pawc {

Compiler::Compiler(const CompilerOptions& options)
    : options_(options) {
    initialize();
}

Compiler::~Compiler() {
    // Cleanup resources in correct order
    
    // 1. First cleanup PassManager
    pass_manager_.reset();
    
    // 2. Cleanup PassContext (contains CodeGenContext)
    if (pass_context_) {
        pass_context_->clearCache();
    }
    pass_context_.reset();
    
    // 3. Cleanup symbol table and types system
    symbol_table_.reset();
    type_system_.reset();
    
    // 4. Finally cleanup diagnostic engine
    diagnostics_.reset();
}

void Compiler::initialize() {
    // Create core components
    diagnostics_ = std::make_unique<DiagnosticEngine>();
    type_system_ = std::make_unique<TypeSystem>();
    symbol_table_ = std::make_unique<SymbolTable>(type_system_.get());
    
    // initializebuiltinsymbol
    symbol_table_->initializeBuiltins();
    
    // createPasscontext
    pass_context_ = std::make_unique<PassContext>(
        diagnostics_.get(),
        type_system_.get(),
        symbol_table_.get()
    );
    
    // configurePasscontext
    pass_context_->setOptLevel(options_.opt_level);
    pass_context_->setVerbose(options_.verbose);
    pass_context_->setOutputFile(options_.output_file);
    
    // createPassManager
    pass_manager_ = std::make_unique<PassManager>(pass_context_.get());
    
    // configurePassflow
    setupPasses();
}

void Compiler::setupPasses() {
    // According to architecture, add passes sequentially
    
    // 1. LLVM IR generation pass
    pass_manager_->addPass<LLVMIRGenPass>();
    
    // 2. Optimization pass (if optimization level > 0)
    if (options_.opt_level > 0) {
        pass_manager_->addPass<LLVMOptimizationPass>();
    }
    
    // 3. Object file generation pass (if not IR-only output)
    if (!options_.emit_llvm_ir) {
        pass_manager_->addPass<ObjectGenPass>();
    }
}

std::string Compiler::readSourceFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        diagnostics_->reportError("Cannot open file: " + filename, SourceLocation());
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool Compiler::compile(const std::string& source_file) {
    if (options_.verbose) {
        std::cout << "+----------------------------------------+\n";
        std::cout << "|   PawLang Compiler v1.4.0              |\n";
        std::cout << "|   Pass-Based Architecture              |\n";
        std::cout << "+----------------------------------------+\n\n";
        std::cout << "Compiling: " << source_file << "\n";
        std::cout << "Optimization level: O" << options_.opt_level << "\n\n";
    }
    
    // === Phase 1: Lexical Analysis ===
    if (options_.verbose) std::cout << "[*] Phase 1: Lexical Analysis\n";
    
    std::string source = readSourceFile(source_file);
    if (source.empty() && diagnostics_->hasErrors()) {
        return false;
    }
    
    Lexer lexer(source, source_file, diagnostics_.get());
    auto tokens = lexer.tokenize();
    
    if (options_.verbose) {
        std::cout << "   [OK] " << tokens.size() << " tokens generated\n\n";
    }
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    // === Phase 2: Syntax Analysis ===
    if (options_.verbose) std::cout << "[*] Phase 2: Syntax Analysis\n";
    
    Parser parser(tokens, diagnostics_.get(), type_system_.get());
    auto ast = parser.parse();
    
    if (options_.verbose) {
        std::cout << "   [OK] " << ast.size() << " statements parsed\n\n";
    }
    
    if (options_.emit_ast) {
        std::cout << "\n========================================\n";
        std::cout << "AST Dump:\n";
        std::cout << "========================================\n\n";
        
        ASTPrinter printer(std::cout);
        for (const auto& stmt : ast) {
            printer.print(stmt.get());
            std::cout << "\n";
        }
        
        std::cout << "========================================\n\n";
    }
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    // === Phase 2.5: Monomorphization (genericmonomorphization) ===
    if (options_.verbose) std::cout << "[*] Phase 2.5: Generic Monomorphization\n";
    
    // setASTtoPassContext
    pass_context_->setAST(&ast);
    
    // runmonomorphizationPass
    MonomorphizationPass mono_pass;
    PassResult mono_results = mono_pass.run(pass_context_.get());
    
    // mergemonomorphizationinstancetoAST
    auto& mono_instances = mono_pass.getMonomorphizedInstances();
    size_t mono_count = mono_instances.size();
    for (auto& inst : mono_instances) {
        ast.push_back(std::move(inst));
    }
    
    if (options_.verbose) {
        std::cout << "   [OK] " << mono_results.message << "\n";
        if (mono_count > 0) {
            std::cout << "   [+] Merged " << mono_count << " monomorphized instances into AST\n";
        }
        std::cout << "   [TIME] Execution time: " << mono_results.execution_time_ms << " ms\n\n";
    }
    
    if (!mono_results.success) {
        diagnostics_->reportError("Monomorphization failed: " + mono_results.message, 
                                  SourceLocation());
        return false;
    }
    
    // Update PassContext with AST (contains monomorphization instances)
    pass_context_->setAST(&ast);
    
    // Debug: monomorphizationback/afterof/theAST dump
    if (options_.emit_ast && mono_count > 0) {
        std::cout << "\n========================================\n";
        std::cout << "Post-Monomorphization AST:\n";
        std::cout << "========================================\n\n";
        
        ASTPrinter printer(std::cout);
        for (const auto& stmt : ast) {
            printer.print(stmt.get());
            std::cout << "\n";
        }
        
        std::cout << "========================================\n\n";
    }
    
    // === Phase 3: Semantic Analysis ===
    if (options_.verbose) std::cout << "[*] Phase 3: Semantic Analysis\n";
    
    TypeChecker type_checker(type_system_.get(), symbol_table_.get(), diagnostics_.get());
    type_checker.check(ast);
    
    if (options_.verbose) {
        std::cout << "   [OK] Type checking complete\n\n";
    }
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    // === Phase 4: Code Generation (through/viaPassManager) ===
    if (options_.verbose) std::cout << "[*] Phase 4: Code Generation\n";
    
    // Pass AST to PassContext
    pass_context_->setAST(&ast);
    
    // runAllPass
    pass_manager_->runAll();
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    if (options_.verbose) {
        std::cout << "   [OK] Code generation complete\n\n";
    }
    
    // === Phase 5: Linking (if needed) ===
    if (!options_.compile_only && !options_.emit_llvm_ir) {
        return processResults();
    }
    
    if (options_.verbose) {
        std::cout << "[SUCCESS] Compilation successful!\n";
    }
    
    return true;
}

bool Compiler::processResults() {
    try {
        // fromPassContextgetobjectfilepath
        std::string object_file;
        if (!pass_context_->getCachedResult("object_file", object_file)) {
            diagnostics_->reportError("No object file generated", SourceLocation());
            return false;
        }
        
        if (options_.verbose) {
            std::cout << "[*] Phase 5: Linking\n";
            std::cout << "   Object file: " << object_file << "\n";
        }
        
        // Create linker
        Linker linker;
        linker.setVerbose(options_.verbose);
        
        // Bundled lld path is auto-detected in Linker constructor
        // If needed, can be manually set here with absolute path
        
        // =================================================================
        // Elegant runtime library path detection
        // =================================================================
        
        std::string runtime_path;
        
        // Strategy 1: Use CMake-provided path hint (highest priority)
#ifdef PAWC_RUNTIME_PATH_HINT
        std::string hint_path = PAWC_RUNTIME_PATH_HINT;
        if (std::filesystem::exists(hint_path)) {
#ifdef _WIN32
            std::string lib_file = hint_path + "/pawc_runtime.lib";
#else
            std::string lib_file = hint_path + "/libpawc_runtime.a";
#endif
            if (std::filesystem::exists(lib_file)) {
                runtime_path = hint_path;
                if (options_.verbose) {
                    std::cout << "   Found runtime library (via CMake hint): " << runtime_path << "\n";
                }
            }
        }
#endif
        
        // Strategy 2: Fallback to relative path search
        if (runtime_path.empty()) {
            std::vector<std::string> relative_search_paths = {
                "../build-msvc/src/runtime/Release",
                "../build-msvc/src/runtime/Debug",
                "../build/src/runtime",
                "../cmake-build-release/src/runtime",
                "../cmake-build-debug/src/runtime"
            };
            
            for (const auto& path : relative_search_paths) {
#ifdef _WIN32
                std::string lib_file = path + "/pawc_runtime.lib";
#else
                std::string lib_file = path + "/libpawc_runtime.a";
#endif
                if (std::filesystem::exists(lib_file)) {
                    runtime_path = path;
                    if (options_.verbose) {
                        std::cout << "   Found runtime library (via fallback): " << runtime_path << "\n";
                    }
                    break;
                }
            }
        }
        
        if (runtime_path.empty()) {
            diagnostics_->reportWarning(
                "Runtime library not found. Please ensure pawc_runtime is compiled.",
                SourceLocation());
        }
        
        linker.setRuntimePath(runtime_path);
        
        // executelink
        if (!linker.link({object_file}, options_.output_file, 
                         options_.library_paths, options_.libraries)) {
            diagnostics_->reportError("Linking failed: " + linker.getError(), 
                                      SourceLocation());
            return false;
        }
        
        if (options_.verbose) {
            std::cout << "   [OK] Executable generated: " << options_.output_file << "\n\n";
        }
        
        std::cout << "[SUCCESS] Compilation successful!\n";
        std::cout << "   Executable: " << options_.output_file << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in processResults: " << e.what() << "\n";
        return false;
    }
}

} // namespace pawc
