# PawLang self语法规则

**版本**: v1.5.0  
**重要**: self参数**只支持简写**，不支持显式类型注解

---

## ✅ 支持的语法

### 三种简写形式

```paw
// 1. 值传递
fn consume(self) -> i32 { }

// 2. 不可变引用
fn get(&self) -> i32 { }

// 3. 可变引用
fn mutate(&~self) -> void { }
```

---

## ❌ 不支持的语法

### 显式类型注解

以下语法**全部不支持**：

```paw
// ❌ 错误 - 显式类型注解
fn method1(self: Self) { }
fn method2(self: &Self) { }
fn method3(self: &~Self) { }

// ❌ 错误 - 任何self的类型注解
fn method4(self: Point) { }
fn method5(self: &Point) { }
```

**原因**：PawLang设计为只支持self简写语法，保持简洁统一。

---

## 正确 vs 错误对比

### 接口定义

```paw
// ❌ 错误
type Display = interface {
    fn show(self: &Self) -> void;
};

// ✅ 正确
type Display = interface {
    fn show(&self) -> void;
};
```

### 接口实现

```paw
// ❌ 错误
support Point with Display {
    fn show(self: &Self) -> void {
        println(self.x);
    }
}

// ✅ 正确
support Point with Display {
    fn show(&self) -> void {
        println(self.x);
    }
}
```

### 普通方法

```paw
// ❌ 错误
support Counter {
    fn increment(self: &~Self) -> void {
        self.value = self.value + 1;
    }
}

// ✅ 正确
support Counter {
    fn increment(&~self) -> void {
        self.value = self.value + 1;
    }
}
```

---

## 与其他参数对比

### 普通参数 vs self参数

```paw
// 普通参数：必须有类型注解
fn add(x: i32, y: i32) -> i32 {  // ✅ 需要类型注解
    return x + y;
}

// self参数：只能用简写，不能注解
fn method(&self) -> i32 {  // ✅ 简写形式
    return self.value;
}

// self参数：不能有类型注解
fn method(self: &Self) -> i32 {  // ❌ 错误！
    return self.value;
}
```

---

## 语法设计理由

### 为什么不支持显式注解？

1. **简洁性**：`&self`比`self: &Self`更简洁
2. **一致性**：统一使用简写，避免混用
3. **清晰性**：三种形式一目了然
4. **现代性**：与Rust等语言保持一致

### 与Rust的对比

| 特性 | PawLang | Rust |
|-----|---------|------|
| 简写支持 | ✅ `self`, `&self`, `&~self` | ✅ `self`, `&self`, `&mut self` |
| 显式注解 | ❌ 不支持 | ✅ 支持（但不常用） |

**Rust示例**：
```rust
// Rust允许但不推荐
fn method(self: &Self) { }

// Rust推荐简写
fn method(&self) { }
```

**PawLang决策**：只支持简写，强制统一风格。

---

## 编译器错误示例

如果使用显式注解，编译器会报错：

```paw
fn show(self: &Self) -> void {
    println(self.x);
}

// 编译错误：
// Expected ')' or ',' after parameter
// self参数只支持简写形式：self, &self, &~self
```

---

## 迁移指南

### 从显式注解迁移

如果你的代码使用了显式注解（不应该存在），需要改为简写：

```paw
// 旧代码（错误）
fn method(self: Self) { }        // ❌
fn method(self: &Self) { }       // ❌
fn method(self: &~Self) { }      // ❌

// 新代码（正确）
fn method(self) { }              // ✅
fn method(&self) { }             // ✅
fn method(&~self) { }            // ✅
```

### 查找和替换

1. 查找：`self: Self` → 替换为：`self`
2. 查找：`self: &Self` → 替换为：`&self`
3. 查找：`self: &~Self` → 替换为：`&~self`

---

## 总结

### 核心规则

1. ✅ **只支持**三种简写：`self`、`&self`、`&~self`
2. ❌ **不支持**任何形式的显式类型注解
3. ✅ 编译器自动推断为对应的Self类型
4. ✅ self是唯一不需要类型注解的参数

### 记忆方法

```
self参数 = 特殊参数
- 不同于普通参数（普通参数需要类型注解）
- 只能使用三种固定简写
- 类型由编译器自动推断
```

---

**遵循规则，保持代码简洁统一！** ✨

---

*文档版本: v1.5.0*  
*最后更新: 2025-11-03*  
*状态: 规范*


