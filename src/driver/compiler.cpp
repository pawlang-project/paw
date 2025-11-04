//===--- compiler.cpp - Compiler Implementation ------------------*- C++ -*-===//

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

namespace pawc {

Compiler::Compiler(const CompilerOptions& options)
    : options_(options) {
    initialize();
}

Compiler::~Compiler() {
    // 按正确的顺序清理资源
    
    // 1. 首先清理Pass管理器
    pass_manager_.reset();
    
    // 2. 清理PassContext（包含CodeGenContext）
    if (pass_context_) {
        pass_context_->clearCache();
    }
    pass_context_.reset();
    
    // 3. 清理符号表和类型系统
    symbol_table_.reset();
    type_system_.reset();
    
    // 4. 最后清理诊断引擎
    diagnostics_.reset();
}

void Compiler::initialize() {
    // 创建核心组件
    diagnostics_ = std::make_unique<DiagnosticEngine>();
    type_system_ = std::make_unique<TypeSystem>();
    symbol_table_ = std::make_unique<SymbolTable>(type_system_.get());
    
    // 初始化builtin符号
    symbol_table_->initializeBuiltins();
    
    // 创建Pass上下文
    pass_context_ = std::make_unique<PassContext>(
        diagnostics_.get(),
        type_system_.get(),
        symbol_table_.get()
    );
    
    // 配置Pass上下文
    pass_context_->setOptLevel(options_.opt_level);
    pass_context_->setVerbose(options_.verbose);
    
    // 创建PassManager
    pass_manager_ = std::make_unique<PassManager>(pass_context_.get());
    
    // 配置Pass流程
    setupPasses();
}

void Compiler::setupPasses() {
    // 根据架构，按顺序添加Pass
    
    // 1. LLVM IR生成Pass
    pass_manager_->addPass<LLVMIRGenPass>();
    
    // 2. 优化Pass（如果优化级别>0）
    if (options_.opt_level > 0) {
        pass_manager_->addPass<LLVMOptimizationPass>();
    }
    
    // 3. 对象文件生成Pass（如果不是仅输出IR）
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
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║   PawLang Compiler v1.4.0              ║\n";
        std::cout << "║   Pass-Based Architecture              ║\n";
        std::cout << "╚════════════════════════════════════════╝\n\n";
        std::cout << "Compiling: " << source_file << "\n";
        std::cout << "Optimization level: O" << options_.opt_level << "\n\n";
    }
    
    // === Phase 1: Lexical Analysis ===
    if (options_.verbose) std::cout << "📝 Phase 1: Lexical Analysis\n";
    
    std::string source = readSourceFile(source_file);
    if (source.empty() && diagnostics_->hasErrors()) {
        return false;
    }
    
    Lexer lexer(source, source_file, diagnostics_.get());
    auto tokens = lexer.tokenize();
    
    if (options_.verbose) {
        std::cout << "   ✅ " << tokens.size() << " tokens generated\n\n";
    }
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    // === Phase 2: Syntax Analysis ===
    if (options_.verbose) std::cout << "📝 Phase 2: Syntax Analysis\n";
    
    Parser parser(tokens, diagnostics_.get(), type_system_.get());
    auto ast = parser.parse();
    
    if (options_.verbose) {
        std::cout << "   ✅ " << ast.size() << " statements parsed\n\n";
    }
    
    if (options_.emit_ast) {
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "AST Dump:\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
        
        ASTPrinter printer(std::cout);
        for (const auto& stmt : ast) {
            printer.print(stmt.get());
            std::cout << "\n";
        }
        
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    }
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    // === Phase 2.5: Monomorphization (泛型单态化) ===
    if (options_.verbose) std::cout << "📝 Phase 2.5: Generic Monomorphization\n";
    
    // 设置AST到PassContext
    pass_context_->setAST(&ast);
    
    // 运行单态化Pass
    MonomorphizationPass mono_pass;
    PassResult mono_result = mono_pass.run(pass_context_.get());
    
    // 合并单态化实例到AST
    auto& mono_instances = mono_pass.getMonomorphizedInstances();
    size_t mono_count = mono_instances.size();
    for (auto& inst : mono_instances) {
        ast.push_back(std::move(inst));
    }
    
    if (options_.verbose) {
        std::cout << "   ✅ " << mono_result.message << "\n";
        if (mono_count > 0) {
            std::cout << "   📦 Merged " << mono_count << " monomorphized instances into AST\n";
        }
        std::cout << "   ⏱️  Execution time: " << mono_result.execution_time_ms << " ms\n\n";
    }
    
    if (!mono_result.success) {
        diagnostics_->reportError("Monomorphization failed: " + mono_result.message, 
                                  SourceLocation());
        return false;
    }
    
    // 更新PassContext中的AST（包含单态化实例）
    pass_context_->setAST(&ast);
    
    // Debug: 单态化后的AST dump
    if (options_.emit_ast && mono_count > 0) {
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "Post-Monomorphization AST:\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
        
        ASTPrinter printer(std::cout);
        for (const auto& stmt : ast) {
            printer.print(stmt.get());
            std::cout << "\n";
        }
        
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    }
    
    // === Phase 3: Semantic Analysis ===
    if (options_.verbose) std::cout << "📝 Phase 3: Semantic Analysis\n";
    
    TypeChecker type_checker(type_system_.get(), symbol_table_.get(), diagnostics_.get());
    type_checker.check(ast);
    
    if (options_.verbose) {
        std::cout << "   ✅ Type checking complete\n\n";
    }
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    // === Phase 4: Code Generation (通过PassManager) ===
    if (options_.verbose) std::cout << "📝 Phase 4: Code Generation\n";
    
    // 将AST传递给PassContext
    pass_context_->setAST(&ast);
    
    // 运行所有Pass
    pass_manager_->runAll();
    
    if (diagnostics_->hasErrors()) {
        diagnostics_->printAll();
        return false;
    }
    
    if (options_.verbose) {
        std::cout << "   ✅ Code generation complete\n\n";
    }
    
    // === Phase 5: 链接（如果需要） ===
    if (!options_.compile_only && !options_.emit_llvm_ir) {
        return processResults();
    }
    
    if (options_.verbose) {
        std::cout << "✨ Compilation successful!\n";
    }
    
    return true;
}

bool Compiler::processResults() {
    try {
        // 从PassContext获取对象文件路径
        std::string object_file;
        if (!pass_context_->getCachedResult("object_file", object_file)) {
            diagnostics_->reportError("No object file generated", SourceLocation());
            return false;
        }
        
        if (options_.verbose) {
            std::cout << "📝 Phase 5: Linking\n";
            std::cout << "   Object file: " << object_file << "\n";
        }
        
        // 创建链接器
        Linker linker;
        linker.setVerbose(options_.verbose);
        
        // 设置runtime库路径（相对于当前工作目录）
        // 从 build/ 目录运行时，路径应该是 src/runtime
        linker.setRuntimePath("src/runtime");
        
        // 执行链接
        if (!linker.link({object_file}, options_.output_file, 
                         options_.library_paths, options_.libraries)) {
            diagnostics_->reportError("Linking failed: " + linker.getError(), 
                                      SourceLocation());
            return false;
        }
        
        if (options_.verbose) {
            std::cout << "   ✅ Executable generated: " << options_.output_file << "\n\n";
        }
        
        std::cout << "✨ Compilation successful!\n";
        std::cout << "   Executable: " << options_.output_file << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in processResults: " << e.what() << "\n";
        return false;
    }
}

} // namespace pawc
