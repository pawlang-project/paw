/**
 * @file source_manager.cpp
 * @brief Implementation of source file manager
 */

#include "source_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace pawc {

SourceFile::SourceFile(const std::string& p, const std::string& c)
    : path(p), content(c) {
    // 计算每行的起始偏移量
    line_offsets.push_back(0);
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\n') {
            line_offsets.push_back(i + 1);
        }
    }
}

std::string SourceFile::getLine(int line_number) const {
    if (line_number < 1 || line_number > static_cast<int>(line_offsets.size())) {
        return "";
    }
    
    size_t start = line_offsets[line_number - 1];
    size_t end;
    
    if (line_number == static_cast<int>(line_offsets.size())) {
        end = content.size();
    } else {
        end = line_offsets[line_number];
        // 去掉换行符
        if (end > 0 && content[end - 1] == '\n') {
            end--;
        }
        if (end > 0 && content[end - 1] == '\r') {
            end--;
        }
    }
    
    if (start >= content.size()) {
        return "";
    }
    
    return content.substr(start, end - start);
}

bool SourceManager::loadFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    addSource(file_path, content);
    return true;
}

void SourceManager::addSource(const std::string& file_path, const std::string& content) {
    files_[file_path] = std::make_unique<SourceFile>(file_path, content);
}

const SourceFile* SourceManager::getFile(const std::string& file_path) const {
    auto it = files_.find(file_path);
    if (it != files_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::string SourceManager::getSourceLine(const std::string& file, int line) const {
    const SourceFile* source_file = getFile(file);
    if (!source_file) {
        return "";
    }
    return source_file->getLine(line);
}

} // namespace pawc

