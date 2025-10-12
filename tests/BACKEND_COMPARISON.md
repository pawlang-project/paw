# 🔬 C 后端 vs LLVM 后端对照测试

**日期**: 2025-10-12  
**测试**: 字符串操作和控制流

---

## 🎯 测试用例

### 测试代码 (`tests/syntax/simple_comparison.paw`)

```paw
fn string_length(s: string) -> i32 {
    let mut len: i32 = 0;
    let mut i: i32 = 0;
    
    loop i in 0..100 {
        let ch_code: i32 = s[i] as i32;
        if ch_code == 0 {
            break;  // 关键：测试 if + break
        }
        len += 1;
    }
    
    return len;
}

fn main() -> i32 {
    let test: string = "Hello";
    return string_length(test);  // 应该返回 5
}
```

---

## 📊 测试结果

| 后端 | 编译 | 运行 | 返回值 | 状态 |
|------|------|------|--------|------|
| **C 后端** | ✅ | ✅ | 5 | ✅ 完美 |
| **LLVM 后端** | ✅ | ❌ | - | ❌ 优化器崩溃 |

---

## 🔍 C 后端详细分析

### 生成的 C 代码

```c
int32_t string_length(char* s) {
    int32_t len = 0;
    int32_t i = 0;
    
    for (int32_t i = 0; i < 100; i++) {
        int32_t ch_code = ((int32_t)(s[i]));
        
        if ((ch_code == 0)) {      // ✅ 修复后：生成 if 语句
            break;                  // ✅ break 正常工作
        }
        
        len += 1;
    }
    
    return len;
}
```

### 修复前 vs 修复后

**修复前**:
```c
((ch_code == 0) ? 0 : 0);  // ❌ 三元运算符，没有 break
```

**修复后**:
```c
if ((ch_code == 0)) {      // ✅ if 语句
    break;                 // ✅ break 工作
}
```

### 编译和运行

```bash
$ gcc output.c -o test_c
$ ./test_c
$ echo $?
5  # ✅ 正确！
```

**结论**: ✅ C 后端完全正常

---

## 🔍 LLVM 后端详细分析

### 生成的 LLVM IR

```llvm
loop.body:                                        ; preds = %loop.cond
  %s3 = load ptr, ptr %s, align 8
  %i4 = load i32, ptr %i1, align 4
  %index = getelementptr inbounds [0 x i32], ptr %s3, i32 0, i32 %i4
  %elem = load i32, ptr %index, align 4          ; ✅ 正确加载
  %ch_code = alloca i32, align 4
  store i32 %elem, ptr %ch_code, align 4
  %ch_code5 = load i32, ptr %ch_code, align 4
  %binop = icmp eq i32 %ch_code5, 0
  br i1 %binop, label %if.then, label %if.else    ; ✅ 条件跳转

if.then:                                          ; preds = %loop.body
  br label %loop.exit    ; ✅ 这就是 break！正确跳出循环
  br label %if.cont      ; ⚠️  Dead code（永不执行）

if.else:                                          ; preds = %loop.body
  br label %if.cont

if.cont:                                          ; preds = %if.else, %if.then
  %if.result = phi i32 [ 0, %if.then ], [ 0, %if.else ]
  ; ... 继续循环体
```

### 问题分析

**控制流正确性**: ✅ 
- if.then 正确跳转到 loop.exit（实现 break）
- 控制流图结构正确

**Dead Code 问题**: ⚠️
- `if.then` 块有两个 `br` 指令
- 第一个 `br label %loop.exit` 后不应再有代码
- 第二个 `br label %if.cont` 是 dead code

**优化器崩溃**: ❌
```
Running pass "loop-simplify" on function "string_length"
Bus error: 10
```

**原因**: LLVM 优化器在简化循环时遇到异常的 CFG（control flow graph）结构

---

## 🐛 LLVM 后端的问题

### 1. Dead Code 生成

**症状**: if.then 块生成两个 br 指令

**根源**: `llvm_native_backend.zig` 中 if 表达式生成逻辑

**当前代码** (推测):
```zig
// if.then 块
self.builder.positionAtEnd(then_block);
const then_value = try self.generateExpr(if_expr.then_branch.*);
const then_end_block = self.builder.getInsertBlock();
_ = self.builder.buildBr(cont_block);  // ❌ 总是添加 br
```

**问题**: 即使 then_branch 包含 break，仍然添加到 cont_block 的跳转

### 2. 优化器不兼容

**症状**: loop-simplify pass 崩溃

**可能原因**:
- CFG 中有不可达的 phi 节点
- Dead code 导致前驱块不一致

**解决方案**:
1. 修复 dead code 生成
2. 或者禁用某些优化 pass

---

## ✅ 修复方案

### C 后端修复 (已完成)

**修改**: `src/codegen.zig` (行 474-549)

**改进**: 
- if 表达式作为语句时，生成 if 语句
- 而不是三元运算符
- break 可以正常工作

**结果**: ✅ 完全修复

### LLVM 后端修复 (待完成)

**修改**: `src/llvm_native_backend.zig`

**需要**:
1. 检测 then/else 分支是否已经终止（break/return）
2. 如果已终止，不添加额外的 br 指令
3. 或者生成 unreachable 指令

**优先级**: 中（C 后端已足够）

---

## 📊 性能对比

### 编译速度

| 后端 | 编译时间 | 备注 |
|------|---------|------|
| C | ~0.1s | pawc + gcc |
| LLVM | ❌ 崩溃 | 优化器问题 |

### 生成代码质量

| 后端 | 可读性 | 优化潜力 | 调试友好 |
|------|--------|---------|---------|
| C | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| LLVM | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |

**C 后端优势**:
- 生成的代码几乎和手写一样
- GCC/Clang 可以充分优化
- 调试时可以直接看 C 代码

---

## 🎯 结论

### 问题回答

**Q1: 修复 C 后端 if break 问题**  
✅ **已修复！** if 语句现在正确生成，break 正常工作。

**Q2: LLVM 后端有这个问题吗？**  
⚠️  **LLVM 后端有不同的问题**：
- ✅ 控制流逻辑正确（break 跳转到 loop.exit）
- ❌ 生成了 dead code
- ❌ 导致优化器崩溃

### 使用建议

**当前版本 (v0.2.0-dev)**:
- ✅ 推荐使用 **C 后端**（稳定、可靠）
- ⚠️  LLVM 后端需要进一步调试

**未来计划**:
- 修复 LLVM 后端的 dead code 生成
- 两个后端都应该完全正常

---

## 🔗 相关文件

- **修复**: `src/codegen.zig` (行 474-549)
- **测试**: `tests/syntax/simple_comparison.paw`
- **生成**: `output.c` (C 后端) , `output.ll` (LLVM 后端)

---

**测试者**: PawLang 开发团队  
**日期**: 2025-10-12  

