/**
 * @file pass_manager.h
 * @brief Pass 管理器
 * 
 * 管理和执行编译器 Pass 序列。
 * 
 * @version 0.3.0-dev (Phase 4)
 * @date 2025-10-27
 */

#ifndef PAWC_PASS_MANAGER_H
#define PAWC_PASS_MANAGER_H

#include "compiler_pass.h"
#include <vector>
#include <memory>

namespace pawc {

/**
 * Pass 管理器
 * 
 * 负责管理和执行编译器 Pass 序列。
 * 特性：
 * - 按顺序执行 Pass
 * - 任何 Pass 失败则停止
 * - 日志记录（可选）
 */
class PassManager {
public:
    PassManager(bool verbose = false) : verbose_(verbose) {}
    
    /**
     * 添加 Pass
     */
    void addPass(std::unique_ptr<CompilerPass> pass) {
        passes_.push_back(std::move(pass));
    }
    
    /**
     * 运行所有 Pass
     * 
     * @param unit 编译单元
     * @return true 所有 Pass 成功，false 任一 Pass 失败
     */
    bool runAll(CompilationUnit* unit);
    
    /**
     * 获取所有 Pass
     */
    const std::vector<std::unique_ptr<CompilerPass>>& getPasses() const {
        return passes_;
    }
    
    /**
     * 设置是否输出详细日志
     */
    void setVerbose(bool verbose) { verbose_ = verbose; }
    
private:
    std::vector<std::unique_ptr<CompilerPass>> passes_;
    bool verbose_;
};

} // namespace pawc

#endif // PAWC_PASS_MANAGER_H

