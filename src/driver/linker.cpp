//===--- linker.cpp - Linker Implementation ----------------------*- C++ -*-===//

#include "linker.h"

#include <cstdlib>
#include <sstream>
#include <iostream>

namespace pawc {

Linker::Linker() : runtime_path_(""), verbose_(false) {}

bool Linker::link(const std::vector<std::string>& object_files,
                  const std::string& output_file,
                  const std::vector<std::string>& library_paths,
                  const std::vector<std::string>& libraries) {
    
    if (object_files.empty()) {
        error_ = "No object files provided";
        return false;
    }
    
    // 构建链接命令
    auto args = buildLinkCommand(object_files, output_file, library_paths, libraries);
    
    // 执行链接
    return invokeSystemLinker(args);
}

std::vector<std::string> Linker::buildLinkCommand(
    const std::vector<std::string>& object_files,
    const std::string& output_file,
    const std::vector<std::string>& library_paths,
    const std::vector<std::string>& libraries) {
    
    std::vector<std::string> args;
    
    // 使用系统默认链接器（macOS用ld, Linux可用ld.lld）
#ifdef __APPLE__
    args.push_back("ld");
#else
    args.push_back("ld.lld");
#endif
    
    // 输出文件
    args.push_back("-o");
    args.push_back(output_file);
    
    // 对象文件
    for (const auto& obj : object_files) {
        args.push_back(obj);
    }
    
    // Runtime库
    if (!runtime_path_.empty()) {
        args.push_back(runtime_path_ + "/libpawc_runtime.a");
    }
    
    // 库搜索路径
    for (const auto& path : library_paths) {
        args.push_back("-L" + path);
    }
    
    // 库
    for (const auto& lib : libraries) {
        args.push_back("-l" + lib);
    }
    
    // 系统库和启动文件
#ifdef __APPLE__
    // macOS特定
    args.push_back("-lSystem");
    args.push_back("-syslibroot");
    args.push_back("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk");
#else
    // Linux
    args.push_back("-lc");
    args.push_back("-lm");
    args.push_back("-lpthread");
#endif
    
    return args;
}

bool Linker::invokeSystemLinker(const std::vector<std::string>& args) {
    // 构建命令字符串
    std::stringstream cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd << " ";
        cmd << args[i];
    }
    
    std::string command = cmd.str();
    
    if (verbose_) {
        std::cout << "   Linking command: " << command << "\n";
    }
    
    // 执行链接命令
    int result = std::system(command.c_str());
    
    if (result != 0) {
        error_ = "Linker failed with exit code: " + std::to_string(result);
        if (verbose_) {
            std::cerr << "   Link command: " << command << "\n";
        }
        return false;
    }
    
    if (verbose_) {
        std::cout << "   Linking successful\n";
    }
    
    return true;
}

} // namespace pawc

