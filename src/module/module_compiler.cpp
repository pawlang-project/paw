#include "module_compiler.h"
#include "llvm/Linker/Linker.h"
#include <iostream>
#include <filesystem>

using pawc::ModuleInfo;  // Forward declaration from module_loader.h

namespace pawc {

ModuleCompiler::ModuleCompiler(const std::string& base_dir)
    : base_directory_(base_dir) {
    loader_ = std::make_unique<ModuleLoader>(base_dir);
    symbol_table_ = std::make_unique<SymbolTable>();
}

bool ModuleCompiler::compile(const std::string& main_file, const std::string& output_file) {
    // 1. 加载主模块及其所有依赖
    std::cout << "Loading modules..." << std::endl;
    if (!loader_->loadModule(main_file)) {
        std::cerr << "Failed to load modules" << std::endl;
        return false;
    }
    
    const auto& load_order = loader_->getLoadOrder();
    const auto& modules = loader_->getModules();
    std::cout << "Loaded " << load_order.size() << " module(s)" << std::endl;
    
    // 2. 按拓扑顺序编译各模块
    for (const std::string& module_name : load_order) {
        std::cout << "Compiling module: " << module_name << std::endl;
        auto it = modules.find(module_name);
        if (it == modules.end()) {
            std::cerr << "Module not found: " << module_name << std::endl;
            return false;
        }
        if (!compileModule(const_cast<ModuleInfo*>(&it->second))) {
            std::cerr << "Failed to compile module: " << module_name << std::endl;
            return false;
        }
    }
    
    std::cout << "All modules compiled successfully" << std::endl;
    
    // 3. 链接所有模块
    if (!linkModules(output_file)) {
        std::cerr << "Failed to link modules" << std::endl;
        return false;
    }
    
    std::cout << "Compilation successful: " << output_file << std::endl;
    return true;
}

bool ModuleCompiler::compileModule(ModuleInfo* module) {
    // 从模块路径提取短名称
    // 例如: "../../stdlib/std::math" -> "math"
    std::string short_name = module->name;
    
    // 先移除路径部分 (/ 或 \)
    size_t last_slash = short_name.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        short_name = short_name.substr(last_slash + 1);
    }
    
    // 再移除双冒号前的部分 (::)
    // "std::math" -> "math"
    size_t last_colon = short_name.rfind("::");
    if (last_colon != std::string::npos) {
        short_name = short_name.substr(last_colon + 2);
    }
    
    // 移除扩展名
    size_t dot_pos = short_name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        short_name = short_name.substr(0, dot_pos);
    }
    
    // 创建CodeGenerator（传入符号表和短模块名）
    auto generator = std::make_unique<CodeGenerator>(short_name, symbol_table_.get());
    
    // 生成LLVM IR
    if (!generator->generate(module->ast)) {
        return false;
    }
    
    // 保存generator（用于后续链接）
    generators_.push_back(std::move(generator));
    
    return true;
}

bool ModuleCompiler::linkModules(const std::string& output_file) {
    if (generators_.empty()) {
        std::cerr << "No modules to link" << std::endl;
        return false;
    }
    
    // 注意：不能直接链接模块，因为ownership问题
    // 改为：每个模块独立编译为.o，然后用clang链接
    std::vector<std::string> obj_files;
    
    for (size_t i = 0; i < generators_.size(); i++) {
        std::string obj_file = "temp_module_" + std::to_string(i) + ".o";
        if (!generators_[i]->compileToObject(obj_file)) {
            std::cerr << "Failed to compile module " << i << " to object file" << std::endl;
            // 清理
            for (const auto& f : obj_files) {
                std::filesystem::remove(f);
            }
            return false;
        }
        obj_files.push_back(obj_file);
    }
    
    // 使用llvm/bin/clang链接所有.o文件为可执行文件
    std::string clang_path = "llvm/bin/clang";
    if (!std::filesystem::exists(clang_path)) {
        std::cerr << "Error: Clang not found at: " << clang_path << std::endl;
        // 清理
        for (const auto& f : obj_files) {
            std::filesystem::remove(f);
        }
        return false;
    }
    
    // 构建链接命令：clang file1.o file2.o ... -o output
    std::string sdk_flags = " -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk";
    std::string link_cmd = clang_path;
    for (const auto& obj : obj_files) {
        link_cmd += " " + obj;
    }
    link_cmd += sdk_flags + " -o " + output_file;
    
    std::cout << "Linking: " << link_cmd << std::endl;
    
    int link_result = std::system(link_cmd.c_str());
    if (link_result != 0) {
        std::cerr << "Linking failed with exit code: " << link_result << std::endl;
        // 清理
        for (const auto& f : obj_files) {
            std::filesystem::remove(f);
        }
        return false;
    }
    
    // 清理临时文件
    for (const auto& f : obj_files) {
        std::filesystem::remove(f);
    }
    
    return true;
}

void ModuleCompiler::printAllIR() const {
    for (const auto& generator : generators_) {
        std::cout << "\n=== Module: " << generator->getModule()->getName().str() << " ===" << std::endl;
        generator->printIR();
    }
}

void ModuleCompiler::dumpSymbolTable() const {
    symbol_table_->dump();
}

} // namespace pawc

