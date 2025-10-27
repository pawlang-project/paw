# PawLang 内存优化指南

> 📊 **版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 减少内存分配，提升编译器性能

---

## 1. 当前内存分配问题

### 1.1 主要内存分配点

**AST 节点**：
```cpp
// 每个表达式/语句都会 new
auto expr = std::make_unique<BinaryExpr>(...);  // 频繁分配
auto stmt = std::make_unique<IfStmt>(...);      // 频繁分配
```
**问题**：数千个小对象的分配/释放

**字符串**：
```cpp
std::string name = "variable_name";  // 重复的标识符
std::string type = "i32";            // 重复的类型名
```
**问题**：相同字符串多次分配

**容器**：
```cpp
std::vector<Token> tokens;           // 可能需要多次扩容
std::map<std::string, Value*> vars;  // 每次插入都分配节点
```
**问题**：动态扩容导致的重新分配

---

## 2. 优化方案

### 2.1 AST 节点池（Arena Allocator）⭐⭐⭐

**原理**：预分配大块内存，从中分配小对象

#### 实现方案

```cpp
// include/pawc/arena_allocator.h
#ifndef PAWC_ARENA_ALLOCATOR_H
#define PAWC_ARENA_ALLOCATOR_H

#include <vector>
#include <cstddef>

namespace pawc {

/**
 * Arena 内存分配器
 * 
 * 用于快速分配 AST 节点等小对象。
 * 特点：
 * - O(1) 分配
 * - 批量释放（析构时一次性释放所有）
 * - 减少内存碎片
 */
class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t block_size = 64 * 1024)  // 64KB per block
        : block_size_(block_size), current_block_(0), current_offset_(0) {
        allocateBlock();
    }
    
    ~ArenaAllocator() {
        // 自动释放所有内存块
        for (auto& block : blocks_) {
            delete[] block;
        }
    }
    
    // 禁止拷贝
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    
    /**
     * 分配内存
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // 对齐
        size_t padding = (alignment - (current_offset_ % alignment)) % alignment;
        size_t required = size + padding;
        
        // 检查当前块是否有足够空间
        if (current_offset_ + required > block_size_) {
            allocateBlock();
            padding = 0;
            required = size;
        }
        
        void* ptr = blocks_[current_block_] + current_offset_ + padding;
        current_offset_ += required;
        total_allocated_ += required;
        
        return ptr;
    }
    
    /**
     * 便捷的类型化分配
     */
    template<typename T, typename... Args>
    T* make(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }
    
    /**
     * 获取统计信息
     */
    size_t getTotalAllocated() const { return total_allocated_; }
    size_t getBlockCount() const { return blocks_.size(); }
    
    /**
     * 重置（清空但保留内存）
     */
    void reset() {
        current_block_ = 0;
        current_offset_ = 0;
        total_allocated_ = 0;
    }
    
private:
    void allocateBlock() {
        blocks_.push_back(new char[block_size_]);
        current_block_ = blocks_.size() - 1;
        current_offset_ = 0;
    }
    
    std::vector<char*> blocks_;
    size_t block_size_;
    size_t current_block_;
    size_t current_offset_;
    size_t total_allocated_ = 0;
};

} // namespace pawc

#endif // PAWC_ARENA_ALLOCATOR_H
```

#### 使用方法

```cpp
// src/parser/parser.h
class Parser {
public:
    Parser(...) : arena_(std::make_unique<ArenaAllocator>()) {}
    
private:
    std::unique_ptr<ArenaAllocator> arena_;
    
    // 使用 arena 分配 AST 节点
    ExprPtr createBinaryExpr(...) {
        return ExprPtr(arena_->make<BinaryExpr>(...));
    }
};
```

**预期收益**：
- 🚀 分配速度：+50-100%
- 💾 内存开销：-10-20%
- 🔥 缓存友好性：显著提升

---

### 2.2 字符串驻留（String Interning）⭐⭐⭐

**原理**：相同字符串只存储一次

#### 实现方案

```cpp
// include/pawc/string_pool.h
#ifndef PAWC_STRING_POOL_H
#define PAWC_STRING_POOL_H

#include <string>
#include <unordered_set>
#include <string_view>

namespace pawc {

/**
 * 字符串池
 * 
 * 用于字符串驻留，减少重复字符串的内存占用。
 */
class StringPool {
public:
    /**
     * 获取驻留字符串
     * 返回的 string_view 在 StringPool 生命周期内有效
     */
    std::string_view intern(std::string_view str) {
        auto [it, inserted] = strings_.insert(std::string(str));
        return *it;
    }
    
    /**
     * 获取统计信息
     */
    size_t size() const { return strings_.size(); }
    
    /**
     * 清空
     */
    void clear() { strings_.clear(); }
    
private:
    std::unordered_set<std::string> strings_;
};

} // namespace pawc

#endif // PAWC_STRING_POOL_H
```

#### 使用方法

```cpp
// src/lexer/lexer.h
class Lexer {
public:
    Lexer(..., StringPool* string_pool) : string_pool_(string_pool) {}
    
private:
    StringPool* string_pool_;
    
    Token scanIdentifier() {
        std::string name = ...;
        // 驻留字符串
        auto interned = string_pool_->intern(name);
        return Token{..., interned};
    }
};
```

**适用场景**：
- 变量名、函数名
- 类型名（i32, string, bool 等）
- 关键字
- 字符串字面量

**预期收益**：
- 💾 内存节省：30-50%（标识符）
- 🚀 比较速度：指针比较，O(1)
- 🔍 查找优化：哈希查找更快

---

### 2.3 容器预分配⭐⭐

**原理**：预估大小，避免动态扩容

#### 实现方案

```cpp
// src/lexer/lexer.cpp
std::vector<Token> Lexer::tokenize(const std::string& source) {
    // 预估：平均每 5 个字符一个 token
    std::vector<Token> tokens;
    tokens.reserve(source.size() / 5);
    
    // ... 扫描 ...
    
    return tokens;
}

// src/parser/parser.cpp
Program Parser::parse() {
    Program program;
    // 预估：至少 100 个语句
    program.statements.reserve(100);
    
    // ... 解析 ...
    
    return program;
}
```

**预期收益**：
- 🚀 减少重新分配：50-80%
- 💾 减少内存浪费：更紧凑
- ⚡ 性能提升：5-10%

---

### 2.4 移动语义优化⭐⭐

**原理**：避免不必要的拷贝

#### 优化点

```cpp
// ❌ 返回拷贝
std::vector<Token> getTokens() {
    std::vector<Token> tokens;
    // ...
    return tokens;  // 可能拷贝（虽然有 RVO）
}

// ✅ 返回移动
std::vector<Token> getTokens() {
    std::vector<Token> tokens;
    // ...
    return std::move(tokens);  // 明确移动
}

// ✅ 接受右值引用
void process(std::vector<Token>&& tokens) {
    // 直接使用，无拷贝
}

// ✅ unique_ptr 传递
ExprPtr parse() {
    return std::make_unique<Expr>(...);  // 移动，无拷贝
}
```

---

### 2.5 小对象优化（SSO）⭐

**原理**：小对象直接存储，不分配堆内存

#### 自定义 SmallVector

```cpp
// include/pawc/small_vector.h
template<typename T, size_t N>
class SmallVector {
public:
    // 如果元素少于 N，使用栈内存
    // 否则使用堆内存
    
private:
    union {
        T stack_storage_[N];
        T* heap_storage_;
    };
    size_t size_;
    size_t capacity_;
    bool on_heap_;
};
```

**使用场景**：
```cpp
// 大多数函数参数很少（< 8 个）
SmallVector<Type*, 8> param_types;

// 大多数 struct 字段不多（< 16 个）
SmallVector<FieldDecl*, 16> fields;
```

**预期收益**：
- 💾 减少堆分配：50-70%
- 🚀 性能提升：10-20%
- 🔥 缓存友好

---

## 3. 实施优先级

### Phase 1：低成本高收益⭐⭐⭐

**任务**：
- [ ] 容器预分配（2小时）
- [ ] 移动语义审查（2小时）
- [ ] 字符串 string_view 使用（3小时）

**预期收益**：
- 内存：-10-15%
- 性能：+5-10%

### Phase 2：中等成本⭐⭐

**任务**：
- [ ] 实现 StringPool（4小时）
- [ ] 集成到 Lexer/Parser（4小时）

**预期收益**：
- 内存：-20-30%
- 性能：+10-15%

### Phase 3：高成本高收益⭐⭐⭐

**任务**：
- [ ] 实现 ArenaAllocator（6小时）
- [ ] 重构 AST 节点分配（8小时）
- [ ] 测试和验证（4小时）

**预期收益**：
- 内存：-30-40%
- 性能：+20-30%

---

## 4. 内存分析工具

### 4.1 Valgrind（Linux）

```bash
# 内存泄漏检测
valgrind --leak-check=full --show-leak-kinds=all ./pawc test.paw

# 内存分析
valgrind --tool=massif ./pawc test.paw
ms_print massif.out.xxxxx
```

### 4.2 Instruments（macOS）

```bash
# Allocations 工具
instruments -t Allocations ./pawc test.paw

# Leaks 工具
instruments -t Leaks ./pawc test.paw
```

### 4.3 自定义统计

```cpp
// include/pawc/memory_stats.h
class MemoryStats {
public:
    static void recordAllocation(size_t size) {
        total_allocated_ += size;
        allocation_count_++;
    }
    
    static void recordDeallocation(size_t size) {
        total_deallocated_ += size;
        deallocation_count_++;
    }
    
    static void printStats() {
        std::cout << "Total allocated: " << total_allocated_ << " bytes\n";
        std::cout << "Total deallocated: " << total_deallocated_ << " bytes\n";
        std::cout << "Allocation count: " << allocation_count_ << "\n";
        std::cout << "Deallocation count: " << deallocation_count_ << "\n";
        std::cout << "Peak memory: " 
                  << (total_allocated_ - total_deallocated_) << " bytes\n";
    }
    
private:
    static inline size_t total_allocated_ = 0;
    static inline size_t total_deallocated_ = 0;
    static inline size_t allocation_count_ = 0;
    static inline size_t deallocation_count_ = 0;
};
```

---

## 5. 性能对比

### 5.1 预期改进

| 优化 | 内存减少 | 性能提升 | 实施难度 |
|------|----------|----------|----------|
| 容器预分配 | 5-10% | 5-10% | ⭐ 简单 |
| 移动语义 | 5-10% | 5-10% | ⭐ 简单 |
| 字符串池 | 20-30% | 10-15% | ⭐⭐ 中等 |
| Arena分配器 | 30-40% | 20-30% | ⭐⭐⭐ 困难 |
| **总计** | **50-60%** | **30-50%** | - |

### 5.2 测试场景

**小文件（< 100 行）**：
```
优化前：5 MB 内存，0.1s
优化后：3 MB 内存，0.08s
```

**中等文件（500-1000 行）**：
```
优化前：50 MB 内存，1.0s
优化后：25 MB 内存，0.7s
```

**大文件（> 5000 行）**：
```
优化前：300 MB 内存，8.0s
优化后：150 MB 内存，5.5s
```

---

## 6. 代码示例

### 6.1 当前实现

```cpp
// ❌ 频繁的小对象分配
auto expr = std::make_unique<BinaryExpr>(
    std::make_unique<NumberExpr>(1),
    Token{...},
    std::make_unique<NumberExpr>(2)
);
// 3 次堆分配
```

### 6.2 优化后

```cpp
// ✅ Arena 分配
auto expr = arena.make<BinaryExpr>(
    arena.make<NumberExpr>(1),
    Token{...},
    arena.make<NumberExpr>(2)
);
// 从预分配的块中分配，快速且连续
```

---

## 7. 实用建议

### 7.1 开发阶段

**优先使用简单方案**：
- ✅ 容器 reserve()
- ✅ std::move()
- ✅ string_view

**避免过早优化**：
- ❌ 不要一开始就用 Arena
- ❌ 先确保正确性
- ✅ 用 profiler 找瓶颈

### 7.2 性能关键代码

**使用高级技术**：
- ✅ ArenaAllocator
- ✅ StringPool
- ✅ SmallVector
- ✅ 自定义分配器

---

## 8. 注意事项

### 8.1 Arena Allocator 限制

⚠️ **不支持单独释放**：
- Arena 中的对象不能单独 delete
- 只能整体释放
- 适合生命周期相同的对象

⚠️ **需要手动调用析构函数**：
```cpp
// 如果对象需要析构
arena.make<Object>(...);  // 构造
// ... 使用 ...
// 需要显式析构（如果有必要）
```

### 8.2 String Pool 限制

⚠️ **内存持续增长**：
- 驻留的字符串不会被释放
- 适合有限的标识符集合
- 不适合用户生成的大量唯一字符串

---

## 9. 总结

### 当前优先级

**立即实施**（Phase 1）：
1. ✅ 容器预分配
2. ✅ 移动语义优化
3. ✅ string_view 使用

**短期实施**（Phase 2）：
1. 🔄 StringPool 实现
2. 🔄 Lexer/Parser 集成

**长期实施**（Phase 3）：
1. ⏳ ArenaAllocator 实现
2. ⏳ AST 节点分配重构

### 预期总体收益

**内存优化**：
- Phase 1: -10-15%
- Phase 2: -20-30%
- Phase 3: -50-60%

**性能提升**：
- Phase 1: +5-10%
- Phase 2: +10-20%
- Phase 3: +30-50%

---

**🐾 内存优化方案完成！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

---

## 快速启动

想要立即开始？从最简单的优化做起：

```cpp
// 1. 添加 reserve()
std::vector<Token> tokens;
tokens.reserve(source.size() / 5);

// 2. 使用 std::move()
return std::move(large_vector);

// 3. 使用 string_view
void process(std::string_view name);  // 代替 const std::string&
```

**这三个改动可以立即带来 10-15% 的性能提升！** ⚡

