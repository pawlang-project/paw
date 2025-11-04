# PawLang Optional 类型语法指南

> 📋 **统一的 Optional 构造语法**  
> 版本：v0.2.2
> 更新时间：2025-11-04

---

## 🎯 语法总览

PawLang 提供统一的 Optional 语法：`some()` / `none`

```paw
// 构造有值的 Optional
let x: i32? = some(42);

// 构造空的 Optional  
let y: i32? = none;

// 模式匹配
result is {
    some(val) => val,
    none => default_value
}
```

---

## 📝 详细用法

### 1. 创建 Optional 值

```paw
// ✅ 使用 some() 构造有值
let name: string? = some("Alice");
let age: i32? = some(30);
let active: bool? = some(true);
```

### 2. 创建空的 Optional

```paw
// ✅ 使用 none 构造空值
let empty: i32? = none;
let nothing: string? = none;
```

### 3. 模式匹配

```paw
fn process(opt: i32?) -> i32 {
    return opt is {
        some(val) => {
            println("有值:");
            println(val);
            val
        },
        none => {
            println("无值");
            0
        }
    };
}
```

### 4. none 检查

```paw
let opt: i32? = some(42);

if opt == none {
    println("是空值");
}
```

---

## 🎓 最佳实践

### ✅ 推荐写法

```paw
// 一致使用 some/none
let有值: i32? = some(42);
let空值: i32? = none;

opt is {
    some(v) => v,
    none => 0
}
```

**理由**:
- 语法对称（some 对应 none）
- 意图明确
- 与其他语言（Rust, Swift）相似

---

## 🔍 技术细节

### 内部实现

```
some(value) → CallExpr("some", [value])
            → TypeChecker: Optional<T>
            → CodeGen: { i1 true, T value }

none        → NoneLiteral
            → TypeChecker: Optional<void>  
            → CodeGen: { i1 false, i8 undef }
```

### 类型推导

```paw
// some() - 从参数推导
let x = some(42);      // 推导为 i32?
let y = some("hello"); // 推导为 string?

// none - 需要类型注解
let z: i32? = none;    // ✅ 必须标注类型
let w = none;          // ⚠️ 推导为 void?
```

---

## 📊 对比其他语言

| 语言 | 有值构造 | 空值构造 | 模式匹配 |
|-----|---------|---------|---------|
| **PawLang** | `some(42)` | `none` | `some(v)` / `none` |
| Rust | `Some(42)` | `None` | `Some(v)` / `None` |
| Swift | `.some(42)` 或 `42` | `nil` | `case .some(let v)` / `case .none` |
| Kotlin | `42` | `null` | `is Int` / `null` |

---

## 📖 示例

### 完整示例：用户查询

```paw
type User = struct {
    id: i32,
    name: string
}

fn find_user(id: i32) -> User? {
    if id <= 0 {
        return none;  // ✅ 返回空
    }
    return some(User { id: id, name: "User" });  // ✅ 返回值
}

fn main() {
    let user = find_user(1);
    
    let name = user is {
        some(u) => u.name,
        none => "Unknown"
    };
    
    println(name);
}
```

---

## ✅ 总结

### 语法

```
有值 Optional:  some(value)  ⭐
空的 Optional:  none          ⭐
模式匹配:       some(v) / none  ⭐
```

### 关键点

1. ✅ `some()` - 显式构造，类型明确
2. ✅ `none` - Optional 空值
3. ✅ 模式匹配使用 `some(v)` / `none`

---

**PawLang v1.4.2 - Optional 语法统一完成！** 🐾

---

*文档版本: v1.4.2*  
*最后更新: 2025-11-04*  
*状态: 已实现并测试*
