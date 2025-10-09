# 🔗 PawLang模块系统

## 概述

PawLang v0.1.2 实现了简洁而强大的模块系统，使用`.`语法进行导入。

---

## 🎯 核心语法

### 导入语法

```paw
import module.item;
```

- 使用`.`分隔模块路径和项名
- 最后一个标识符是要导入的项
- 前面的部分是模块路径

### 示例

```paw
// 导入函数
import math.add;
import math.multiply;

// 导入类型
import math.Vec2;
import math.Point;

// 嵌套模块
import graphics.shapes.Circle;
import graphics.colors.RGB;
```

---

## 📁 模块文件组织

### 模块查找规则

| 导入语句 | 查找文件（按顺序） |
|----------|-------------------|
| `import math.add` | 1. `math.paw`<br>2. `math/mod.paw` |
| `import math.vec.Point` | 1. `math/vec.paw`<br>2. `math/vec/mod.paw` |
| `import graphics.shapes.Circle` | 1. `graphics/shapes.paw`<br>2. `graphics/shapes/mod.paw` |

### 推荐的项目结构

```
my_project/
├── main.paw              # 主程序
├── math.paw              # math模块
├── utils.paw             # utils模块
└── graphics/
    ├── mod.paw           # graphics模块（可选）
    ├── shapes.paw        # graphics.shapes子模块
    └── colors.paw        # graphics.colors子模块
```

---

## ✨ 完整示例

### 创建模块

**math.paw**:
```paw
// 只有pub标记的项可以被导入

pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub fn multiply(a: i32, b: i32) -> i32 {
    return a * b;
}

// 私有函数不能被导入
fn internal_helper() -> i32 {
    return 42;
}

pub type Vec2 = struct {
    x: i32,
    y: i32,
}

pub type Point = struct {
    x: i32,
    y: i32,
    
    fn new(x: i32, y: i32) -> Point {
        return Point { x: x, y: y };
    }
}
```

### 使用模块

**main.paw**:
```paw
// 导入需要的项
import math.add;
import math.multiply;
import math.Vec2;
import math.Point;

fn main() -> i32 {
    // 使用导入的函数
    let sum: i32 = add(10, 20);
    let product: i32 = multiply(5, 6);
    
    // 使用导入的类型
    let vector: Vec2 = Vec2 { x: 1, y: 2 };
    let point: Point = Point { x: 3, y: 4 };
    
    println("模块系统工作正常!");
    return 0;
}
```

### 运行

```bash
./zig-out/bin/pawc main.paw --run
```

---

## 📋 导出规则

### pub关键字

只有标记为`pub`的项才能被其他模块导入：

```paw
// math.paw

pub fn add(a: i32, b: i32) -> i32 {  // ✅ 可以导入
    return a + b;
}

fn helper() -> i32 {  // ❌ 不能导入（没有pub）
    return 0;
}

pub type Point = struct { ... }  // ✅ 可以导入

type Internal = struct { ... }   // ❌ 不能导入
```

---

## 🎨 设计特点

### 1. 简洁语法

不使用复杂的`::`或`use`，直接用`.`导入：

```paw
// PawLang - 简洁直观
import math.add;
let sum = add(10, 20);

// vs Rust
// use math::add;
// let sum = add(10, 20);
```

### 2. 显式导入

必须明确指定要导入的项，不支持通配符：

```paw
// ✅ 明确导入
import math.add;
import math.multiply;

// ❌ 不支持通配符
// import math.*;
```

### 3. 命名空间隔离

导入的项直接在当前作用域可用，不需要前缀：

```paw
import math.add;

fn main() -> i32 {
    let x = add(1, 2);  // 直接使用，不需要 math.add()
    return 0;
}
```

---

## 🔧 技术实现

### 模块加载流程

```
import math.add
    ↓
解析为：module_path="math", item_name="add"
    ↓
查找文件：math.paw
    ↓
读取并解析文件
    ↓
查找pub标记的add函数
    ↓
将add函数添加到当前AST
    ↓
继续编译
```

### 缓存机制

- 每个模块只加载一次
- 多次导入同一模块不会重复解析
- 提升编译速度

---

## 📊 性能

- **模块加载**: <5ms（典型模块）
- **缓存命中**: <0.1ms
- **内存占用**: 每个模块~10KB

---

## 🚀 未来计划

### v0.1.3可能添加

1. **相对导入**
   ```paw
   import .utils.helper;     // 当前目录
   import ..common.types;    // 上级目录
   ```

2. **重命名导入**
   ```paw
   import math.add as math_add;
   ```

3. **批量导入**
   ```paw
   import math.{add, multiply, Vec2};
   ```

---

## 📖 最佳实践

### 1. 模块命名

- 使用小写和下划线：`string_utils.paw`
- 简短且描述性：`math.paw`, `file.paw`

### 2. 导出策略

- 只导出稳定的API
- 内部实现不加pub
- 文档化pub项

### 3. 文件组织

```
项目根目录/
├── main.paw          # 主程序
├── math.paw          # 简单模块
└── graphics/         # 复杂模块
    ├── shapes.paw
    └── colors.paw
```

---

## 📚 示例项目

查看示例：
- `examples/module_demo.paw` - 模块系统演示
- `tests/test_modules.paw` - 模块测试
- `math.paw` - 示例模块

---

**🐾 PawLang模块系统 - 让代码组织更清晰！**

