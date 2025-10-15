#include "module_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace pawc {

ModuleLoader::ModuleLoader(const std::string& base_path)
    : base_path_(base_path) {}

bool ModuleLoader::loadModule(const std::string& main_file) {
    // 加载主模块
    std::string main_name = "__main__";
    
    if (!loadSingleModule(main_name, main_file)) {
        std::cerr << "Failed to load main module: " << main_file << std::endl;
        return false;
    }
    
    // 递归加载所有依赖
    std::vector<std::string> to_load;
    to_load.push_back(main_name);
    
    while (!to_load.empty()) {
        std::string current = to_load.back();
        to_load.pop_back();
        
        auto it = modules_.find(current);
        if (it == modules_.end()) continue;
        
        for (const auto& dep : it->second.dependencies) {
            // 如果还没加载
            if (modules_.find(dep) == modules_.end()) {
                std::string dep_path = resolveModulePath(dep);
                if (loadSingleModule(dep, dep_path)) {
                    to_load.push_back(dep);
                } else {
                    std::cerr << "Failed to load dependency: " << dep << std::endl;
                    return false;
                }
            }
        }
    }
    
    // 检测循环依赖
    for (const auto& [name, info] : modules_) {
        if (hasCyclicDependency(name)) {
            std::cerr << "Cyclic dependency detected involving: " << name << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string ModuleLoader::resolveModulePath(const std::string& import_path) {
    // "std::math" → "stdlib/std/math.paw"
    // "utils::helper" → "utils/helper.paw"
    
    std::string path = import_path;
    
    // 替换 :: 为 /
    size_t pos = 0;
    while ((pos = path.find("::")) != std::string::npos) {
        path.replace(pos, 2, "/");
    }
    
    // 添加.paw扩展名
    path += ".paw";
    
    // 1. 先尝试标准库路径 stdlib/
    std::string stdlib_path = "stdlib/" + path;
    std::ifstream stdlib_file(stdlib_path);
    if (stdlib_file.good()) {
        stdlib_file.close();
        return stdlib_path;
    }
    
    // 2. 如果有base_path，添加前缀
    if (!base_path_.empty()) {
        path = base_path_ + "/" + path;
    }
    
    return path;
}

bool ModuleLoader::loadSingleModule(const std::string& module_name, const std::string& file_path) {
    // 检查是否已加载
    if (modules_.find(module_name) != modules_.end()) {
        return true;
    }
    
    // 检查循环依赖
    if (loading_.find(module_name) != loading_.end()) {
        std::cerr << "Cyclic dependency: " << module_name << " is already being loaded" << std::endl;
        return false;
    }
    
    loading_.insert(module_name);
    
    // 读取文件
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << file_path << std::endl;
        loading_.erase(module_name);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();
    
    // Lexer
    Lexer lexer(source, file_path);
    auto tokens = lexer.tokenize();
    
    // Parser
    Parser parser(tokens);
    Program ast = parser.parse();
    
    if (!ast.errors.empty()) {
        std::cerr << "Parse errors in " << file_path << std::endl;
        for (const auto& error : ast.errors) {
            std::cerr << "  " << error.location.filename << ":" 
                     << error.location.line << ":" << error.location.column 
                     << ": " << error.message << std::endl;
        }
        loading_.erase(module_name);
        return false;
    }
    
    // 提取依赖
    std::vector<std::string> deps = extractDependencies(ast);
    
    // 保存模块信息
    ModuleInfo info;
    info.name = module_name;
    info.file_path = file_path;
    info.ast = std::move(ast);
    info.dependencies = std::move(deps);
    
    modules_[module_name] = std::move(info);
    loading_.erase(module_name);
    
    return true;
}

std::vector<std::string> ModuleLoader::extractDependencies(const Program& ast) {
    std::vector<std::string> deps;
    
    for (const auto& stmt : ast.statements) {
        if (stmt->kind == Stmt::Kind::Import) {
            const ImportStmt* import_stmt = static_cast<const ImportStmt*>(stmt.get());
            deps.push_back(import_stmt->module_path);
        }
    }
    
    return deps;
}

bool ModuleLoader::hasCyclicDependency(const std::string& module_name) {
    std::set<std::string> visited;
    std::set<std::string> rec_stack;
    
    std::function<bool(const std::string&)> dfs = [&](const std::string& name) -> bool {
        if (rec_stack.find(name) != rec_stack.end()) {
            return true;  // 循环依赖
        }
        
        if (visited.find(name) != visited.end()) {
            return false;  // 已访问
        }
        
        visited.insert(name);
        rec_stack.insert(name);
        
        auto it = modules_.find(name);
        if (it != modules_.end()) {
            for (const auto& dep : it->second.dependencies) {
                if (dfs(dep)) {
                    return true;
                }
            }
        }
        
        rec_stack.erase(name);
        return false;
    };
    
    return dfs(module_name);
}

std::vector<std::string> ModuleLoader::getLoadOrder() const {
    // 拓扑排序：返回依赖顺序
    std::vector<std::string> order;
    std::set<std::string> visited;
    
    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (visited.find(name) != visited.end()) return;
        
        visited.insert(name);
        
        auto it = modules_.find(name);
        if (it != modules_.end()) {
            // 先访问依赖
            for (const auto& dep : it->second.dependencies) {
                visit(dep);
            }
        }
        
        order.push_back(name);
    };
    
    // 从所有模块开始
    for (const auto& [name, info] : modules_) {
        visit(name);
    }
    
    return order;
}

} // namespace pawc

