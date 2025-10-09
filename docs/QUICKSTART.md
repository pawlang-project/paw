# 🐾 PawLang 快速开始

## 5分钟上手PawLang v0.1.2

---

## 📦 安装

```bash
git clone https://github.com/yourusername/PawLang.git
cd PawLang
zig build
```

---

## 🎯 第一个程序

创建 `hello.paw`：

```paw
fn main() -> i32 {
    println("Hello, PawLang! 🐾");
    return 0;
}
```

运行：

```bash
./zig-out/bin/pawc hello.paw --run
```

---

## 🌟 核心特性演示

### 1. 泛型静态方法

```paw
fn main() -> i32 {
    // 使用 :: 调用静态方法
    let vec: Vec<i32> = Vec<i32>::new();
    let vec2: Vec<i32> = Vec<i32>::with_capacity(10);
    
    println("Vec创建成功!");
    return 0;
}
```

### 2. 泛型实例方法

```paw
fn main() -> i32 {
    let vec: Vec<i32> = Vec<i32>::with_capacity(5);
    
    // 使用 . 调用实例方法
    let len: i32 = vec.length();
    let cap: i32 = vec.capacity_method();
    
    println("长度: $len, 容量: $cap");
    return 0;
}
```

### 3. 自定义泛型类型

```paw
type Box<T> = struct {
    value: T,
    
    // self不需要类型！
    fn get(self) -> T {
        return self.value;
    }
}

fn main() -> i32 {
    let box: Box<i32> = Box<i32>::new(42);
    let val: i32 = box.get();
    
    println("值: $val");
    return 0;
}
```

---

## 📚 更多示例

运行示例程序：

```bash
# 泛型方法完整演示
./zig-out/bin/pawc examples/generic_methods.paw --run

# 泛型演示
./zig-out/bin/pawc examples/generics_demo.paw --run

# 字符串插值
./zig-out/bin/pawc examples/string_interpolation.paw --run

# 错误处理
./zig-out/bin/pawc examples/error_propagation.paw --run
```

---

## 🎓 下一步

1. 📖 阅读 [README.md](README.md) 了解完整特性
2. 🔍 查看 [examples/](examples/) 目录学习更多示例
3. 📝 阅读 [RELEASE_NOTES_v0.1.2.md](RELEASE_NOTES_v0.1.2.md) 了解最新特性
4. 💻 开始编写自己的PawLang程序！

---

**🐾 Happy Coding!**

