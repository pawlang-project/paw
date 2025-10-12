# 📦 Collections Module

**路径**: `stdlib/collections/`  
**版本**: v0.2.0  
**状态**: 🚧 开发中

---

## ⚠️ 重要提示

**基础泛型容器已在 Prelude 中**，无需 import：

```paw
// ✅ 这些类型自动可用（来自 Prelude）
let v: Vec<i32> = Vec::new();
let b: Box<i32> = Box::new(42);

// ❌ 不需要 import collections!
```

**stdlib/collections 提供的是扩展功能**（未来）。

---

## 📦 当前内容

### Vec<T> 完整实现

**文件**: `vec.paw`  
**状态**: 🚧 开发中（等待 FFI）

```paw
// 注意：基础定义在 Prelude 中
// 这里提供完整的方法实现（需要动态内存支持）

pub type Vec<T> = struct {
    ptr: i64,
    length: i32,
    capacity: i32,
    
    // 扩展方法（需要 FFI）
    pub fn push(mut self, item: T) -> bool
    pub fn pop(mut self) -> Option<T>
    pub fn get(self, index: i32) -> Option<T>
    pub fn clear(mut self) -> i32
}
```

**当前限制**:
- ⚠️ 需要 `paw_malloc` / `paw_free` FFI 支持
- ⚠️ 需要 `paw_write_i32` / `paw_read_i32` FFI 支持

---

## 🔮 计划中的类型

### HashMap<K, V> (v0.3.0)

```paw
pub type HashMap<K, V> = struct {
    buckets: Vec<Vec<Pair<K, V>>>,
    size: i32,
    
    pub fn new() -> HashMap<K, V>
    pub fn insert(mut self, key: K, value: V) -> bool
    pub fn get(self, key: K) -> Option<V>
    pub fn remove(mut self, key: K) -> Option<V>
    pub fn contains(self, key: K) -> bool
}
```

---

### HashSet<T> (v0.3.0)

```paw
pub type HashSet<T> = struct {
    map: HashMap<T, bool>,
    
    pub fn new() -> HashSet<T>
    pub fn insert(mut self, value: T) -> bool
    pub fn contains(self, value: T) -> bool
    pub fn remove(mut self, value: T) -> bool
}
```

---

### LinkedList<T> (v0.4.0)

```paw
pub type LinkedList<T> = struct {
    head: Option<Box<Node<T>>>,
    tail: Option<Box<Node<T>>>,
    len: i32,
    
    pub fn new() -> LinkedList<T>
    pub fn push_front(mut self, value: T) -> i32
    pub fn push_back(mut self, value: T) -> i32
    pub fn pop_front(mut self) -> Option<T>
    pub fn pop_back(mut self) -> Option<T>
}
```

---

## 🆚 Prelude vs Collections

| 类型 | Prelude | Collections |
|------|---------|-------------|
| `Vec<T>` 定义 | ✅ 自动可用 | ❌ 不需要 |
| `Vec<T>` 完整实现 | ❌ | ✅ 此模块 |
| `Box<T>` 定义 | ✅ 自动可用 | ❌ 不需要 |
| `HashMap<K, V>` | ❌ | ✅ 未来 |
| `HashSet<T>` | ❌ | ✅ 未来 |

---

## 💡 使用指南

### 当前可用

```paw
// ✅ 使用 Prelude 中的基础定义
fn main() -> i32 {
    let v: Vec<i32> = Vec::new();
    let len: i32 = v.length();
    let empty: bool = v.is_empty();
    
    let b: Box<i32> = Box::new(42);
    let val: i32 = b.get();
    
    return 0;
}
```

### 未来可用（v0.3.0 - FFI 后）

```paw
import collections;  // ✅ 需要 import 完整实现

fn main() -> i32 {
    let mut v: Vec<i32> = Vec::with_capacity(10);
    
    v.push(1);
    v.push(2);
    v.push(3);
    
    let item: Option<i32> = v.pop();
    
    return v.length();
}
```

---

## 📊 实现状态

| 功能 | Prelude | Collections | FFI | 状态 |
|------|---------|-------------|-----|------|
| Vec::new | ✅ | ✅ | ❌ | ✅ 可用 |
| Vec::length | ✅ | ✅ | ❌ | ✅ 可用 |
| Vec::is_empty | ✅ | ✅ | ❌ | ✅ 可用 |
| Vec::push | ❌ | ✅ | ✅ | ⏳ 等待 FFI |
| Vec::pop | ❌ | ✅ | ✅ | ⏳ 等待 FFI |
| Box::new | ✅ | ✅ | ❌ | ✅ 可用 |
| Box::get | ✅ | ✅ | ❌ | ✅ 可用 |

---

## 🚀 路线图

### v0.2.0 (当前)
- ✅ Vec<T> 和 Box<T> 在 Prelude 中
- ✅ 基础方法可用
- ⏳ 完整实现等待 FFI

### v0.3.0
- ✅ FFI 集成
- ✅ Vec<T> 完整实现（push, pop, get）
- ✅ HashMap<K, V> 基础实现

### v0.4.0
- ✅ HashSet<T>
- ✅ LinkedList<T>
- ✅ BinaryTree<T>

---

## 📚 相关文档

- `src/prelude/prelude.paw` - Vec<T> 和 Box<T> 定义
- `docs/PRELUDE_V0.2.0.md` - Prelude API 文档
- `src/builtin/memory.zig` - 底层内存管理

---

**维护者**: PawLang 核心开发团队  
**最后更新**: 2025-10-12  
**版本**: v0.2.0  
**许可**: MIT

