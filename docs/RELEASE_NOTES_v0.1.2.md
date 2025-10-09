# PawLang v0.1.2 Release Notes

**发布日期**: 2024年10月8日  
**版本**: v0.1.2  
**主题**: 🎉 完整泛型方法系统

---

## 🌟 核心功能

### 泛型方法系统

PawLang v0.1.2 实现了**完整的泛型方法系统**，包括：

1. **静态方法（关联函数）** - 使用 `::` 语法
2. **实例方法** - 使用 `.` 语法，**self参数不需要类型！**

---

## ✨ 新特性

### 1. 泛型静态方法

**语法**：`Type<T>::method()`

```paw
type Vec<T> = struct {
    ptr: i32,
    len: i32,
    cap: i32,
    
    // 静态方法：无self参数
    fn new() -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: 0 };
    }
    
    fn with_capacity(capacity: i32) -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: capacity };
    }
}

// 调用
let vec: Vec<i32> = Vec<i32>::new();
let vec2: Vec<i32> = Vec<i32>::with_capacity(10);
```

### 2. 泛型实例方法 ⭐

**语法**：`instance.method()`  
**亮点**：`self`参数**不需要写类型**！

```paw
type Vec<T> = struct {
    ptr: i32,
    len: i32,
    
    // 实例方法：self不带类型！
    fn length(self) -> i32 {
        return self.len;
    }
    
    fn capacity_method(self) -> i32 {
        return self.cap;
    }
}

// 调用
let vec: Vec<i32> = Vec<i32>::with_capacity(5);
let len: i32 = vec.length();           // 使用 . 语法
let cap: i32 = vec.capacity_method();  // 自动推导self类型
```

### 3. 多类型参数支持

同时支持多个不同类型的泛型实例：

```paw
// Vec<i32>
let vec_int: Vec<i32> = Vec<i32>::new();
let len1: i32 = vec_int.length();

// Vec<string>
let vec_str: Vec<string> = Vec<string>::new();
let len2: i32 = vec_str.length();

// Box<i32>
let box_int: Box<i32> = Box<i32>::new(42);

// Box<string>
let box_str: Box<string> = Box<string>::new("hello");
```

---

## 🔧 技术实现

### 核心组件

1. **Parser扩展**
   - 支持`Type<T>::method()`语法（静态方法）
   - 支持`self`参数自动类型推导
   - 自动将`self`类型设置为当前struct

2. **Monomorphizer扩展**
   - `GenericMethodInstance`结构
   - 自动收集所有方法调用
   - 为每个类型参数组合生成独立实例

3. **CodeGen扩展**
   - 生成单态化方法的C函数
   - 自动将self转换为指针参数
   - 正确的name mangling（Vec_i32_method）

### 生成的C代码示例

**PawLang代码**：
```paw
fn length(self) -> i32 {
    return self.len;
}

let vec: Vec<i32> = Vec<i32>::with_capacity(5);
let len: i32 = vec.length();
```

**生成的C代码**：
```c
// 方法定义
int32_t Vec_i32_length(Vec_i32* self) {
    return self->len;
}

// 调用
Vec_i32 vec = Vec_i32_with_capacity(5);
int32_t len = Vec_i32_length(&vec);
```

---

## 📊 标准库更新

### Vec<T> 新增方法

**静态方法**：
- `Vec<T>::new()` - 创建空Vec
- `Vec<T>::with_capacity(capacity: i32)` - 创建指定容量的Vec

**实例方法**：
- `vec.length()` - 获取长度
- `vec.capacity_method()` - 获取容量

### Box<T> 新增方法

**静态方法**：
- `Box<T>::new(value: T)` - 创建Box

---

## 🎯 测试结果

### 测试覆盖

✅ **静态方法**：
- Vec<i32>::new()
- Vec<i32>::with_capacity()
- Vec<string>::new()
- Box<i32>::new()
- Box<string>::new()

✅ **实例方法**：
- vec.length()
- vec.capacity_method()
- 多类型参数（i32, string）

✅ **C代码编译**：
- gcc编译无警告
- 程序运行成功
- 所有测试通过

### 运行示例

```bash
$ ./zig-out/bin/pawc tests/test_methods_complete.paw
Compilation complete: tests/test_methods_complete.paw -> output.c (0.00s)
✅ C code generated: output.c

$ gcc output.c -o output
$ ./output
=== PawLang v0.1.2 完整泛型方法测试 ===

[组1] Vec<i32> 静态方法
  ✓ Vec<i32>::new()
  ✓ Vec<i32>::with_capacity(10)

[组1] Vec<i32> 实例方法
  ✓ vec.length()
  ✓ vec.capacity_method()

[组2] Vec<string> 静态方法
  ✓ Vec<string>::new()

[组2] Vec<string> 实例方法
  ✓ vec_str.length()

[组3] Box<i32> 静态方法
  ✓ Box<i32>::new(42)
  ✓ Box<i32>::new(100)

[组4] Box<string> 静态方法
  ✓ Box<string>::new(hello)

=== ✅ 所有测试通过! PawLang泛型方法系统完成! ===
```

---

## 🎨 设计亮点

### 1. self参数无需类型 ⭐

这是PawLang的独特设计！在方法定义中，`self`参数**不需要显式写类型**：

```paw
// ❌ 不需要这样写
fn length(self: Vec<T>) -> i32 { ... }

// ✅ 直接这样写
fn length(self) -> i32 { ... }
```

编译器会自动推导`self`的类型为当前struct。

### 2. 零运行时开销

所有泛型方法在编译时完全单态化：
- `Vec<i32>::new()` → `Vec_i32_new()`
- `Vec<string>::new()` → `Vec_string_new()`
- 每个类型参数组合生成独立的C函数
- 没有虚函数表，没有动态分发

### 3. 类型安全

- 编译时检查所有类型
- 方法调用自动验证类型匹配
- self参数自动转换为指针

---

## 📈 性能

- **编译速度**: <0.01s (典型Paw程序)
- **生成代码**: 高质量C代码
- **运行时**: 零开销抽象

---

## 📝 已知限制

1. **需要类型标注**  
   目前需要显式写变量类型：
   ```paw
   let vec: Vec<i32> = Vec<i32>::new();  // 需要类型标注
   ```
   
   未来改进：支持类型推导，允许：
   ```paw
   let vec = Vec<i32>::new();  // 自动推导类型
   ```

2. **内存泄漏警告**  
   Parser中有一些内存泄漏警告（不影响功能）
   将在v0.1.3中修复

---

## 🔄 从v0.1.1升级

### 新增语法

```paw
// v0.1.1: 只能这样初始化
let vec = Vec { ptr: 0, len: 0, cap: 0 };

// v0.1.2: 可以使用方法
let vec: Vec<i32> = Vec<i32>::new();
let len: i32 = vec.length();
```

### 向后兼容

v0.1.2 完全向后兼容 v0.1.1，所有旧代码仍然可以正常工作。

---

## 📚 代码统计

### 新增代码
- `src/generics.zig`: +150行
- `src/codegen.zig`: +320行
- `src/parser.zig`: +50行
- `src/std/prelude.paw`: +20行

### 新增测试
- `tests/test_static_methods.paw`: 35行
- `tests/test_instance_methods.paw`: 25行
- `tests/test_methods_complete.paw`: 56行

### 文档
- `ROADMAP_v0.1.2.md`: 路线图
- `PROGRESS_v0.1.2.md`: 进度报告
- `CODEGEN_FIXES_SUMMARY.md`: 修复总结
- `RELEASE_NOTES_v0.1.2.md`: 发布说明

---

## 🚀 v0.1.3 展望

计划在下一个版本中实现：

1. **自动类型推导**
   ```paw
   let vec = Vec<i32>::new();  // 不需要类型标注
   ```

2. **泛型约束（Trait Bounds）**
   ```paw
   type Container<T: Display> = struct { ... }
   ```

3. **更多标准库**
   - HashMap<K, V>
   - String
   - 数学函数

4. **性能优化**
   - 减少内存分配
   - 改进编译速度

---

## 🏆 里程碑

**PawLang v0.1.2 是第一个重要里程碑！**

- ✅ 完整的泛型系统
- ✅ 静态方法和实例方法
- ✅ self参数无需类型
- ✅ 零运行时开销
- ✅ 类型安全
- ✅ 标准库扩展

这为PawLang未来的面向对象特性和高级类型系统奠定了坚实的基础！

---

## 🙏 致谢

感谢所有支持PawLang开发的朋友们！

---

## 📖 参考资料

- [ROADMAP_v0.1.2.md](ROADMAP_v0.1.2.md) - 开发路线图
- [CHANGELOG.md](CHANGELOG.md) - 完整变更日志
- [examples/](examples/) - 示例代码

---

**Happy Coding with PawLang! 🐾**

