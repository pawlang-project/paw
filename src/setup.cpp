// LLVM自动下载和设置工具
// 这个工具会在CMake配置时自动运行

#include "llvm_downloader.h"
#include <iostream>

int main() {
    pawc::LLVMDownloader downloader("llvm");
    
    // 检查LLVM是否已安装
    if (downloader.isInstalled()) {
        std::cout << "✓ LLVM已安装: llvm/" << std::endl;
        return 0;
    }
    
    // 开始下载
    std::cout << "========================================" << std::endl;
    std::cout << "  PawLang 自动下载预编译LLVM" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "来源: github.com/pawlang-project/llvm-build" << std::endl;
    std::cout << "版本: 21.1.3" << std::endl;
    std::cout << std::endl;
    
    if (downloader.downloadAndInstall(true)) {
        std::cout << std::endl;
        std::cout << "✓ LLVM下载完成！" << std::endl;
        std::cout << "  继续构建项目..." << std::endl;
        return 0;
    } else {
        std::cerr << std::endl;
        std::cerr << "❌ LLVM下载失败" << std::endl;
        std::cerr << std::endl;
        std::cerr << "请手动下载:" << std::endl;
        std::cerr << "  https://github.com/pawlang-project/llvm-build/releases/tag/llvm-21.1.3" << std::endl;
        std::cerr << std::endl;
        return 1;
    }
}

