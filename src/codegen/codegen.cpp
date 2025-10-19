/**
 * @file codegen.cpp
 * @brief PawLang LLVM代码生成器实现
 * 
 * 负责将AST转换为LLVM IR，支持完整的PawLang语言特性：
 * - 泛型系统（函数、Struct、Enum）- 完整单态化
 * - 泛型struct内部方法（静态、实例）
 * - 模块系统和跨模块调用
 * - 错误处理（T?类型、?操作符、ok/err）
 * - 模式匹配（is表达式、match表达式）
 * - 类型推导和类型转换
 * - Struct引用语义（统一指针传递）
 * - 数组（定长、不定长、多维）
 * - 字符串和字符类型
 * - 循环控制（4种loop + break/continue）
 * 
 * 文件组织：
 * - 第1部分：初始化和核心接口
 * - 第2部分：类型转换系统
 * - 第3部分：表达式生成
 * - 第4部分：语句生成
 * - 第5部分：泛型实例化
 * 
 * @note 文件大小：3936行（计划未来拆分）
 * @todo 拆分为多个文件：expr、stmt、type、struct、match
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
// 第1部分：初始化和核心接口
// ============================================================================

/**
 * @brief 构造CodeGenerator（单文件模式）
 * @param module_name 模块名称
 * 
 * 初始化LLVM组件：
 * - LLVMContext：独立的编译上下文
 * - Module：IR模块容器
 * - IRBuilder：IR指令构建器
 * - Builtins：内置函数管理器
 * - DataLayout：目标平台数据布局（确保正确对齐）
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
    
    // 设置目标平台的DataLayout（确保所有类型使用正确对齐）
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
    
    // 声明所有内置函数
    builtins_->declareAll();
}

/**
 * @brief 构造CodeGenerator（多文件模式）
 * @param module_name 模块名称
 * @param symbol_table 符号表指针（用于跨模块查找）
 * 
 * 与单文件模式相同，但额外支持：
 * - 跨模块类型查找
 * - 跨模块函数调用
 * - 泛型跨模块实例化
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
    
    // 设置目标平台的DataLayout（确保所有类型使用正确对齐）
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
    
    // 声明所有内置函数
    builtins_->declareAll();
}

/**
 * @brief 生成LLVM IR代码（主入口）
 * @param program AST程序节点
 * @return true 成功，false 验证失败
 * 
 * 采用两遍编译：
 * 1. 第一遍：注册所有类型定义（Struct和Enum）
 *    - 允许函数签名引用这些类型
 *    - 处理泛型定义注册
 * 2. 第二遍：生成函数、语句等
 *    - 类型已完全可用
 *    - 支持前向引用
 * 
 * 最后使用llvm::verifyModule()验证生成的IR正确性。
 */
bool CodeGenerator::generate(const Program& program) {
    // 第一遍：注册所有类型定义（Struct和Enum）
    // 这样函数签名中可以引用这些类型
    for (const auto& stmt : program.statements) {
        if (stmt->kind == Stmt::Kind::Struct) {
            generateStructStmt(static_cast<const StructStmt*>(stmt.get()));
        } else if (stmt->kind == Stmt::Kind::Enum) {
            generateEnumStmt(static_cast<const EnumStmt*>(stmt.get()));
        }
    }
    
    // 第二遍：生成其他语句（函数等）
    for (const auto& stmt : program.statements) {
        // 跳过已经处理的类型定义
        if (stmt->kind != Stmt::Kind::Struct && stmt->kind != Stmt::Kind::Enum) {
            generateStmt(stmt.get());
        }
    }
    
    // 验证模块
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
    // 只初始化本地目标（不是All）
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();
    
    // 使用 Triple 对象 (LLVM 21+)
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
    // 使用新API (LLVM 21+)
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
