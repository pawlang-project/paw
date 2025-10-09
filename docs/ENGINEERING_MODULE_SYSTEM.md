# 工程化模块系统设计方案

**目标**: 支持大型项目的模块组织和管理  
**版本**: v0.2.0  
**设计时间**: 2025-10-09

---

## 🎯 设计目标

### 1. 项目结构支持
```
my_project/
├── paw.toml           # 项目配置
├── src/
│   ├── main.paw       # 入口文件
│   ├── lib.paw        # 库入口（可选）
│   ├── utils/
│   │   ├── mod.paw    # 模块入口
│   │   ├── math.paw
│   │   └── string.paw
│   └── models/
│       ├── mod.paw
│       ├── user.paw
│       └── post.paw
├── tests/
│   └── test_utils.paw
└── examples/
    └── demo.paw
```

### 2. 清晰的导入语法
```paw
// 导入单个项
import utils.math.add;
import models.user.User;

// 导入多个项
import utils.math.{add, multiply, PI};

// 导入整个模块
import utils.math.*;

// 模块别名
import utils.math as m;
use m.add;

// 重新导出
pub import utils.math.add;  // 公开导出
```

### 3. 模块路径解析
```paw
// 绝对路径（从src/开始）
import utils.math.add;     → src/utils/math.paw
import models.user.User;   → src/models/user.paw

// 相对路径
import .sibling;           → 同目录
import ..parent;           → 父目录
import ..utils.math;       → 父目录的utils

// 标准库
import std.vec.Vec;        → stdlib/vec.paw
import std.io.println;     → stdlib/io.paw
```

---

## 📋 实现方案

### Phase 1: 项目配置支持

#### paw.toml
```toml
[package]
name = "my_project"
version = "0.1.0"
authors = ["Your Name"]

[dependencies]
# 未来支持外部依赖
# some_lib = "1.0.0"

[features]
# 编译特性开关
default = []
experimental = []
```

#### 读取配置
```zig
pub const ProjectConfig = struct {
    name: []const u8,
    version: []const u8,
    src_dir: []const u8,      // 默认 "src"
    lib_dir: ?[]const u8,     // 默认 null
    test_dir: []const u8,     // 默认 "tests"
    
    pub fn load(allocator: Allocator, path: []const u8) !ProjectConfig {
        // 解析 paw.toml
        // ...
    }
};
```

---

### Phase 2: 模块路径解析增强

#### 当前实现（v0.1.3）
```paw
import math.add;  → 查找 math.paw
```

#### 工程化实现（v0.2.0）
```paw
import utils.math.add;

解析顺序:
1. src/utils/math.paw (文件)
2. src/utils/math/mod.paw (模块目录)
3. lib/utils/math.paw (库目录)
4. stdlib/utils/math.paw (标准库)
```

#### 实现代码
```zig
fn findModuleFile(
    self: *ModuleLoader,
    module_path: []const u8,
    project_config: ProjectConfig
) ![]const u8 {
    // 1. 尝试 src/path/to/module.paw
    const direct_file = try std.fmt.allocPrint(
        self.allocator,
        "{s}/{s}.paw",
        .{project_config.src_dir, module_path}
    );
    if (fileExists(direct_file)) return direct_file;
    
    // 2. 尝试 src/path/to/module/mod.paw
    const mod_file = try std.fmt.allocPrint(
        self.allocator,
        "{s}/{s}/mod.paw",
        .{project_config.src_dir, module_path}
    );
    if (fileExists(mod_file)) return mod_file;
    
    // 3. 尝试标准库
    const stdlib_file = try std.fmt.allocPrint(
        self.allocator,
        "stdlib/{s}.paw",
        .{module_path}
    );
    if (fileExists(stdlib_file)) return stdlib_file;
    
    return error.ModuleNotFound;
}
```

---

### Phase 3: 多项导入支持

#### 语法设计
```paw
// 当前 (v0.1.3): 每项一个import
import math.add;
import math.multiply;
import math.PI;

// 工程化 (v0.2.0): 多项导入
import math.{add, multiply, PI};

// 通配符导入
import math.*;
```

#### AST修改
```zig
pub const ImportDecl = struct {
    module_path: []const u8,
    items: ImportItems,
    
    pub const ImportItems = union(enum) {
        single: []const u8,           // import math.add
        multiple: [][]const u8,       // import math.{add, sub}
        wildcard,                     // import math.*
        aliased: struct {             // import math as m
            items: [][]const u8,
            alias: []const u8,
        },
    };
};
```

---

### Phase 4: 模块命名空间

#### 设计
```paw
// 方案1: 显式使用模块前缀
import math;  // 导入整个模块

let sum = math.add(1, 2);
let v = math.Vec2 { x: 1, y: 2 };

// 方案2: 别名
import utils.math as m;

let sum = m.add(1, 2);

// 方案3: 选择性导入
import math.{add, Vec2};

let sum = add(1, 2);      // 直接使用
let v = Vec2 { x: 1, y: 2 };
```

---

### Phase 5: 重新导出

#### 用途
```paw
// utils/mod.paw - 模块入口
pub import .math.{add, multiply};
pub import .string.{concat, split};
pub import .io.println;

pub type Result<T> = ...;

// main.paw
import utils.{add, println, Result};  // 从utils统一导入
```

#### 实现
```zig
pub const ImportDecl = struct {
    module_path: []const u8,
    items: ImportItems,
    is_pub: bool,  // 🆕 是否重新导出
};
```

---

### Phase 6: 标准库模块化

#### 标准库结构
```
stdlib/
├── mod.paw           # 标准库入口
├── prelude.paw       # 自动导入
├── collections/
│   ├── mod.paw
│   ├── vec.paw       # Vec<T>
│   ├── hashmap.paw   # HashMap<K,V>
│   └── list.paw      # LinkedList<T>
├── io/
│   ├── mod.paw
│   ├── print.paw     # println, print
│   ├── file.paw      # File operations
│   └── stream.paw    # Stream I/O
├── string/
│   ├── mod.paw
│   └── string.paw    # String type
├── math/
│   ├── mod.paw
│   ├── basic.paw     # abs, min, max
│   └── trig.paw      # sin, cos, tan
└── result/
    └── result.paw    # Result<T, E>, Option<T>
```

#### 使用方式
```paw
// 自动导入prelude（Vec, Box, println等）
// 无需手动import

// 导入其他标准库
import std.collections.HashMap;
import std.io.File;
import std.string.String;

fn main() -> i32 {
    // Vec来自prelude，自动可用
    let vec = Vec<i32>::new();
    
    // HashMap需要导入
    let map = HashMap<string, i32>::new();
    
    // println来自prelude，自动可用
    println("Hello!");
    
    return 0;
}
```

---

## 🏗️ 项目组织最佳实践

### 小型项目
```
simple_project/
├── main.paw           # 单文件项目
└── utils.paw          # 工具模块
```

### 中型项目
```
medium_project/
├── paw.toml
├── src/
│   ├── main.paw
│   ├── lib.paw        # 库接口
│   ├── core/
│   │   └── ...
│   └── utils/
│       └── ...
└── tests/
    └── ...
```

### 大型项目
```
large_project/
├── paw.toml
├── src/
│   ├── main.paw
│   ├── lib.paw
│   ├── core/
│   │   ├── mod.paw
│   │   ├── engine.paw
│   │   └── renderer.paw
│   ├── models/
│   │   ├── mod.paw
│   │   ├── user.paw
│   │   └── post.paw
│   ├── services/
│   │   ├── mod.paw
│   │   ├── auth.paw
│   │   └── api.paw
│   └── utils/
│       ├── mod.paw
│       ├── math.paw
│       └── string.paw
├── tests/
│   ├── core_test.paw
│   └── models_test.paw
└── examples/
    └── demo.paw
```

---

## 🔧 实现步骤

### Step 1: 项目配置支持 (2小时)
- [ ] 创建 paw.toml 解析器
- [ ] 定义 ProjectConfig 结构
- [ ] 在main.zig中加载配置

### Step 2: 增强路径解析 (3小时)
- [ ] 支持多级模块路径（a.b.c）
- [ ] 支持mod.paw模块入口
- [ ] 实现查找优先级

### Step 3: 多项导入语法 (4小时)
- [ ] 扩展Parser支持 {a, b, c}
- [ ] 扩展ImportDecl AST
- [ ] 更新模块加载逻辑

### Step 4: 命名空间支持 (3小时)
- [ ] 实现模块别名
- [ ] 支持完整路径访问
- [ ] 避免命名冲突

### Step 5: 重新导出 (2小时)
- [ ] 支持 pub import
- [ ] 传递导出逻辑
- [ ] 模块接口设计

### Step 6: 标准库重组 (4小时)
- [ ] 创建stdlib目录结构
- [ ] 分离Vec, Box到collections
- [ ] 创建io, string模块
- [ ] 保持prelude自动导入

---

## 🎨 语法示例

### 基础导入
```paw
// 当前方式（继续支持）
import math.add;
import math.Vec2;

// 新方式：多项导入
import math.{add, multiply, Vec2};

// 新方式：通配符
import math.*;
```

### 模块别名
```paw
import utils.math as m;
import std.collections.hashmap as hm;

let sum = m.add(1, 2);
let map = hm.HashMap<string, i32>::new();
```

### 嵌套模块
```paw
// src/models/user.paw
pub type User = struct {
    id: i32,
    name: string,
}

// src/models/mod.paw
pub import .user.User;
pub import .post.Post;

// main.paw
import models.{User, Post};  // 从mod.paw导出
```

### 相对导入
```paw
// src/services/auth.paw
import ..models.User;      // 导入上级目录的models
import .api.ApiClient;     // 导入同级目录的api

pub fn authenticate(u: User) -> bool {
    // ...
}
```

---

## 📝 配置文件格式

### paw.toml
```toml
[package]
name = "my_awesome_project"
version = "0.1.0"
authors = ["Your Name <email@example.com>"]
edition = "2025"

[project]
src_dir = "src"           # 源代码目录
lib_entry = "lib.paw"     # 库入口（可选）
main_entry = "main.paw"   # 主程序入口

[dependencies]
# 外部依赖（未来支持）
# http_client = { version = "1.0" }
# json_parser = { git = "https://..." }

[dev-dependencies]
# 开发依赖
# test_framework = "0.1"

[features]
default = []
experimental = ["async", "macros"]

[build]
optimize = "ReleaseFast"  # Debug, ReleaseSafe, ReleaseFast, ReleaseSmall
target = "native"
```

---

## 🔍 模块发现算法

### 查找优先级
```
import a.b.c;

查找顺序:
1. ./src/a/b/c.paw          (直接文件)
2. ./src/a/b/c/mod.paw      (模块目录)
3. ./lib/a/b/c.paw          (库文件)
4. ./stdlib/a/b/c.paw       (标准库)
5. ~/.paw/packages/...      (全局包，未来)
```

### 相对导入解析
```
当前文件: src/services/auth.paw

import .api;        → src/services/api.paw
import ..models;    → src/models/mod.paw
import ...utils;    → src/utils/mod.paw
```

---

## 🎯 标准库组织

### stdlib/prelude.paw (自动导入)
```paw
// 核心类型
pub type Vec<T> = ...;
pub type Box<T> = ...;
pub type Option<T> = ...;
pub type Result<T, E> = ...;

// 核心函数
pub fn println(msg: string) -> i32 { ... }
pub fn print(msg: string) -> i32 { ... }
pub fn panic(msg: string) -> void { ... }
```

### stdlib/collections/vec.paw
```paw
pub type Vec<T> = struct {
    ptr: i32,
    len: i32,
    cap: i32,
    
    pub fn new() -> Vec<T> { ... }
    pub fn with_capacity(cap: i32) -> Vec<T> { ... }
    pub fn push(self, item: T) { ... }
    pub fn pop(self) -> Option<T> { ... }
    pub fn length(self) -> i32 { ... }
}
```

### stdlib/io/mod.paw
```paw
pub import .print.{println, print, eprintln, eprint};
pub import .file.{File, open, read, write};
pub import .stream.{Stream, stdin, stdout, stderr};
```

---

## 🚀 使用示例

### 示例1: 简单项目
```paw
// main.paw
import utils.math;

fn main() -> i32 {
    let sum = math.add(1, 2);
    println("Sum: {sum}");
    return 0;
}
```

### 示例2: 中型项目
```paw
// src/main.paw
import models.{User, Post};
import services.auth.authenticate;
import utils.{math, string};

fn main() -> i32 {
    let user = User { id: 1, name: "Alice" };
    
    if authenticate(user) {
        println("Welcome!");
    }
    
    return 0;
}
```

### 示例3: 库项目
```paw
// src/lib.paw - 库的公共接口
pub import .models.{User, Post};
pub import .utils.{add, multiply};
pub import .core.Engine;

// 使用此库的项目
import my_lib.{User, Engine};

fn main() -> i32 {
    let user = User { id: 1, name: "Bob" };
    let engine = Engine::new();
    return 0;
}
```

---

## 📚 可见性规则

### 1. pub修饰符
```paw
pub fn public_function() { }    // 外部可见
fn private_function() { }       // 模块内部

pub type PublicType = ...;      // 外部可见
type PrivateType = ...;         // 模块内部
```

### 2. 模块级pub
```paw
// utils/math.paw
pub fn add(a: i32, b: i32) -> i32 { a + b }  // 可被外部导入

fn helper() -> i32 { ... }  // 仅模块内部使用
```

### 3. 重新导出
```paw
// utils/mod.paw
pub import .math.add;        // 重新导出add
import .internal.helper;     // 不导出helper

// main.paw
import utils.add;  // ✅ OK
import utils.helper;  // ❌ Error: helper is private
```

---

## 🔧 实现优先级

### v0.2.0 (核心)
- ⭐⭐⭐ 多项导入 `import a.{b, c}`
- ⭐⭐⭐ mod.paw 模块入口
- ⭐⭐⭐ 标准库模块化
- ⭐⭐ 项目配置 (paw.toml)
- ⭐⭐ 模块别名 `import a as b`

### v0.3.0 (增强)
- ⭐⭐ 通配符导入 `import a.*`
- ⭐⭐ 重新导出 `pub import`
- ⭐ 相对导入 `import .sibling`
- ⭐ 包管理器集成

---

## 🧪 测试计划

### 测试1: 多项导入
```paw
// math.paw
pub fn add(a: i32, b: i32) -> i32 { a + b }
pub fn sub(a: i32, b: i32) -> i32 { a - b }
pub const PI: f64 = 3.14159;

// main.paw
import math.{add, sub, PI};

fn main() -> i32 {
    let sum = add(1, 2);
    let diff = sub(5, 3);
    return sum + diff;
}
```

### 测试2: mod.paw入口
```
src/utils/
├── mod.paw          # pub import .math.*;
├── math.paw
└── string.paw

main.paw:
import utils.math.add;  // 通过mod.paw找到
```

### 测试3: 标准库
```paw
// 无需import，prelude自动可用
let vec = Vec<i32>::new();
println("Hello");

// 需要import
import std.collections.HashMap;
let map = HashMap<string, i32>::new();
```

---

## 💡 迁移路径

### 从v0.1.3到v0.2.0

**完全向后兼容**：
```paw
// v0.1.3 语法继续支持
import math.add;
import math.Vec2;

// v0.2.0 新语法（推荐）
import math.{add, Vec2};
```

**无需修改现有代码**！

---

## 📊 预期收益

### 代码组织
- ✅ 清晰的项目结构
- ✅ 模块化设计
- ✅ 易于维护

### 开发效率
- ✅ 减少import语句
- ✅ 更好的代码导航
- ✅ IDE支持更好

### 团队协作
- ✅ 明确的模块边界
- ✅ 可见性控制
- ✅ 接口设计

---

## 🚀 实现时间表

### Week 1 (核心功能)
- Day 1-2: 多项导入语法
- Day 3-4: mod.paw支持
- Day 5: 项目配置

### Week 2 (标准库)
- Day 1-3: 标准库重组
- Day 4-5: 测试和文档

### Week 3 (完善)
- Day 1-2: 模块别名
- Day 3-4: 重新导出
- Day 5: 最终测试

**总计**: 3周完成工程化模块系统

---

## 📋 检查清单

### 功能完整性
- [ ] 多项导入
- [ ] mod.paw支持
- [ ] 模块别名
- [ ] 重新导出
- [ ] 标准库模块化
- [ ] 项目配置

### 质量保证
- [ ] 所有测试通过
- [ ] 文档完整
- [ ] 示例丰富
- [ ] 向后兼容

### 用户体验
- [ ] 清晰的错误消息
- [ ] 合理的默认行为
- [ ] 易学易用

---

## 🔗 参考

### Rust模块系统
```rust
mod math;
use math::{add, multiply};
use std::collections::HashMap;
```

### TypeScript模块系统
```typescript
import { add, multiply } from './math';
import * as math from './math';
export { add } from './math';
```

### Python模块系统
```python
from math import add, multiply
import math as m
from . import sibling
```

---

## 🎯 开始实现

准备好开始构建工程化模块系统了吗？

建议从**多项导入**开始，它是最高优先级且最有价值的功能！

