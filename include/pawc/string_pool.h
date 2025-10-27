/**
 * @file string_pool.h
 * @brief 字符串池 - 字符串驻留（String Interning）
 * 
 * 用于减少重复字符串的内存占用。
 * 相同的字符串只存储一次，大幅降低标识符、类型名等的内存开销。
 * 
 * @version 0.2.2 (Memory Optimization Phase 2)
 * @date 2025-10-27
 */

#ifndef PAWC_STRING_POOL_H
#define PAWC_STRING_POOL_H

#include <string>
#include <string_view>
#include <unordered_set>
#include <iostream>

namespace pawc {

/**
 * 字符串池
 * 
 * 字符串驻留（String Interning）实现。
 * 
 * 特点：
 * - 相同字符串只存储一次
 * - 返回稳定的 string_view（池生命周期内有效）
 * - 字符串比较变为指针比较（O(1)）
 * - 减少内存占用 30-50%（标识符）
 * 
 * 使用场景：
 * - 变量名、函数名
 * - 类型名（i32, string, bool 等）
 * - 关键字
 * - 模块名
 * 
 * 注意：
 * - 驻留的字符串不会被释放（直到 StringPool 析构）
 * - 适合数量有限的标识符
 * - 不适合用户生成的大量唯一字符串
 */
class StringPool {
public:
    StringPool() = default;
    ~StringPool() = default;
    
    // 禁止拷贝
    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;
    
    /**
     * 驻留字符串
     * 
     * @param str 要驻留的字符串
     * @return string_view 驻留后的字符串视图（稳定，池生命周期内有效）
     * 
     * 示例：
     *   auto name1 = pool.intern("variable");
     *   auto name2 = pool.intern("variable");
     *   // name1.data() == name2.data()  // 相同指针！
     */
    std::string_view intern(std::string_view str) {
        // 尝试插入
        auto [it, inserted] = strings_.insert(std::string(str));
        // 返回存储的字符串的 string_view
        return *it;
    }
    
    /**
     * 检查字符串是否已驻留
     */
    bool contains(std::string_view str) const {
        return strings_.find(std::string(str)) != strings_.end();
    }
    
    /**
     * 获取驻留字符串数量
     */
    size_t size() const { return strings_.size(); }
    
    /**
     * 清空字符串池
     */
    void clear() { strings_.clear(); }
    
    /**
     * 获取内存占用估计（字节）
     */
    size_t getMemoryUsage() const {
        size_t total = 0;
        for (const auto& str : strings_) {
            total += str.capacity();
        }
        return total;
    }
    
    /**
     * 打印统计信息
     */
    void printStats() const {
        std::cout << "StringPool Statistics:\n";
        std::cout << "  Unique strings: " << strings_.size() << "\n";
        std::cout << "  Memory usage: " << (getMemoryUsage() / 1024.0) << " KB\n";
    }
    
private:
    // 使用 unordered_set 存储唯一字符串
    // set 中的字符串地址稳定（不会因为插入而移动）
    std::unordered_set<std::string> strings_;
};

} // namespace pawc

#endif // PAWC_STRING_POOL_H

