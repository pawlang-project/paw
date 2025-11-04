# PawLang self参数语法指南

**版本**: v0.2.2 
**日期**: 2025-11-03  
**状态**: ✅ 稳定

---

## 概述

PawLang **只支持**三种self参数简写形式，**不支持显式类型注解**：

| 语法 | 等价类型 | 用途 | 所有权 |
|-----|---------|------|-------|
| `self` | `Self` | 值传递 | 消耗对象 |
| `&self` | `&Self` | 不可变引用 | 只读访问 |
| `&~self` | `&~Self` | 可变引用 | 可修改 |

⚠️ **重要**：不能使用显式注解如`self: Self`或`self: &Self`

---

## 基础用法

### 1. 不可变引用 (`&self`)

**最常用**：只读访问对象字段

```paw
type Point = struct { x: i32, y: i32 };

type Getter = interface {
    fn get_x(&self) -> i32;
    fn get_y(&self) -> i32;
};

support Point with Getter {
    fn get_x(&self) -> i32 {
        return self.x;  // ✅ 读取字段
    }
    
    fn get_y(&self) -> i32 {
        return self.y;
    }
}

fn main() -> void {
    let p = Point { x: 10, y: 20 };
    println(p.get_x());  // 10
    println(p.get_y());  // 20
    // p仍然有效，可以继续使用
}
```

### 2. 可变引用 (`&~self`)

修改对象的字段

```paw
type Counter = struct { value: i32 };

type Incrementer = interface {
    fn increment(&~self) -> void;
    fn add(&~self, n: i32) -> void;
};

support Counter with Incrementer {
    fn increment(&~self) -> void {
        self.value = self.value + 1;  // ✅ 修改字段
    }
    
    fn add(&~self, n: i32) -> void {
        self.value = self.value + n;
    }
}

fn main() -> void {
    let ~counter = Counter { value: 0 };  // ✅ 必须是可变的
    counter.increment();
    counter.add(5);
    println(counter.value);  // 6
}
```

### 3. 值传递 (`self`)

消耗对象（移动语义）

```paw
type Resource = struct { id: i32 };

type Consumer = interface {
    fn consume(self) -> i32;
};

support Resource with Consumer {
    fn consume(self) -> i32 {
        return self.id;  // ✅ 消耗self并返回值
    }
}

fn main() -> void {
    let res = Resource { id: 42 };
    let id = res.consume();
    println(id);  // 42
    // res不再有效，已被消耗
}
```

---

## 完整示例

### 综合示例：计数器类型

```paw
type Counter = struct { value: i32 };

// 定义接口
type CounterOps = interface {
    fn get(&self) -> i32;           // 读取
    fn increment(&~self) -> void;   // 修改
    fn consume(self) -> i32;        // 消耗
};

// 实现接口
support Counter with CounterOps {
    fn get(&self) -> i32 {
        return self.value;
    }
    
    fn increment(&~self) -> void {
        self.value = self.value + 1;
    }
    
    fn consume(self) -> i32 {
        return self.value;
    }
}

fn main() -> void {
    let ~c = Counter { value: 10 };
    
    println("初始值:");
    println(c.get());           // 10
    
    c.increment();
    println("增加后:");
    println(c.get());           // 11
    
    let final_value = c.consume();
    println("最终值:");
    println(final_value);       // 11
    // c已被消耗，不能再使用
}
```

---

## 使用场景

### 何时使用 `&self`

✅ **推荐场景**：
- 读取字段值
- 不修改对象状态
- 对象需要在调用后继续使用
- 大多数getter方法

```paw
fn get_field(&self) -> Type { }
fn calculate(&self) -> Result { }
fn display(&self) -> void { }
```

### 何时使用 `&~self`

✅ **推荐场景**：
- 需要修改字段
- setter方法
- 累加/更新操作
- 对象状态转换

```paw
fn set_field(&~self, value: Type) -> void { }
fn update(&~self) -> void { }
fn reset(&~self) -> void { }
```

### 何时使用 `self`

✅ **推荐场景**：
- 消耗对象并返回新值
- 转换为其他类型
- 资源清理
- builder模式的链式调用

```paw
fn into_inner(self) -> InnerType { }
fn consume(self) -> Result { }
fn build(self) -> FinalType { }
```

---

## self字段访问

### 基础访问

```paw
type Point = struct { x: i32, y: i32 };

support Point {
    fn sum(&self) -> i32 {
        return self.x + self.y;  // ✅ 字段访问
    }
    
    fn scale(&~self, factor: i32) -> void {
        self.x = self.x * factor;
        self.y = self.y * factor;
    }
}
```

### 嵌套字段访问

```paw
type Inner = struct { value: i32 };
type Outer = struct { inner: Inner };

support Outer {
    fn get_inner_value(&self) -> i32 {
        return self.inner.value;  // ✅ 嵌套访问
    }
}
```

---

## 与Rust对比

| 特性 | PawLang | Rust |
|-----|---------|------|
| 值传递 | `self` | `self` |
| 不可变引用 | `&self` | `&self` |
| 可变引用 | `&~self` | `&mut self` |
| 类型注解 | 可选 | 通常省略 |

### 语法差异

```rust
// Rust
impl Counter {
    fn get(&self) -> i32 { self.value }
    fn increment(&mut self) { self.value += 1; }
    fn consume(self) -> i32 { self.value }
}
```

```paw
// PawLang
support Counter with CounterOps {
    fn get(&self) -> i32 { return self.value; }
    fn increment(&~self) -> void { self.value = self.value + 1; }
    fn consume(self) -> i32 { return self.value; }
}
```

**主要区别**：
- PawLang使用`~`表示可变性（`&~self`）
- Rust使用`mut`关键字（`&mut self`）
- PawLang的`~`与变量声明一致（`let ~x`）

---

## 高级用法

### 链式调用（Builder模式）

```paw
type Builder = struct {
    value: i32,
    name: string
};

type BuilderOps = interface {
    fn with_value(self, v: i32) -> Self;
    fn with_name(self, n: string) -> Self;
    fn build(self) -> FinalType;
};

support Builder with BuilderOps {
    fn with_value(self, v: i32) -> Self {
        return Builder { value: v, name: self.name };
    }
    
    fn with_name(self, n: string) -> Self {
        return Builder { value: self.value, name: n };
    }
    
    fn build(self) -> FinalType {
        // ...
    }
}

// 使用
let result = Builder { value: 0, name: "" }
    .with_value(42)
    .with_name("test")
    .build();
```

### Self类型返回

```paw
type Cloneable = interface {
    fn clone(&self) -> Self;
};

support Point with Cloneable {
    fn clone(&self) -> Self {
        return Point { x: self.x, y: self.y };
    }
}
```

---

## 最佳实践

### 1. 优先使用`&self`

```paw
// ✅ 好
fn get_value(&self) -> i32 { return self.value; }

// ❌ 避免（除非需要消耗）
fn get_value(self) -> i32 { return self.value; }
```

### 2. 只在需要时使用`&~self`

```paw
// ✅ 好 - 需要修改
fn increment(&~self) { self.value = self.value + 1; }

// ❌ 避免 - 不需要修改
fn get(&~self) -> i32 { return self.value; }
```

### 3. 明确所有权转移

```paw
// ✅ 好 - 函数名表明消耗对象
fn consume(self) -> i32 { }
fn into_inner(self) -> Inner { }

// ⚠️ 注意 - 不明显的消耗
fn process(self) { }  // 考虑使用&self
```

### 4. 保持一致性

```paw
// ✅ 好 - 同类方法使用相同模式
type Counter = interface {
    fn get(&self) -> i32;
    fn set(&~self, v: i32);
    fn reset(&~self);
};

// ❌ 避免 - 不一致
type Counter = interface {
    fn get(&self) -> i32;      // 引用
    fn set(self, v: i32);      // 值传递（不一致）
};
```

---

## 常见错误

### 错误1：使用显式类型注解

```paw
// ❌ 错误 - 不支持显式注解
type Display = interface {
    fn show(self: &Self) -> void;  // 语法错误！
};

// ✅ 正确 - 使用简写
type Display = interface {
    fn show(&self) -> void;
};
```

```paw
// ❌ 错误 - 不支持显式注解
support Point with Display {
    fn show(self: &Self) -> void {  // 语法错误！
        println(self.x);
    }
}

// ✅ 正确 - 使用简写
support Point with Display {
    fn show(&self) -> void {
        println(self.x);
    }
}
```

### 错误2：忘记声明可变性

```paw
// ❌ 错误
let c = Counter { value: 0 };
c.increment();  // 错误：c不可变

// ✅ 正确
let ~c = Counter { value: 0 };
c.increment();
```

### 错误3：消耗后继续使用

```paw
// ❌ 错误
let c = Counter { value: 0 };
let v = c.consume();
println(c.value);  // 错误：c已被消耗

// ✅ 正确
let c = Counter { value: 0 };
println(c.get());
let v = c.consume();  // 最后消耗
```

### 错误4：不必要的值传递

```paw
// ❌ 避免 - 不必要的复制/移动
fn display(self) -> void {
    println(self.x);
}

// ✅ 好 - 使用引用
fn display(&self) -> void {
    println(self.x);
}
```

---

## 总结

### 核心概念

1. **三种形式**：`self`、`&self`、`&~self`
2. **只支持简写**：不能使用`self: &Self`等显式注解
3. **自动推断**：编译器自动推断为对应的Self类型
4. **字段访问**：使用`self.field`直接访问
5. **所有权语义**：遵循PawLang的所有权规则

### 选择指南

```
需要修改对象？
├─ 是 → 使用 &~self
└─ 否 → 需要消耗对象？
         ├─ 是 → 使用 self
         └─ 否 → 使用 &self
```

### 关键优势

- ✅ **简洁**：无需重复类型注解
- ✅ **直观**：语法清晰表达意图
- ✅ **安全**：编译时检查所有权
- ✅ **现代**：与Rust等语言保持一致

---

**PawLang self参数语法 - 简洁、安全、现代！** 🎉

---

*文档版本: v1.5.0*  
*最后更新: 2025-11-03*  
*状态: 稳定*

