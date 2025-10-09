# Paw v0.1.1 发布说明

## 🎉 重大更新：完整泛型系统

Paw v0.1.1 实现了完整的泛型系统，包括泛型函数和泛型结构体，使 Paw 成为少数几个同时支持这两大特性的系统编程语言之一。

## ✨ 新功能

### 1. 泛型函数 (Generic Functions)

```paw
fn identity<T>(x: T) -> T {
    return x;
}

fn pair<A, B>(first: A, second: B) -> i32 {
    println("Pair created");
    return 0;
}

fn main() -> i32 {
    let a = identity(42);      // 自动推导为 identity_i32
    let b = identity(3.14);    // 自动推导为 identity_f64
    let c = pair(42, 3.14);    // 自动推导为 pair_i32_f64
    return a;
}
```

**特性：**
- ✅ 单/多类型参数支持
- ✅ 自动类型推导（从实参推导类型）
- ✅ 单态化代码生成
- ✅ 名称修饰（`identity<i32>` → `identity_i32`）
- ✅ 去重优化
- ✅ 零运行时开销

### 2. 泛型结构体 (Generic Structs)

```paw
type Box<T> = struct {
    value: T,
}

type Pair<A, B> = struct {
    first: A,
    second: B,
}

fn main() -> i32 {
    // 单类型参数
    let box1 = Box { value: 42 };     // Box_i32
    let box2 = Box { value: 3.14 };   // Box_f64
    let val1 = box1.value;
    
    // 多类型参数
    let pair = Pair { first: 100, second: 2.71 };  // Pair_i32_f64
    let a = pair.first;
    let b = pair.second;
    
    return val1;
}
```

**特性：**
- ✅ 单/多类型参数支持
- ✅ 自动类型推导（从字段值推导类型）
- ✅ 单态化结构体生成
- ✅ 字段访问
- ✅ 多实例支持（同一泛型结构体的不同类型实例）
- ✅ 零运行时开销

### 3. 类型推导引擎

编译器自动从表达式推导类型：
- `42` → `i32`
- `3.14` → `f64`
- `"hello"` → `string`
- `true` → `bool`
- `'c'` → `char`

## 🔧 技术实现

### 编译器架构
```
Lexer → Parser → TypeChecker → GenericEngine → CodeGen → C → TinyCC/GCC → Executable
                                     ↓
                            Monomorphization
                            Type Inference
                            Name Mangling
```

### 核心模块
1. **`src/generics.zig`** - 泛型引擎
   - 类型推导
   - 单态化
   - 名称修饰
   
2. **`src/codegen.zig`** - 代码生成
   - 泛型函数单态化
   - 泛型结构体单态化
   - 类型参数替换

3. **`src/typechecker.zig`** - 类型检查
   - 泛型类型参数作用域
   - 类型兼容性检查

## 📊 性能

- **编译速度：** 毫秒级（小型项目）
- **运行时开销：** 零（完全单态化）
- **代码大小：** 与手写 C 代码相同

## 🧪 测试

### 测试文件
- `tests/test_generic_function.paw` - 泛型函数测试
- `tests/test_box_simple.paw` - 单类型参数结构体
- `tests/test_box_access.paw` - 字段访问
- `tests/test_pair.paw` - 多类型参数结构体
- `tests/test_generic_struct_complete.paw` - 完整测试

### 测试结果
```
✅ 所有测试通过
✅ 生成的 C 代码正确
✅ 编译成功
✅ 运行正确
```

## 📝 限制和已知问题

### 当前不支持的功能
1. **泛型方法** - 将在 v0.2.1 实现
   ```paw
   fn Box<T>::new(val: T) -> Box<T> { ... }  // 暂不支持
   ```

2. **泛型约束** - 将在 v0.3.0 实现
   ```paw
   fn print<T: Display>(val: T) { ... }  // 暂不支持
   ```

3. **高阶泛型** - 将在 v0.3.0+ 实现
   ```paw
   type Container<T<U>> = ...  // 暂不支持
   ```

### 变通方案
- 泛型方法：使用普通函数 + 第一个参数为结构体
- 泛型约束：当前无约束，接受所有类型

## 🔄 从 v0.1.0 升级

### 新增语法
```paw
// 泛型函数
fn func<T>(x: T) -> T { ... }

// 泛型结构体
type Box<T> = struct { value: T }
```

### 向后兼容
- ✅ 所有 v0.1.0 代码继续有效
- ✅ 无破坏性更改
- ✅ 可选使用泛型

## 🚀 开始使用

### 安装
```bash
git clone https://github.com/yourusername/PawLang
cd PawLang
zig build
```

### 快速开始
```paw
// hello_generic.paw
fn greet<T>(val: T) -> i32 {
    println("Hello, Paw Generics!");
    return 0;
}

fn main() -> i32 {
    return greet(42);
}
```

编译运行：
```bash
./zig-out/bin/pawc hello_generic.paw --run
```

## 🙏 致谢

感谢所有贡献者和测试者！

## 📅 发布日期

2025年10月9日

## 📄 许可证

MIT License

---

**Paw - 简单、安全、高性能的系统编程语言 🐾**
