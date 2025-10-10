# PawLang 测试套件

本目录包含 PawLang 编译器的完整测试套件。

## 📁 目录结构

```
tests/
├── llvm/          LLVM 后端测试
├── syntax/        基础语法测试
├── types/         类型系统测试
├── generics/      泛型功能测试
├── methods/       方法调用测试
├── modules/       模块系统测试
└── stdlib/        标准库测试
```

## 🧪 测试分类

### LLVM 后端测试 (`llvm/`)

测试 LLVM 原生后端的代码生成功能。

- `llvm_hello.paw` - 基础 Hello World
- `llvm_arithmetic.paw` - 算术运算测试
- `llvm_function.paw` - 函数定义和调用
- `llvm_operators_test.paw` - 完整运算符测试（算术、比较、逻辑）
- `llvm_features_test.paw` - 综合功能测试
- `llvm_static_method_test.paw` - 静态方法调用
- `loop_simple_test.paw` - 简单循环测试
- `loop_syntax_test.paw` - 循环语法完整测试
- `test_llvm_c_api.zig` - LLVM C API 绑定测试

**运行方式**：
```bash
./zig-out/bin/pawc tests/llvm/llvm_hello.paw --backend=llvm
```

### 基础语法测试 (`syntax/`)

测试 PawLang 的基础语法特性。

- `02_let_mut.paw` - 可变变量声明和赋值
- `05_type_struct.paw` - 结构体定义
- `06_type_with_methods.paw` - 带方法的类型

**运行方式**：
```bash
./zig-out/bin/pawc tests/syntax/02_let_mut.paw --backend=c
```

### 类型系统测试 (`types/`)

测试类型检查和类型推断功能。

- `11_type_errors.paw` - 类型错误检测
- `test_type_inference_basic.paw` - 基础类型推断
- `test_type_inference_v1.paw` - 类型推断 v1
- `test_advanced_inference.paw` - 高级类型推断

**运行方式**：
```bash
./zig-out/bin/pawc check tests/types/test_type_inference_basic.paw
```

### 泛型测试 (`generics/`)

测试泛型函数和泛型结构体。

- `test_generic_struct_complete.paw` - 完整泛型结构体测试
- `test_multi_type_params.paw` - 多类型参数测试

**运行方式**：
```bash
./zig-out/bin/pawc tests/generics/test_generic_struct_complete.paw --backend=c
```

### 方法测试 (`methods/`)

测试实例方法和静态方法。

- `test_instance_methods.paw` - 实例方法测试
- `test_static_methods.paw` - 静态方法测试
- `test_methods_complete.paw` - 完整方法测试

**运行方式**：
```bash
./zig-out/bin/pawc tests/methods/test_instance_methods.paw --backend=c
```

### 模块系统测试 (`modules/`)

测试模块导入和导出功能。

- `test_modules.paw` - 基础模块系统
- `test_mod_entry.paw` - 模块入口点
- `test_multi_import.paw` - 多项导入

**运行方式**：
```bash
./zig-out/bin/pawc tests/modules/test_modules.paw --backend=c
```

### 标准库测试 (`stdlib/`)

测试标准库功能。

- `test_stdlib.paw` - 标准库函数测试

**运行方式**：
```bash
./zig-out/bin/pawc tests/stdlib/test_stdlib.paw --backend=c
```

## 🚀 运行所有测试

### 测试 C 后端
```bash
for file in tests/**/*.paw; do
    echo "Testing: $file"
    ./zig-out/bin/pawc "$file" --backend=c || echo "Failed: $file"
done
```

### 测试 LLVM 后端
```bash
for file in tests/llvm/*.paw; do
    echo "Testing: $file"
    ./zig-out/bin/pawc "$file" --backend=llvm || echo "Failed: $file"
done
```

### 测试类型检查
```bash
for file in tests/**/*.paw; do
    echo "Checking: $file"
    ./zig-out/bin/pawc check "$file" || echo "Failed: $file"
done
```

## 📊 测试统计

- **总测试文件**：26 个
- **LLVM 测试**：9 个
- **语法测试**：3 个
- **类型测试**：4 个
- **泛型测试**：2 个
- **方法测试**：3 个
- **模块测试**：3 个
- **标准库测试**：1 个
- **Zig 测试**：1 个

## 🎯 测试覆盖

- ✅ 基础语法（变量、函数、结构体）
- ✅ 类型系统（推断、检查、错误）
- ✅ 泛型（函数、结构体、多类型参数）
- ✅ 方法（实例方法、静态方法）
- ✅ 模块系统（导入、导出）
- ✅ LLVM 后端（IR 生成、优化）
- ✅ 控制流（if、loop、break、continue）
- ✅ 运算符（算术、比较、逻辑）
- ✅ 标准库（IO 函数）

## 💡 添加新测试

在相应的子目录中创建新的 `.paw` 文件：

```bash
# 创建新的 LLVM 测试
echo 'fn main() -> i32 { return 42; }' > tests/llvm/my_test.paw

# 运行测试
./zig-out/bin/pawc tests/llvm/my_test.paw --backend=llvm
```

---

**注意**：所有测试都应该能够成功编译（除了 `11_type_errors.paw`，它专门用于测试类型错误检测）。

