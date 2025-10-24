# PawLang 模块系统配置指南

## 📚 模块系统概述

PawLang使用**基于路径的模块系统**，类似于Rust，支持：
- 标准库模块 (stdlib/)
- 用户模块 (相对路径)
- 跨文件编译
- 自动依赖解析

## 🗂️ 目录结构

```
paw/
├── stdlib/                    # 标准库目录
│   └── std/                   # std命名空间
│       ├── array.paw          # std::array
│       ├── string.paw         # std::string
│       ├── math.paw           # std::math
│       ├── io.paw             # std::io
│       ├── fs.paw             # std::fs
│       ├── collections.paw    # std::collections
│       ├── conv.paw           # std::conv
│       ├── fmt.paw            # std::fmt
│       ├── mem.paw            # std::mem
│       ├── os.paw             # std::os
│       ├── parse.paw          # std::parse
│       ├── path.paw           # std::path
│       ├── result.paw         # std::result
│       ├── time.paw           # std::time
│       └── vec.paw            # std::vec
├── examples/
│   ├── modules/               # 模块示例
│   │   ├── main.paw          # 主程序
│   │   ├── math.paw          # 数学模块
│   │   ├── types.paw         # 类型模块
│   │   └── ...
│   └── *.paw                 # 单文件示例
└── build/
    └── stdlib/               # 构建时复制的标准库
        └── std/
```

## 🔧 模块路径解析规则

### 规则1: 标准库优先

```paw
import "std::math";
```

**解析过程**:
1. 将 `::` 替换为 `/`: `std/math`
2. 添加扩展名: `std/math.paw`
3. 先查找标准库: `stdlib/std/math.paw` ✅
4. 找到则使用

### 规则2: 相对路径

```paw
import "math";
```

**解析过程**:
1. 处理路径: `math.paw`
2. 如果标准库不存在: 使用相对路径
3. 查找: `base_path/math.paw`（base_path是主文件所在目录）

### 规则3: 嵌套模块

```paw
import "utils::helper";
```

**解析过程**:
1. `utils::helper` → `utils/helper.paw`
2. 先查找: `stdlib/utils/helper.paw`
3. 不存在则: `base_path/utils/helper.paw`

## 📝 配置文件: paw.toml

当前配置:
```toml
[package]
name = "pawc"
version = "0.2.1"

[build]
target = "native"
opt_level = "2"
```

**注意**: 目前paw.toml主要用于元数据，模块路径由代码硬编码。

## 💡 使用示例

### 示例1: 使用标准库

```paw
// main.paw
import "std::math";
import "std::string";

fn main() -> i32 {
    let x: i32 = math::abs(-42);
    let s: string = "Hello";
    let len: i64 = string::len(s);
    return 0;
}
```

**编译**:
```bash
./build/pawc main.paw -o app
```

**模块查找**:
- `std::math` → `stdlib/std/math.paw` ✅
- `std::string` → `stdlib/std/string.paw` ✅

### 示例2: 用户模块

```paw
// math.paw
pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

```paw
// main.paw
import "math";

fn main() -> i32 {
    return math::add(10, 20);
}
```

**目录结构**:
```
my_project/
├── main.paw
└── math.paw
```

**编译**:
```bash
cd my_project
../build/pawc main.paw -o app
```

**模块查找**:
- `math` → 先查找 `stdlib/math.paw` (不存在)
- 然后查找 `my_project/math.paw` ✅

### 示例3: 嵌套模块

```paw
// utils/string.paw
pub fn reverse(s: string) -> string {
    // ...
}
```

```paw
// main.paw
import "utils::string";

fn main() -> i32 {
    // ...
}
```

**目录结构**:
```
my_project/
├── main.paw
└── utils/
    └── string.paw
```

**模块查找**:
- `utils::string` → `utils/string.paw` ✅

## 🔍 模块加载机制

### 核心代码: `src/module/module_loader.cpp`

**关键函数**:

```cpp
std::string ModuleLoader::resolveModulePath(const std::string& import_path) {
    // "std::math" → "stdlib/std/math.paw"
    
    std::string path = import_path;
    
    // 1. 替换 :: 为 /
    path.replace("::", "/");
    
    // 2. 添加.paw扩展名
    path += ".paw";
    
    // 3. 先尝试标准库路径
    std::string stdlib_path = "stdlib/" + path;
    if (file_exists(stdlib_path)) {
        return stdlib_path;  // ✅ 标准库优先
    }
    
    // 4. 使用base_path（主文件目录）
    if (!base_path_.empty()) {
        path = base_path_ + "/" + path;
    }
    
    return path;
}
```

## 🎯 当前支持的标准库模块

| 模块名 | import语句 | 文件路径 |
|--------|-----------|---------|
| array | `import "std::array";` | `stdlib/std/array.paw` |
| collections | `import "std::collections";` | `stdlib/std/collections.paw` |
| conv | `import "std::conv";` | `stdlib/std/conv.paw` |
| fmt | `import "std::fmt";` | `stdlib/std/fmt.paw` |
| fs | `import "std::fs";` | `stdlib/std/fs.paw` |
| io | `import "std::io";` | `stdlib/std/io.paw` |
| math | `import "std::math";` | `stdlib/std/math.paw` |
| mem | `import "std::mem";` | `stdlib/std/mem.paw` |
| os | `import "std::os";` | `stdlib/std/os.paw` |
| parse | `import "std::parse";` | `stdlib/std/parse.paw` |
| path | `import "std::path";` | `stdlib/std/path.paw` |
| result | `import "std::result";` | `stdlib/std/result.paw` |
| string | `import "std::string";` | `stdlib/std/string.paw` |
| time | `import "std::time";` | `stdlib/std/time.paw` |
| vec | `import "std::vec";` | `stdlib/std/vec.paw` |

## 🚨 常见错误和解决方案

### 错误1: 找不到模块

```
Cannot open file: examples/math.paw
Failed to load dependency: math
```

**原因**: 模块路径不正确

**解决方案**:

**选项1**: 使用标准库
```paw
// 错误
import "math";

// 正确
import "std::math";
```

**选项2**: 创建对应文件
```bash
# 如果使用 import "math"
# 需要在examples/目录创建math.paw
touch examples/math.paw
```

**选项3**: 调整import路径
```paw
// 如果文件在 examples/modules/math.paw
import "modules::math";  // ✅
```

### 错误2: 标准库路径问题

```
Cannot open file: examples/std/io.paw
```

**原因**: 相对路径从examples/目录查找，而不是项目根目录

**解决方案**:
```paw
// 错误（会从examples/std/查找）
import "std::io";  // 从examples/目录编译时

// 正确
import "std::io";  // 从项目根目录编译时
```

**编译方式**:
```bash
# 在项目根目录编译 ✅
cd /path/to/paw
./build/pawc examples/io_advanced.paw -o app

# 而不是在examples/目录编译 ❌
cd examples
../build/pawc io_advanced.paw -o app
```

## 📖 模块开发最佳实践

### 1. 标准库模块开发

**位置**: `stdlib/std/yourmodule.paw`

**模板**:
```paw
// stdlib/std/yourmodule.paw

// 声明需要的C库函数
extern "C" fn some_c_func(x: i32) -> i32;

// 导出公共函数
pub fn your_function(x: i32) -> i32 {
    return some_c_func(x);
}

// 私有函数（不导出）
fn internal_helper() -> i32 {
    return 42;
}
```

**使用**:
```paw
import "std::yourmodule";

fn main() -> i32 {
    return yourmodule::your_function(10);
}
```

### 2. 用户模块开发

**目录结构**:
```
my_project/
├── main.paw
├── utils/
│   ├── math.paw
│   └── string.paw
└── data/
    └── types.paw
```

**模块示例**:
```paw
// utils/math.paw
pub fn square(x: i32) -> i32 {
    return x * x;
}
```

```paw
// main.paw
import "utils::math";

fn main() -> i32 {
    return math::square(5);  // 25
}
```

### 3. 公共/私有控制

```paw
// mymodule.paw

// 公共函数 - 其他模块可以访问
pub fn public_func() -> i32 {
    return 42;
}

// 私有函数 - 只能在本模块内使用
fn private_func() -> i32 {
    return 100;
}

// 公共类型
pub type MyStruct = struct {
    value: i32,
}
```

## 🛠️ 调试模块问题

### 查看模块加载过程

```bash
# 编译时会显示加载的模块
./build/pawc main.paw -o app
```

输出:
```
Loading modules...
Loaded 2 module(s)
Compiling module: std::math
Compiling module: __main__
```

### 检查模块文件是否存在

```bash
# 检查标准库模块
ls stdlib/std/

# 检查用户模块
ls your_modules/
```

## 📋 实际工作示例

### examples/modules/ 目录

查看实际工作的模块示例:
```bash
ls examples/modules/
```

文件:
- `main.paw` - 主程序，导入其他模块
- `math.paw` - 数学函数模块
- `types.paw` - 类型定义模块
- `data.paw` - 数据模块

**main.paw示例**:
```paw
import "math";

fn main() -> i32 {
    let x: i32 = math::add(10, 20);
    return x;
}
```

**编译**:
```bash
cd /Users/haojunhuang/CLionProjects/paw
./build/pawc examples/modules/main.paw -o app
./app
```

## 🔑 关键概念

### 1. base_path (基础路径)
- 主文件所在的目录
- 用于解析相对import

### 2. 模块查找顺序
1. **标准库**: `stdlib/` + 路径
2. **相对路径**: `base_path/` + 路径

### 3. 命名空间
- 使用 `::` 分隔: `std::math`
- 内部转换为 `/`: `std/math.paw`

### 4. 可见性
- `pub fn` - 公共函数，可跨模块调用
- `fn` - 私有函数，仅模块内部使用
- `pub type` - 公共类型

## 🐛 修复失败的测试

### import_simple.paw 问题

**当前代码**:
```paw
import "math";
import "utils::helper";
```

**问题**: `examples/math.paw` 和 `examples/utils/helper.paw` 不存在

**解决方案1**: 创建缺失的文件
```bash
# 创建math.paw
cat > examples/math.paw << 'EOF'
pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
EOF

# 创建utils/helper.paw
mkdir -p examples/utils
cat > examples/utils/helper.paw << 'EOF'
pub fn helper_func() -> i32 {
    return 100;
}
EOF
```

**解决方案2**: 使用标准库
```paw
import "std::math";  // ✅ 使用标准库
```

**解决方案3**: 使用modules/子目录
```paw
import "modules::math";  // ✅ 使用examples/modules/math.paw
```

### io_advanced.paw 问题

**当前代码**:
```paw
import "std::io";
```

**问题**: 从`examples/`目录查找，路径变成`examples/std/io.paw`

**解决方案**: 从项目根目录编译
```bash
# 正确 ✅
cd /Users/haojunhuang/CLionProjects/paw
./build/pawc examples/io_advanced.paw -o app

# 错误 ❌
cd examples
../build/pawc io_advanced.paw -o app
```

## 📊 模块系统当前状态

### ✅ 已实现的功能
- ✅ 标准库模块加载
- ✅ 相对路径模块
- ✅ 跨模块函数调用
- ✅ 跨模块类型使用
- ✅ pub可见性控制
- ✅ 自动依赖解析
- ✅ 循环依赖检测
- ✅ 拓扑排序编译
- ✅ 跨模块泛型调用

### 🔄 工作流程

```
main.paw (import "std::math")
   │
   ├─> 解析import: std::math
   ├─> 查找文件: stdlib/std/math.paw
   ├─> 加载模块: std::math
   ├─> 递归加载依赖
   ├─> 拓扑排序
   ├─> 按顺序编译各模块
   ├─> 链接所有.o文件
   └─> 生成可执行文件
```

## 🎓 进阶技巧

### 技巧1: 模块别名（未来功能）
```paw
// 当前不支持，未来可能支持
import "std::collections" as col;
```

### 技巧2: 选择性导入（未来功能）
```paw
// 当前不支持
import "std::math" { abs, max, min };
```

### 技巧3: 重新导出（未来功能）
```paw
// 当前不支持
pub use "std::math";
```

## 🔧 故障排除

### 问题: "Cannot open file"

**检查清单**:
1. ✅ 文件是否存在？
   ```bash
   ls stdlib/std/math.paw
   ```

2. ✅ 路径是否正确？
   ```paw
   import "std::math";  # 标准库
   import "mymodule";   # 相对路径
   ```

3. ✅ 编译目录是否正确？
   ```bash
   cd /Users/haojunhuang/CLionProjects/paw  # 项目根目录
   ./build/pawc examples/main.paw -o app
   ```

4. ✅ 模块名称是否匹配？
   ```paw
   # 如果文件是 stdlib/std/math.paw
   import "std::math";  # ✅
   import "math";       # ❌
   ```

### 问题: "Cyclic dependency"

**原因**: 模块A导入B，B又导入A

**解决**: 重构模块结构，避免循环依赖

## 📚 参考

### 成功的模块示例

查看 `examples/modules/` 目录中的工作示例：
```bash
cd /Users/haojunhuang/CLionProjects/paw
cat examples/modules/main.paw
cat examples/modules/math.paw
```

### 标准库参考

查看标准库实现：
```bash
ls stdlib/std/
cat stdlib/std/math.paw
cat stdlib/std/string.paw
```

---

## 🎯 快速参考

### 导入标准库
```paw
import "std::math";      // 数学函数
import "std::string";    // 字符串操作
import "std::io";        // 输入输出
import "std::fs";        // 文件系统
import "std::collections"; // 集合类型
```

### 导入用户模块
```paw
import "mymodule";       // 同目录
import "utils::helper";  // 子目录
```

### 调用函数
```paw
import "std::math";

fn main() -> i32 {
    return math::abs(-42);  // 命名空间::函数
}
```

---

**总结**: PawLang的模块系统简洁强大，标准库优先，支持嵌套模块，自动依赖解析！🚀


