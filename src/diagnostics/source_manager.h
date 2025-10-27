/**
 * @file source_manager.h
 * @brief Source file manager for diagnostic reporting
 * 
 * Manages source files and provides line/column lookup for error reporting.
 * 
 * @version 0.2.3
 * @date 2025-10-27
 */

#ifndef PAWC_SOURCE_MANAGER_H
#define PAWC_SOURCE_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace pawc {

/**
 * 源文件信息
 */
struct SourceFile {
    std::string path;               // 文件路径
    std::string content;            // 文件内容
    std::vector<size_t> line_offsets;  // 每行的起始偏移量
    
    SourceFile(const std::string& p, const std::string& c);
    
    /**
     * 获取指定行的内容
     */
    std::string getLine(int line_number) const;
    
    /**
     * 获取总行数
     */
    int getLineCount() const {
        return static_cast<int>(line_offsets.size());
    }
};

/**
 * 源码管理器
 */
class SourceManager {
public:
    SourceManager() = default;
    
    /**
     * 加载源文件
     */
    bool loadFile(const std::string& file_path);
    
    /**
     * 添加源文件内容（用于内存中的源码）
     */
    void addSource(const std::string& file_path, const std::string& content);
    
    /**
     * 获取源文件
     */
    const SourceFile* getFile(const std::string& file_path) const;
    
    /**
     * 获取指定位置的行内容
     */
    std::string getSourceLine(const std::string& file, int line) const;
    
    /**
     * 检查文件是否已加载
     */
    bool hasFile(const std::string& file_path) const {
        return files_.find(file_path) != files_.end();
    }
    
private:
    std::map<std::string, std::unique_ptr<SourceFile>> files_;
};

} // namespace pawc

#endif // PAWC_SOURCE_MANAGER_H

