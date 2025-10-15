#pragma once
#include <string>

namespace pawc {

class LLVMDownloader {
public:
    LLVMDownloader(const std::string& install_dir = "llvm");
    
    // 检查LLVM是否已安装
    bool isInstalled() const;
    
    // 下载并安装LLVM
    bool downloadAndInstall(bool verbose = false);
    
    // 获取LLVM目录
    std::string getLLVMDir() const;
    
private:
    std::string install_dir_;
    std::string version_;
    
    // 检测平台
    std::string detectPlatform() const;
    
    // 下载文件
    bool downloadFile(const std::string& url, const std::string& output_file, bool verbose) const;
    
    // 解压文件
    bool extractArchive(const std::string& archive_path, const std::string& dest_dir) const;
};

} // namespace pawc

