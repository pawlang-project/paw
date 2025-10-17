#include "llvm_downloader.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

namespace pawc {

LLVMDownloader::LLVMDownloader(const std::string& install_dir)
    : install_dir_(install_dir), version_("21.1.3") {}

bool LLVMDownloader::isInstalled() const {
    std::string config = install_dir_ + "/lib/cmake/llvm/LLVMConfig.cmake";
    struct stat buffer;
    return (stat(config.c_str(), &buffer) == 0);
}

std::string LLVMDownloader::detectPlatform() const {
    std::string os, arch;
    
#ifdef __APPLE__
    os = "macos";
#elif __linux__
    os = "linux";
#elif defined(_WIN32)
    os = "windows";
#else
    os = "unknown";
#endif
    
#if defined(__x86_64__) || defined(_M_X64)
    arch = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    arch = "aarch64";  // 修改: arm64 -> aarch64
#elif defined(__arm__) || defined(_M_ARM)
    arch = "arm";
#elif defined(__i386__) || defined(_M_IX86)
    arch = "x86";
#else
    arch = "unknown";
#endif
    
    return os + "-" + arch;
}

bool LLVMDownloader::downloadFile(const std::string& url, const std::string& output_file, bool verbose) const {
    if (verbose) {
        std::cout << "下载: " << url << std::endl;
        std::cout << "保存: " << output_file << std::endl;
    }
    
    // 使用curl下载
    std::string cmd = "curl -L -# -o \"" + output_file + "\" \"" + url + "\"";
    
#ifdef _WIN32
    if (!verbose) {
        cmd += " 2>nul";
    }
#else
    if (!verbose) {
        cmd += " 2>/dev/null";
    }
#endif
    
    int result = system(cmd.c_str());
    return result == 0;
}

bool LLVMDownloader::extractArchive(const std::string& archive_path, const std::string& dest_dir) const {
    std::cout << "解压到: " << dest_dir << std::endl;
    
    // 创建目录
#ifdef _WIN32
    std::string mkdir_cmd = "if not exist \"" + dest_dir + "\" mkdir \"" + dest_dir + "\"";
#else
    std::string mkdir_cmd = "mkdir -p \"" + dest_dir + "\"";
#endif
    system(mkdir_cmd.c_str());
    
    // 解压tar.gz
#ifdef _WIN32
    // Windows: tar命令会因为符号链接报错，但实际文件已经解压
    // 所以我们先执行tar，然后检查关键文件是否存在
    std::string extract_cmd = "tar -xzf \"" + archive_path + "\" -C \"" + dest_dir + "\" --strip-components=1 2>nul";
    system(extract_cmd.c_str());  // 忽略返回值
    
    // 检查关键文件是否存在来判断解压是否成功
    std::string check_file = dest_dir + "/lib/cmake/llvm/LLVMConfig.cmake";
    struct stat buffer;
    if (stat(check_file.c_str(), &buffer) == 0) {
        // 关键文件存在，解压成功
        return true;
    }
    
    // 如果失败，尝试不带strip-components
    std::cout << "  重试解压..." << std::endl;
    extract_cmd = "tar -xzf \"" + archive_path + "\" -C \"" + dest_dir + "\" 2>nul";
    system(extract_cmd.c_str());
    
    // 再次检查
    check_file = dest_dir + "/llvm-21.1.3-windows-x86_64/lib/cmake/llvm/LLVMConfig.cmake";
    if (stat(check_file.c_str(), &buffer) == 0) {
        // 需要移动文件到正确位置
        std::string move_cmd = "xcopy /E /I /Y \"" + dest_dir + "\\llvm-*\\*\" \"" + dest_dir + "\" >nul && rmdir /S /Q \"" + dest_dir + "\\llvm-*\" >nul";
        system(move_cmd.c_str());
        return true;
    }
    
    return false;
#else
    std::string extract_cmd = "tar -xzf \"" + archive_path + "\" -C \"" + dest_dir + "\" --strip-components=1 2>/dev/null";
    int result = system(extract_cmd.c_str());
    
    if (result != 0) {
        // 尝试不带strip-components
        extract_cmd = "tar -xzf \"" + archive_path + "\" -C \"" + dest_dir + "\"";
        result = system(extract_cmd.c_str());
    }
    
    return result == 0;
#endif
}

bool LLVMDownloader::downloadAndInstall(bool verbose) {
    std::string platform = detectPlatform();
    
    if (platform.find("unknown") != std::string::npos) {
        std::cerr << "不支持的平台: " << platform << std::endl;
        return false;
    }
    
    // 修改文件名格式: pawlang-xxx -> llvm-21.1.3-xxx
    std::string filename = "llvm-" + version_ + "-" + platform + ".tar.gz";
    std::string url = "https://github.com/pawlang-project/llvm-build/releases/download/llvm-" + version_ + "/" + filename;
    
    std::cout << "平台: " << platform << std::endl;
    std::cout << "版本: " << version_ << std::endl;
    std::cout << std::endl;
    
    // 下载
    std::cout << "[1/3] 下载预编译LLVM (~500MB)..." << std::endl;
    if (!downloadFile(url, filename, verbose)) {
        std::cerr << "下载失败" << std::endl;
        return false;
    }
    
    // 解压
    std::cout << std::endl;
    std::cout << "[2/3] 解压..." << std::endl;
    if (!extractArchive(filename, install_dir_)) {
        std::cerr << "解压失败" << std::endl;
        return false;
    }
    
    // 清理
    std::cout << std::endl;
    std::cout << "[3/3] 清理..." << std::endl;
#ifdef _WIN32
    std::string rm_cmd = "del /F /Q \"" + filename + "\" 2>nul";
#else
    std::string rm_cmd = "rm -f \"" + filename + "\"";
#endif
    system(rm_cmd.c_str());
    
    std::cout << std::endl;
    std::cout << "✓ LLVM安装完成！" << std::endl;
    std::cout << "  位置: " << install_dir_ << "/" << std::endl;
    
    return true;
}

std::string LLVMDownloader::getLLVMDir() const {
    return install_dir_ + "/lib/cmake/llvm";
}

} // namespace pawc

