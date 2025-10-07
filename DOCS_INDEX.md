# Paw 文档索引

## 📖 核心文档（必读）

### 1. [README.md](README.md) - 项目主页
- **阅读时间：10分钟**
- 项目概述和核心特点
- 快速示例和对比
- 从这里开始了解 Paw

### 2. [START_HERE.md](START_HERE.md) ⭐
- **阅读时间：5分钟**
- 最快速的入门指南
- 5分钟学习路径
- 核心概念速览

### 3. [CHEATSHEET.md](CHEATSHEET.md) ⭐
- **阅读时间：10分钟**
- 一页纸语法速查
- 所有语法模式
- 可打印参考卡

---

## 📚 语法文档

### 4. [SYNTAX.md](SYNTAX.md) ⭐
- **阅读时间：1小时**
- 完整的语法规范
- 详细代码示例
- 所有语言特性

### 5. [SEMICOLON_RULES.md](SEMICOLON_RULES.md)
- **阅读时间：15分钟**
- 分号使用规则（与 Rust 一致）
- 常见错误和解决方案
- 代码风格指南

### 6. [MODULE_SYSTEM.md](MODULE_SYSTEM.md)
- **阅读时间：20分钟**
- 文件即模块系统
- import 导入语法
- 项目组织最佳实践

### 7. [VISIBILITY_GUIDE.md](VISIBILITY_GUIDE.md)
- **阅读时间：15分钟**
- pub 可见性控制
- 默认私有，显式公开
- 封装和 API 设计

---

## 🔬 深度分析

### 8. [VISUAL_COMPARISON.md](VISUAL_COMPARISON.md)
- **阅读时间：30分钟**
- Rust vs Paw 详细对比
- 真实项目案例
- 代码量和性能分析

### 9. [READABILITY_ANALYSIS.md](READABILITY_ANALYSIS.md)
- **阅读时间：20分钟**
- 可读性量化研究
- 认知负荷分析
- 学习曲线对比

### 10. [DESIGN.md](DESIGN.md)
- **阅读时间：30分钟**
- 设计理念和哲学
- 技术决策过程
- 核心优势总结

---

## 🎯 按需求导航

### 我想快速了解 Paw
→ [README.md](README.md) (10分钟)

### 我想立即开始编码
→ [START_HERE.md](START_HERE.md) (5分钟)  
→ [CHEATSHEET.md](CHEATSHEET.md) (10分钟)  
→ examples/hello.paw (动手)

### 我想系统学习语法
→ [SYNTAX.md](SYNTAX.md) (1小时)  
→ [SEMICOLON_RULES.md](SEMICOLON_RULES.md) (15分钟)  
→ examples/ 所有示例 (2小时)

### 我需要了解模块系统
→ [MODULE_SYSTEM.md](MODULE_SYSTEM.md) (20分钟)  
→ examples/module_example.paw

### 我需要了解可见性
→ [VISIBILITY_GUIDE.md](VISIBILITY_GUIDE.md) (15分钟)  
→ examples/visibility.paw

### 我来自 Rust 背景
→ [VISUAL_COMPARISON.md](VISUAL_COMPARISON.md) - 看差异  
→ [SEMICOLON_RULES.md](SEMICOLON_RULES.md) - 分号规则一致  
→ [SYNTAX.md](SYNTAX.md) - 快速掌握

### 我想深入理解设计
→ [DESIGN.md](DESIGN.md)  
→ [READABILITY_ANALYSIS.md](READABILITY_ANALYSIS.md)

---

## 📂 示例代码索引

| 示例 | 特性 | 难度 | 文件 |
|------|------|------|------|
| Hello World | 基础语法、分号、字符串插值 | ⭐ | hello.paw |
| 斐波那契 | 递归、迭代、loop | ⭐⭐ | fibonacci.paw |
| 结构体方法 | struct、方法、mut self | ⭐⭐ | struct_methods.paw |
| 模式匹配 | enum、is 匹配 | ⭐⭐⭐ | pattern_matching.paw |
| 错误处理 | Result、? 操作符 | ⭐⭐⭐ | error_handling.paw |
| 循环 | loop 统一语法 | ⭐⭐ | loops.paw |
| 可见性 | pub 控制 | ⭐⭐⭐ | visibility.paw |
| 模块系统 | import、文件模块 | ⭐⭐⭐ | module_example.paw |
| Web API | 完整应用 | ⭐⭐⭐⭐ | complete_example.paw |

---

## 🎓 推荐学习路径

### 快速入门（1天）

```
上午 (2h)
  ├─ README.md (10分钟)
  ├─ START_HERE.md (20分钟)
  ├─ CHEATSHEET.md (30分钟)
  └─ examples/hello.paw (1小时)

下午 (2h)
  ├─ SEMICOLON_RULES.md (30分钟)
  ├─ examples/fibonacci.paw (30分钟)
  └─ examples/struct_methods.paw (1小时)

晚上 (2h)
  ├─ examples/pattern_matching.paw
  ├─ examples/error_handling.paw
  └─ examples/loops.paw
```

### 系统学习（3天）

```
Day 1: 核心语法
  └─ SYNTAX.md + 基础示例

Day 2: 高级特性
  └─ MODULE_SYSTEM + VISIBILITY_GUIDE + 高级示例

Day 3: 实战
  └─ complete_example.paw 分析和修改
```

### 深入理解（选修）

```
选修 1: 对比研究
  └─ VISUAL_COMPARISON.md

选修 2: 设计分析
  └─ READABILITY_ANALYSIS.md + DESIGN.md
```

---

## 🔍 主题索引

### 语法主题

| 主题 | 主要文档 | 示例代码 |
|------|---------|----------|
| 变量和常量 | SYNTAX.md §1 | hello.paw |
| 类型定义 | SYNTAX.md §1 | struct_methods.paw |
| 函数和方法 | SYNTAX.md §2 | struct_methods.paw |
| 控制流 | SYNTAX.md §3 | loops.paw |
| 模式匹配 | SYNTAX.md §3 | pattern_matching.paw |
| 错误处理 | SYNTAX.md §4 | error_handling.paw |
| 异步编程 | SYNTAX.md §5 | complete_example.paw |
| 所有权借用 | SYNTAX.md §6 | SYNTAX.md 示例 |
| 泛型 | SYNTAX.md §8 | pattern_matching.paw |

### 系统主题

| 主题 | 文档 |
|------|------|
| 分号规则 | SEMICOLON_RULES.md |
| 模块系统 | MODULE_SYSTEM.md |
| 可见性 | VISIBILITY_GUIDE.md |
| 关键字 | SYNTAX.md §开头 |

---

## 💡 快速查找

### 我想知道...

**如何声明变量？**
→ CHEATSHEET.md 或 SYNTAX.md §1

**分号什么时候需要？**
→ SEMICOLON_RULES.md ⭐

**如何组织模块？**
→ MODULE_SYSTEM.md ⭐

**pub 怎么使用？**
→ VISIBILITY_GUIDE.md

**与 Rust 有什么区别？**
→ VISUAL_COMPARISON.md

**为什么这样设计？**
→ DESIGN.md

---

## 📊 文档质量

| 文档 | 完整度 | 易读性 | 实用性 |
|------|--------|--------|--------|
| START_HERE.md | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| CHEATSHEET.md | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| SYNTAX.md | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| SEMICOLON_RULES.md | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| MODULE_SYSTEM.md | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| VISIBILITY_GUIDE.md | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |

---

## 🚀 立即开始

```bash
# 最快路径（30分钟）
cat START_HERE.md        # 5分钟
cat CHEATSHEET.md        # 10分钟
cat examples/hello.paw   # 5分钟
zig build && ./zig-out/bin/pawc examples/hello.paw -o hello
./hello                  # 运行

# 系统学习（1周）
Day 1: START_HERE + CHEATSHEET + 基础示例
Day 2-3: SYNTAX + SEMICOLON_RULES + MODULE_SYSTEM
Day 4-7: 所有示例 + 实战项目
```

---

**从 [START_HERE.md](START_HERE.md) 开始你的 Paw 之旅！** 🚀
