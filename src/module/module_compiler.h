#ifndef PAWC_MODULE_COMPILER_H
#define PAWC_MODULE_COMPILER_H

#include "module_loader.h"
#include "symbol_table.h"
#include "../codegen/codegen.h"
#include <string>
#include <memory>

namespace pawc {

/**
 * 模块编译器
 * 
 * 整合ModuleLoader、SymbolTable和CodeGenerator，实现完整的模块编译流程：
 * 1. 加载所有模块及其依赖
 * 2. 按拓扑顺序编译各模块
 * 3. 合并所有模块的LLVM IR
 * 4. 生成最终可执行文件
 */
class ModuleCompiler {
public:
    ModuleCompiler(const std::string& base_dir);
    
    /**
     * 设置 clang 编译器路径
     * @param clang_path clang++ 可执行文件的完整路径
     */
    void setClangPath(const std::string& clang_path);
    
    /**
     * 编译多文件项目
     * @param main_file 主文件路径（如 "main.paw"）
     * @param output_file 输出文件路径（如 "main"）
     * @return 是否成功
     */
    bool compile(const std::string& main_file, const std::string& output_file);
    
    /**
     * 输出所有模块的LLVM IR（调试用）
     */
    void printAllIR() const;
    
    /**
     * 输出符号表（调试用）
     */
    void dumpSymbolTable() const;
    
private:
    std::string base_directory_;
    std::string clang_path_;  // clang++ 编译器路径
    std::unique_ptr<ModuleLoader> loader_;
    std::unique_ptr<SymbolTable> symbol_table_;
    std::vector<std::unique_ptr<CodeGenerator>> generators_;
    
    /**
     * 合并所有模块的LLVM IR到一个Module
     */
    bool linkModules(const std::string& output_file);
    
    /**
     * 编译单个模块
     */
    bool compileModule(ModuleInfo* module);
};

} // namespace pawc

#endif // PAWC_MODULE_COMPILER_H

