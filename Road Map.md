# PawLang 自举计划

PawLang 是一个实验性编程语言，当前使用 Rust 编写前端（AST、Parser、TypeChecker）和后端（Cranelift → 对象文件 → Zig CC 链接）。
我们的目标是 **实现自举** —— 用 PawLang 自己实现自己的编译器。

---

## 🎯 总体路线

自举过程分为 **两阶段三步走**：

1. **阶段 0（现状）**

    * 编译器完全用 Rust 实现，依赖 Zig CC 进行链接。
    * 运行时 `pawrt` 使用 `no_std`，跨平台支持（Windows / Linux / macOS）。

2. **阶段 1（半自举）**

    * 抽取后端为一个 C ABI 静态库（`libpawbackend.a`）。
    * 在 PawLang 中实现前端（lexer、parser、typecheck、IR 降级）。
    * Paw 前端通过 FFI 调用 Rust 后端，生成目标文件并链接运行时。

3. **阶段 2（完全自举）**

    * 逐步用 PawLang 实现后端（例如先做 Paw → C，再到 Paw → 对象文件）。
    * 编译器完全脱离 Rust，仅用 PawLang 和运行时库构建。

---

## 📦 阶段规划

### 阶段 0：准备与稳定化

* [x] 运行时 `pawrt`：提供基本 IO、文件读取、字符串、退出。
* [x] 新增内存分配接口：`paw_malloc` / `paw_realloc` / `paw_free_raw`。
* [x] 使用 Zig CC / LLD 进行跨平台链接。
* [ ] 增加简单的容器库（`string.paw`, `vec.paw`）。

### 阶段 1：半自举

* [ ] 抽取 Rust 后端为 `libpawbackend.a`，导出 C ABI 接口：

    * `paw_backend_new`
    * `paw_backend_declare_fn`
    * `paw_backend_define_fn_ir`
    * `paw_backend_finalize_to_path`
    * `paw_backend_free`
* [ ] 定义最小 Paw IR（文本形式，先易实现）：

    * 支持函数定义、变量、常量、加减乘除、条件分支、return。
* [ ] 用 PawLang 实现前端：

    * 词法分析器（lexer.paw）
    * 语法分析器（parser.paw）
    * 抽象语法树（ast.paw）
    * 类型检查（typecheck.paw，简化）
    * IR 降级（lower\_ir.paw → 文本 IR）
* [ ] 使用 `pawc0` 编译 `paw_frontend.paw` → 产出 `pawc1`。
* [ ] 使用 `pawc1` 编译 `paw_frontend.paw` → 产出 `pawc2`。
* [ ] 验证：

    * 功能一致：用同一组示例程序测试。
    * （可选）二进制可重复性：尽量稳定输出。

### 阶段 2：完全自举

* [ ] 实现 Paw → C 的后端（最简单方案）。
* [ ] 使用系统 C 编译器（clang / gcc / zig cc）链接运行时。
* [ ] 逐步扩展为 Paw → 目标文件（COFF/ELF/Mach-O）。
* [ ] 移除 Rust 后端，仅保留运行时 `pawrt`（Rust 写的，但可长期沿用）。
* [ ] PawLang 编译器完全用 PawLang 自身实现。

---

## 🔄 迭代与验证

* 每完成一个阶段，必须验证 **pawc 能编译自己**。
* 保留 `examples/` 测试用例，保证输出结果一致。
* 可以使用 CI（GitHub Actions）跑跨平台测试。

---

## 📌 下一步

当前建议优先完成：

1. 在运行时增加 `paw_malloc/paw_free_raw`（支持容器）。
2. 抽出后端到 `libpawbackend.a`。
3. 在 PawLang 中实现最小前端，跑通第一个“半自举”。
