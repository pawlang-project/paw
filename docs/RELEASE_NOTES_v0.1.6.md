# 🎯 PawLang v0.1.6 Release Notes

**Release Date**: TBD

**Theme**: 完善 let mut 系统

---

## 🌟 Highlights

### 完整的可变性控制系统 ⭐

PawLang v0.1.6 实现了完整的可变性控制，这是向 Rust 级别内存安全迈出的重要一步！

- ✅ **let vs let mut**: 变量默认不可变，必须显式声明 `mut` 才能修改
- ✅ **编译期检查**: 尝试修改不可变变量会导致编译错误
- ✅ **mut self 支持**: 方法可以声明 `mut self` 来修改对象
- ✅ **清晰错误消息**: 提供有用的错误提示和修复建议

---

## 📦 What's New

### 1. let mut 语法

**不可变变量（默认）**:
```paw
let x = 10;
// x = 20;  // ❌ 编译错误: Cannot assign to immutable variable 'x'
```

**可变变量（显式声明）**:
```paw
let mut y = 10;
y = 20;      // ✅ OK
y += 5;      // ✅ OK
```

**对比**:
```paw
fn example() -> i32 {
    let immutable = 10;        // 不可变
    let mut mutable = 20;      // 可变
    
    // immutable = 15;         // ❌ 编译错误
    mutable = mutable + 10;    // ✅ OK
    
    return immutable + mutable;  // 40
}
```

### 2. mut self 支持

**不可变方法 vs 可变方法**:
```paw
type Counter = struct {
    value: i32,
    
    // 不可变方法：只能读取
    fn get_value(self) -> i32 {
        return self.value;
    }
    
    // 可变方法：可以修改
    fn increment(mut self) -> i32 {
        self.value = self.value + 1;  // ✅ OK
        return self.value;
    }
}

fn main() -> i32 {
    let mut counter = Counter { value: 0 };
    
    let v1 = counter.get_value();   // 0
    let v2 = counter.increment();   // 1
    let v3 = counter.increment();   // 2
    
    return v1 + v2 + v3;  // 3
}
```

### 3. 编译期可变性检查

**类型检查器现在会验证**:
- ✅ 只有 `let mut` 变量可以被赋值
- ✅ 只有 `mut self` 方法可以修改 `self`
- ✅ 参数默认不可变（未来可能支持 `mut` 参数）

**错误消息示例**:
```
Error: Cannot assign to immutable variable 'x'. Use 'let mut x' to make it mutable.
```

### 4. Backend 修复

**C Backend**:
- 修复：函数最后的表达式语句现在正确生成 `return` 语句
- 影响：所有非 `void` 函数现在都能正确返回值

**LLVM Backend**:
- 修复：函数最后的表达式语句现在正确生成 `ret` 指令
- 影响：和 C backend 保持一致的行为

**Before (Bug)**:
```c
int32_t test() {
    int32_t x = 42;
    x;  // ❌ 缺少 return
}
```

**After (Fixed)**:
```c
int32_t test() {
    int32_t x = 42;
    return x;  // ✅ 正确生成 return
}
```

---

## 🔧 Technical Details

### AST Changes

**新增字段**:
```zig
pub const Param = struct {
    name: []const u8,
    type: Type,
    is_mut: bool,  // 🆕 v0.1.6: 参数可变性
};
```

### TypeChecker Improvements

**新增内容**:
- `mutable_vars: StringHashMap(bool)` - 跟踪变量可变性
- `checkMutability()` - 验证赋值目标是否可变
- 在 `let_decl` 时记录变量的 `is_mut` 标志
- 在 `assign` 和 `compound_assign` 时检查可变性

**内存管理改进**:
- 修复错误消息内存泄漏
- 在 `deinit()` 中释放所有动态分配的错误消息

### Parser Improvements

**三处参数解析更新**:
1. 普通函数参数解析
2. Trait 方法签名解析
3. Enum 方法签名解析

所有三处现在都正确解析和记录 `mut self` 和 `mut` 参数。

### Code Generator Fixes

**C Backend**:
```zig
// 特殊处理最后一个表达式语句
for (func.body, 0..) |stmt, i| {
    const is_last = (i == func.body.len - 1);
    const is_non_void = func.return_type != .void;
    
    if (is_last and stmt == .expr and is_non_void) {
        try self.output.appendSlice("return ");
        _ = try self.generateExpr(stmt.expr);
        try self.output.appendSlice(";\n");
    } else {
        try self.generateStmt(stmt);
    }
}
```

**LLVM Backend**: 类似的修复

---

## 🧪 Testing

### 新增测试

**tests/syntax/let_mut_complete_test.paw**:
- 6 个测试函数覆盖核心场景
- 测试内容：
  - 基本可变变量
  - 复合赋值操作
  - 多个可变变量
  - 混合可变/不可变变量
  - 循环中修改变量
  - 不可变变量（对比）

**tests/syntax/test_immutable_error.paw**:
- 验证不可变变量赋值错误检测
- 确保错误消息清晰有用

**tests/syntax/test_mut_self.paw**:
- 验证 `mut self` 功能
- 测试结构体方法修改字段

### 测试结果

所有测试通过：
- ✅ C Backend: 正确的退出码
- ✅ LLVM Backend: 与 C Backend 一致
- ✅ 错误检测: 正确报告不可变性违规

---

## 📊 Compatibility

### Breaking Changes

**无破坏性变更**: 
- 现有代码继续工作
- `let` 变量已经是不可变的（只是现在会强制检查）
- `let mut` 是新语法，不影响现有代码

### Migration Guide

如果你的代码中有修改变量的情况，需要添加 `mut`:

**Before**:
```paw
let counter = 0;
counter = counter + 1;  // 这在 v0.1.5 可能工作，但现在会报错
```

**After**:
```paw
let mut counter = 0;
counter = counter + 1;  // ✅ OK
```

---

## 🎯 Benefits

### 1. 更好的代码意图表达

```paw
let config = load_config();    // 明确：这个不会改变
let mut state = init_state();  // 明确：这个会被修改
```

### 2. 防止意外修改

```paw
fn process(data: Data) -> i32 {
    // data 不能被修改，防止意外 bug
    return data.value;
}
```

### 3. 更安全的并发（未来）

不可变数据天然线程安全，为未来的并发特性打下基础。

### 4. 向 Rust 级别安全迈进

这是 PawLang 实现 Rust 级别内存安全的重要一步：
- ✅ 可变性控制
- ⏳ 所有权系统（未来）
- ⏳ 借用检查器（未来）
- ⏳ 生命周期（未来）

---

## 🐛 Bug Fixes

1. **C Backend**: 修复函数最后表达式不生成 return 的问题
2. **LLVM Backend**: 修复函数最后表达式不生成 ret 的问题
3. **TypeChecker**: 修复错误消息内存泄漏

---

## 📈 Performance

- **编译速度**: 无明显影响
- **运行时性能**: 无影响（零开销抽象）
- **内存使用**: TypeChecker 内存使用略微增加（跟踪可变性）

---

## 🔮 Future Work

v0.1.7 计划:
- [ ] LLVM 优化支持 (-O0, -O1, -O2, -O3)
- [ ] 增强错误消息（源码位置，颜色高亮）
- [ ] 字符串类型改进
- [ ] 标准库扩展

长期目标:
- [ ] 所有权系统
- [ ] 借用检查器
- [ ] 生命周期
- [ ] Trait 系统

---

## 🙏 Acknowledgments

感谢所有测试者和贡献者！

---

## 📄 Full Changelog

查看完整变更：[CHANGELOG.md](../CHANGELOG.md#016---tbd)

---

<div align="center">

**🐾 PawLang v0.1.6 - 向内存安全又迈进一步！**

</div>

