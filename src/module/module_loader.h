#ifndef PAWC_MODULE_LOADER_H
#define PAWC_MODULE_LOADER_H

#include "../parser/parser.h"
#include "../lexer/lexer.h"
#include <map>
#include <set>
#include <string>
#include <memory>
#include <filesystem>

namespace pawc {

struct ModuleInfo {
    std::string name;           // 模块名（如"math"）
    std::string file_path;      // 文件路径
    Program ast;                // 解析后的AST
    std::vector<std::string> dependencies;  // 依赖的模块
};

/**
 * 模块加载器
 * 
 * 职责：
 * - 解析import语句
 * - 加载.paw文件
 * - 缓存已加载模块
 * - 解析依赖关系
 */
class ModuleLoader {
public:
    ModuleLoader(const std::string& base_path);
    
    // 加载主模块及其所有依赖
    bool loadModule(const std::string& main_file);
    
    // 获取所有已加载的模块
    const std::map<std::string, ModuleInfo>& getModules() const { return modules_; }
    
    // 获取加载顺序（依赖排序）
    std::vector<std::string> getLoadOrder() const;
    
private:
    std::string base_path_;  // 项目根目录
    std::map<std::string, ModuleInfo> modules_;  // 已加载的模块
    std::set<std::string> loading_;  // 正在加载中（检测循环依赖）
    
    // 解析模块路径：  "utils::helper" → "utils/helper.paw"
    std::string resolveModulePath(const std::string& import_path);
    
    // 加载单个模块
    bool loadSingleModule(const std::string& module_name, const std::string& file_path);
    
    // 提取模块的import依赖
    std::vector<std::string> extractDependencies(const Program& ast);
    
    // 检测循环依赖
    bool hasCyclicDependency(const std::string& module_name);
};

} // namespace pawc

#endif // PAWC_MODULE_LOADER_H

