/**
 * @file codegen.cpp
 * @brief PawLang LLVM code generator implementation
 * 
 * Responsible for converting AST to LLVM IR, supporting full PawLang language features:
 * - Generic system (functions, Struct, Enum) - full monomorphization
 * - Generic struct internal methods (static, instance)
 * - Module system and cross-module calls
 * - Error handling (T? type, ? operator, ok/err)
 * - Pattern matching (is expression, match expression)
 * - Type inference and type conversion
 * - Struct reference semantics (unified pointer passing)
 * - Arrays (fixed-length, variable-length, multi-dimensional)
 * - String and character types
 * - Loop control (4 loop types + break/continue)
 * 
 * File organization:
 * - Part 1: Initialization and core interface
 * - Part 2: Type conversion system
 * - Part 3: Expression generation
 * - Part 4: Statement generation
 * - Part 5: Generic instantiation
 * 
 * @note File size: 3936 lines (planned to be split in the future)
 * @todo Split into multiple files: expr, stmt, type, struct, match
 * 
 * @version 0.2.1
 * @date 2025-10-16
 */

#include "codegen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include <iostream>

namespace pawc {

// ============================================================================
// Part 1: Initialization and Core Interface
// ============================================================================

/**
 * @brief Construct CodeGenerator (single-file mode)
 * @param module_name Module name
 * 
 * Initialize LLVM components:
 * - LLVMContext: Independent compilation context
 * - Module: IR module container
 * - IRBuilder: IR instruction builder
 * - Builtins: Built-in function manager
 * - DataLayout: Target platform data layout (ensure proper alignment)
 */
CodeGenerator::CodeGenerator(const std::string& module_name)
    : current_function_(nullptr), current_function_return_type_(nullptr),
      current_struct_(nullptr), current_struct_name_(""), 
      current_is_method_(false), 
      module_name_(module_name), symbol_table_(nullptr) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(module_name, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
    builtins_ = std::make_unique<Builtins>(*context_, *module_);
    
    // Set target platform DataLayout (ensure all types use proper alignment)
    llvm::InitializeNativeTarget();
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    module_->setTargetTriple(triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
    if (target) {
        llvm::TargetOptions opt;
        auto RM = std::optional<llvm::Reloc::Model>();
        auto target_machine = target->createTargetMachine(triple, "generic", "", opt, RM);
        if (target_machine) {
            module_->setDataLayout(target_machine->createDataLayout());
        }
    }
    
    // Declare all built-in functions
    builtins_->declareAll();
}

/**
 * @brief Construct CodeGenerator (multi-file mode)
 * @param module_name Module name
 * @param symbol_table Symbol table pointer (for cross-module lookup)
 * 
 * Same as single-file mode, but with additional support for:
 * - Cross-module type lookup
 * - Cross-module function calls
 * - Cross-module generic instantiation
 */
CodeGenerator::CodeGenerator(const std::string& module_name, SymbolTable* symbol_table)
    : current_function_(nullptr), current_function_return_type_(nullptr),
      current_struct_(nullptr), current_struct_name_(""), 
      current_is_method_(false), 
      module_name_(module_name), symbol_table_(symbol_table) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(module_name, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
    builtins_ = std::make_unique<Builtins>(*context_, *module_);
    
    // Set target platform DataLayout (ensure all types use proper alignment)
    llvm::InitializeNativeTarget();
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    module_->setTargetTriple(triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
    if (target) {
        llvm::TargetOptions opt;
        auto RM = std::optional<llvm::Reloc::Model>();
        auto target_machine = target->createTargetMachine(triple, "generic", "", opt, RM);
        if (target_machine) {
            module_->setDataLayout(target_machine->createDataLayout());
        }
    }
    
    // Declare all built-in functions
    builtins_->declareAll();
}

/**
 * @brief Generate LLVM IR code (main entry point)
 * @param program AST program node
 * @return true on success, false on verification failure
 * 
 * Uses two-pass compilation:
 * 1. First pass: Register all type definitions (Struct and Enum)
 *    - Allow function signatures to reference these types
 *    - Handle generic definition registration
 * 2. Second pass: Generate functions, statements, etc.
 *    - Types are fully available
 *    - Support forward references
 * 
 * Finally uses llvm::verifyModule() to verify the correctness of generated IR.
 */
bool CodeGenerator::generate(const Program& program) {
    // First pass: Register all type definitions (Struct and Enum)
    // This allows function signatures to reference these types
    for (const auto& stmt : program.statements) {
        if (stmt->kind == Stmt::Kind::Struct) {
            generateStructStmt(static_cast<const StructStmt*>(stmt.get()));
        } else if (stmt->kind == Stmt::Kind::Enum) {
            generateEnumStmt(static_cast<const EnumStmt*>(stmt.get()));
        }
    }
    
    // Second pass: Generate other statements (functions, etc.)
    for (const auto& stmt : program.statements) {
        // Skip already processed type definitions
        if (stmt->kind != Stmt::Kind::Struct && stmt->kind != Stmt::Kind::Enum) {
            generateStmt(stmt.get());
        }
    }
    
    // Verify module
    std::string error_str;
    llvm::raw_string_ostream error_stream(error_str);
    if (llvm::verifyModule(*module_, &error_stream)) {
        std::cerr << "Module verification failed:\n" << error_str << std::endl;
        return false;
    }
    
    return true;
}

void CodeGenerator::printIR() {
    module_->print(llvm::outs(), nullptr);
}

void CodeGenerator::saveIR(const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream file(filename, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file: " << EC.message() << std::endl;
        return;
    }
    module_->print(file, nullptr);
}

bool CodeGenerator::compileToObject(const std::string& filename) {
    // Only initialize native target (not All)
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();
    
    // Use Triple object (LLVM 21+)
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    module_->setTargetTriple(triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
    if (!target) {
        std::cerr << "Target lookup failed: " << error << std::endl;
        return false;
    }
    
    auto CPU = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto RM = std::optional<llvm::Reloc::Model>();
    // Use new API (LLVM 21+)
    auto target_machine = target->createTargetMachine(triple, CPU, features, opt, RM);
    
    module_->setDataLayout(target_machine->createDataLayout());
    
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file: " << EC.message() << std::endl;
        return false;
    }
    
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::ObjectFile;
    
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        std::cerr << "TargetMachine can't emit a file of this type" << std::endl;
        return false;
    }
    
    pass.run(*module_);
    dest.flush();
    
    return true;
}

} // namespace pawc
