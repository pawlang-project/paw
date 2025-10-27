/**
 * @file arena_allocator.h
 * @brief Arena 内存分配器 - 快速对象池
 * 
 * 用于快速分配 AST 节点等小对象。
 * 特点：
 * - O(1) 分配速度
 * - 批量释放（析构时一次性释放所有）
 * - 减少内存碎片
 * - 提升缓存局部性
 * 
 * @version 0.2.2 (Memory Optimization Phase 3)
 * @date 2025-10-27
 */

#ifndef PAWC_ARENA_ALLOCATOR_H
#define PAWC_ARENA_ALLOCATOR_H

#include <vector>
#include <cstddef>
#include <new>
#include <iostream>

namespace pawc {

/**
 * Arena 内存分配器
 * 
 * 预分配大块内存（blocks），然后从中分配小对象。
 * 
 * 优点：
 * - 分配速度快：O(1)，只需移动指针
 * - 释放速度快：析构时批量释放
 * - 内存连续：提升 CPU 缓存命中率
 * - 减少碎片：大块分配
 * 
 * 适用场景：
 * - AST 节点（生命周期相同）
 * - 临时对象池
 * - 编译期间的大量小对象
 * 
 * 限制：
 * - 不支持单独释放（只能整体释放）
 * - 对象析构需要手动管理（如有必要）
 * - 适合生命周期相同的对象
 */
class ArenaAllocator {
public:
    /**
     * 构造函数
     * 
     * @param block_size 每个内存块的大小（默认 64KB）
     */
    explicit ArenaAllocator(size_t block_size = 64 * 1024)
        : block_size_(block_size), 
          current_block_(0), 
          current_offset_(0),
          total_allocated_(0) {
        allocateBlock();
    }
    
    /**
     * 析构函数 - 释放所有内存块
     */
    ~ArenaAllocator() {
        for (auto& block : blocks_) {
            delete[] block;
        }
    }
    
    // 禁止拷贝
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    
    // 允许移动
    ArenaAllocator(ArenaAllocator&& other) noexcept
        : blocks_(std::move(other.blocks_)),
          block_size_(other.block_size_),
          current_block_(other.current_block_),
          current_offset_(other.current_offset_),
          total_allocated_(other.total_allocated_) {
        other.current_block_ = 0;
        other.current_offset_ = 0;
        other.total_allocated_ = 0;
    }
    
    /**
     * 分配内存
     * 
     * @param size 要分配的字节数
     * @param alignment 内存对齐要求
     * @return void* 分配的内存指针
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // 计算对齐需要的填充
        size_t padding = (alignment - (current_offset_ % alignment)) % alignment;
        size_t required = size + padding;
        
        // 检查当前块是否有足够空间
        if (current_offset_ + required > block_size_) {
            // 如果对象太大，无法放入标准块
            if (required > block_size_) {
                // 分配一个特殊的大块
                return allocateLargeObject(size, alignment);
            }
            // 否则分配新的标准块
            allocateBlock();
            padding = 0;
            required = size;
        }
        
        // 从当前块分配
        void* ptr = blocks_[current_block_] + current_offset_ + padding;
        current_offset_ += required;
        total_allocated_ += required;
        
        return ptr;
    }
    
    /**
     * 便捷的类型化分配
     * 
     * @tparam T 要分配的类型
     * @tparam Args 构造函数参数类型
     * @param args 构造函数参数
     * @return T* 构造好的对象指针
     * 
     * 示例：
     *   auto expr = arena.make<BinaryExpr>(left, op, right);
     */
    template<typename T, typename... Args>
    T* make(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }
    
    /**
     * 获取总分配字节数
     */
    size_t getTotalAllocated() const { 
        return total_allocated_; 
    }
    
    /**
     * 获取内存块数量
     */
    size_t getBlockCount() const { 
        return blocks_.size(); 
    }
    
    /**
     * 获取当前块使用率
     */
    double getCurrentBlockUtilization() const {
        if (blocks_.empty()) return 0.0;
        return static_cast<double>(current_offset_) / block_size_;
    }
    
    /**
     * 重置（清空但保留内存块）
     * 
     * 注意：对象析构需要手动处理
     */
    void reset() {
        current_block_ = 0;
        current_offset_ = 0;
        total_allocated_ = 0;
        // 保留已分配的块，供重用
    }
    
    /**
     * 打印统计信息
     */
    void printStats() const {
        std::cout << "ArenaAllocator Statistics:\n";
        std::cout << "  Total allocated: " << (total_allocated_ / 1024.0) << " KB\n";
        std::cout << "  Block count: " << blocks_.size() << "\n";
        std::cout << "  Block size: " << (block_size_ / 1024.0) << " KB\n";
        std::cout << "  Current block utilization: " 
                  << (getCurrentBlockUtilization() * 100.0) << "%\n";
        std::cout << "  Average utilization: "
                  << ((total_allocated_ / (blocks_.size() * block_size_)) * 100.0) << "%\n";
    }
    
private:
    /**
     * 分配新的内存块
     */
    void allocateBlock() {
        blocks_.push_back(new char[block_size_]);
        current_block_ = blocks_.size() - 1;
        current_offset_ = 0;
    }
    
    /**
     * 分配大对象（超过block_size）
     */
    void* allocateLargeObject(size_t size, size_t alignment) {
        // 为大对象分配专用块
        size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
        char* large_block = new char[aligned_size];
        large_blocks_.push_back(large_block);
        total_allocated_ += aligned_size;
        return large_block;
    }
    
    std::vector<char*> blocks_;              ///< 标准内存块
    std::vector<char*> large_blocks_;        ///< 大对象专用块
    size_t block_size_;                      ///< 每个块的大小
    size_t current_block_;                   ///< 当前使用的块索引
    size_t current_offset_;                  ///< 当前块的偏移量
    size_t total_allocated_;                 ///< 总分配字节数
};

} // namespace pawc

#endif // PAWC_ARENA_ALLOCATOR_H

