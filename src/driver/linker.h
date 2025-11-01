//===--- linker.h - Linker Interface -----------------------------*- C++ -*-===//
//
// PawLang Compiler - Linker
//
//===----------------------------------------------------------------------===//

#ifndef PAW_LINKER_H
#define PAW_LINKER_H

#include <string>
#include <vector>

namespace pawc {

/// Linker - 链接器封装
///
/// 负责将对象文件链接成可执行文件
/// 使用LLD作为底层链接器
class Linker {
public:
    Linker();
    
    /// 链接对象文件生成可执行文件
    /// \param object_files 对象文件列表
    /// \param output_file 输出可执行文件名
    /// \param library_paths 库搜索路径
    /// \param libraries 需要链接的库
    /// \return true表示成功
    bool link(const std::vector<std::string>& object_files,
              const std::string& output_file,
              const std::vector<std::string>& library_paths = {},
              const std::vector<std::string>& libraries = {});
    
    /// 设置runtime库路径
    void setRuntimePath(const std::string& path) { runtime_path_ = path; }
    
    /// 设置详细输出
    void setVerbose(bool v) { verbose_ = v; }
    
    /// 获取错误信息
    std::string getError() const { return error_; }
    
private:
    std::string runtime_path_;
    std::string error_;
    bool verbose_ = false;
    
    /// 调用系统链接器
    bool invokeSystemLinker(const std::vector<std::string>& args);
    
    /// 构建链接命令
    std::vector<std::string> buildLinkCommand(
        const std::vector<std::string>& object_files,
        const std::string& output_file,
        const std::vector<std::string>& library_paths,
        const std::vector<std::string>& libraries);
};

} // namespace pawc

#endif // PAW_LINKER_H

