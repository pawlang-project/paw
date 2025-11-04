# 高级模式匹配实现计划

## 🎯 目标

实现4个高级模式：
1. 范围模式 (Range Pattern)
2. OR 模式 (Or Pattern)
3. 嵌套模式完善
4. 中间 Rest 模式

## 📝 实现顺序

### 1. 范围模式 (Range Pattern)
**语法**: `1..10`, `'a'..'z'`, `1..=10`

**AST**:
```cpp
class RangePattern : public Pattern {
    PatternPtr start;    // 起始值
    PatternPtr end;      // 结束值
    bool inclusive;      // 是否包含结束值
};
```

### 2. OR 模式 (Or Pattern)
**语法**: `1 | 2 | 3`, `Color::red | Color::blue`

**AST**:
```cpp
class OrPattern : public Pattern {
    std::vector<PatternPtr> alternatives;  // 多个可选模式
};
```

### 3. 嵌套模式优化
**已支持，需测试和优化**

### 4. 中间 Rest 模式
**语法**: `[first, .., last]`, `[a, ..middle, z]`

**扩展**: SlicePattern 支持后缀和中间位置

---

## 开始实现...

