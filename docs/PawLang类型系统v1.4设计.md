# PawLang v1.4.0 类型系统设计

*新设计 - 2025-10-31*

---

## 🎯 核心类型系统

### 类型修饰符

```paw
T         - 普通类型
T?        - Optional<T>          // 可选值（可能null）     ⭐
T!        - Result<T, string>    // 错误处理（可能失败）   ⭐
T?!       - Result<Optional<T>>  // 可失败的Optional      ⭐
T!?       - Optional<Result<T>>  // 可选的Result          ⭐
```

---

## 📋 详细说明

### `T` - 普通类型

**语义**: 普通的、必定存在的、不会失败的值

**示例**:
```paw
let x: i32 = 10;
let name: string = "Alice";
```

---

### `T?` - Optional类型（可选值） ⭐

**语义**: 值可能存在，也可能为null

**内部表示**: `Optional<T>`

**结构**:
```
struct Optional<T> {
    is_some: bool,   // true=有值, false=null
    value: T,        // 值（null时未初始化）
}
```

**使用**:
```paw
// 定义
let name: string? = "Alice";
let empty: i32? = null;

// null检查
if empty == null {
    println("是null");
}

// 模式匹配
let result = name is {
    null => "未知",
    value => value,  // 自动解包
};

// 安全访问（未来）
let len = name?.len();  // null → null
```

**使用场景**:
- 数据库查询结果（可能不存在）
- 配置项（可能未设置）
- 用户输入（可能为空）

---

### `T!` - Result类型（错误处理） ⭐

**语义**: 操作可能成功（ok），也可能失败（err）

**内部表示**: `Result<T, string>`

**结构**:
```
struct Result<T> {
    is_ok: bool,      // true=ok, false=err
    value: T,         // 成功时的值
    error: string,    // 失败时的错误（固定string类型）
}
```

**使用**:
```paw
// 定义
fn divide(a: i32, b: i32) -> i32! {
    if b == 0 {
        return err("Division by zero");
    }
    return ok(a / b);
}

// Try表达式（自动提取或返回）
fn safe_calc() -> i32! {
    let x = divide(10, 2)!;  // 提取值或返回错误
    return ok(x * 2);
}

// 模式匹配
let result = divide(10, 2) is {
    ok(value) => value,
    err(msg) => {
        println(msg);
        0
    },
};
```

**使用场景**:
- 文件操作（可能失败）
- 网络请求（可能失败）
- 数值计算（可能出错）

---

### `T?!` - Result<Optional<T>>（可失败的Optional） ⭐⭐⭐

**语义**: 操作本身可能失败，成功时结果可能为空

**读法**: "可失败的可选T" 或 "Result包含Optional"

**三种可能值**:
1. `err(msg)` - 操作失败
2. `ok(null)` - 操作成功但结果为空
3. `ok(value)` - 操作成功且有值

**内部表示**:
```
Result<Optional<T>> = {
    is_ok: bool,
    value: Optional<T>,  // 成功时可能是null或值
    error: string,
}
```

**使用**:
```paw
// 数据库查询
fn db_find_user(id: i32) -> User?! {
    // 连接数据库（可能失败）
    let conn = connect_db();
    if conn == null {
        return err("Connection failed");  // ❌ 操作失败
    }
    
    // 查询用户（可能不存在）
    let user = query(conn, id);
    if user == null {
        return ok(null);  // ✅ 操作成功，但用户不存在
    }
    
    return ok(user);  // ✅ 操作成功，找到用户
}

// 使用
fn main() -> i32! {
    let result = db_find_user(1);
    
    let user = result is {
        err(msg) => {
            println(f"数据库错误: {msg}");
            return err(msg);
        },
        ok(null) => {
            println("用户不存在");
            return err("Not found");
        },
        ok(user) => user,
    };
    
    println(f"找到用户: {user.name}");
    return ok(0);
}
```

**使用场景**:
- ✅ 数据库查询（连接失败 vs 未找到）
- ✅ API调用（请求失败 vs 响应为空）
- ✅ 文件查找（IO错误 vs 文件不存在）
- ✅ 缓存查询（系统错误 vs 缓存miss）

---

### `T!?` - Optional<Result<T>>（可选的Result） ⭐⭐

**语义**: 值可能不存在，存在时操作可能失败

**读法**: "可选的Result" 或 "Optional包含Result"

**三种可能值**:
1. `null` - 值不存在（跳过操作）
2. `ok(value)` - 值存在且操作成功
3. `err(msg)` - 值存在但操作失败

**内部表示**:
```
Optional<Result<T>> = {
    is_some: bool,
    value: Result<T>,  // 存在时可能ok或err
}
```

**使用**:
```paw
// 可选的验证
fn verify_if_exists(user: User?) -> User!? {
    // 如果用户不存在，返回null
    if user == null {
        return null;  // ❓ 用户不存在
    }
    
    // 验证用户（可能失败）
    if !validate(user) {
        return err("Validation failed");  // ❌ 验证失败
    }
    
    return ok(user);  // ✅ 验证成功
}

// 使用
fn main() -> i32 {
    let user: User? = find_user(1);
    let result = verify_if_exists(user);
    
    match result {
        null => println("用户不存在，跳过验证"),
        err(msg) => println(f"验证失败: {msg}"),
        ok(user) => println(f"验证通过: {user.name}"),
    }
    
    return 0;
}
```

**使用场景**:
- ⏳ 可选的验证（用户可能不存在）
- ⏳ 条件性处理（数据可能未加载）
- ⏳ 缓存场景（缓存可能不存在或失效）

**注意**: 使用频率比`T?!`低

---

## 📊 类型对照表

| 类型 | 内部表示 | 可能的值 | 主要用途 | 使用频率 |
|-----|---------|---------|---------|---------|
| `T` | `T` | 值 | 普通值 | ⭐⭐⭐⭐⭐ |
| `T?` | `Optional<T>` | 值, null | 可选值 | ⭐⭐⭐⭐⭐ |
| `T!` | `Result<T, string>` | ok, err | 错误处理 | ⭐⭐⭐⭐ |
| `T?!` | `Result<Optional<T>>` | err, ok(null), ok(值) | DB/API | ⭐⭐⭐ |
| `T!?` | `Optional<Result<T>>` | null, ok, err | 可选验证 | ⭐⭐ |

---

## 🎓 使用指南

### 什么时候用`T?`

**场景**: "这个值可能不存在"

```paw
// ✅ 查找操作
fn find_user(id: i32) -> User? {
    if !exists(id) {
        return null;
    }
    return user;
}

// ✅ 配置项
fn get_config(key: string) -> string? {
    return config_map.get(key);  // 可能不存在
}

// ✅ 解析可选字段
fn parse_age(json: Json) -> i32? {
    return json.get("age");  // 字段可能不存在
}
```

### 什么时候用`T!`

**场景**: "这个操作可能失败"

```paw
// ✅ 文件操作
fn read_file(path: string) -> string! {
    if !exists(path) {
        return err("File not found");
    }
    return ok(content);
}

// ✅ 网络请求
fn http_get(url: string) -> string! {
    if network_error {
        return err("Network error");
    }
    return ok(response);
}

// ✅ 数值计算
fn divide(a: i32, b: i32) -> i32! {
    if b == 0 {
        return err("Division by zero");
    }
    return ok(a / b);
}
```

### 什么时候用`T?!`

**场景**: "操作可能失败，结果可能为空"

```paw
// ✅ 数据库查询
fn db_query(sql: string) -> User?! {
    let conn = connect();
    if conn == null {
        return err("Connection failed");  // 连接失败
    }
    
    let user = execute(conn, sql);
    if user == null {
        return ok(null);  // 查询成功但无结果
    }
    
    return ok(user);  // 查询成功且有结果
}

// ✅ API调用
fn api_find(endpoint: string) -> Data?! {
    let response = http_get(endpoint);
    if response.is_error() {
        return err("Request failed");
    }
    if response.is_empty() {
        return ok(null);
    }
    return ok(response.data);
}
```

### 什么时候用`T!?`

**场景**: "值可能不存在，存在时操作可能失败"

```paw
// ⏳ 可选的验证
fn validate_if_present(data: Data?) -> Data!? {
    if data == null {
        return null;  // 数据不存在，跳过验证
    }
    
    if !is_valid(data) {
        return err("Validation failed");
    }
    
    return ok(data);
}
```

---

## 🎯 操作符语义

### `expr?` - 安全访问/null检查

**用于Optional类型**:
```paw
let name: string? = get_name();

// 安全调用（如果实现）
let len = name?.len();  // name为null时返回null

// 链式调用
let first_char = name?.chars()?.first();
```

### `expr!` - Try表达式

**用于Result类型**:
```paw
fn operation() -> Data! {
    let x = step1()!;  // 如果step1失败，自动return err
    let y = step2()!;
    return ok(x + y);
}
```

### 组合使用

```paw
// T?!类型的Try表达式
fn process() -> Data! {
    let result = db_query("SELECT...")!;  // 提取Optional<Data>
    
    if result == null {
        return err("No data");
    }
    
    return ok(result);
}
```

---

## 📊 完整示例

### 示例：用户系统

```paw
type User = struct {
    id: i32,
    name: string,
    email: string?,  // 邮箱可选
}

// 查找用户（可能不存在）
fn find_user(id: i32) -> User? {
    if id == 0 {
        return null;
    }
    return User { id: id, name: "User", email: null };
}

// 发送邮件（可能失败）
fn send_email(to: string, msg: string) -> ()! {
    if !is_valid_email(to) {
        return err("Invalid email");
    }
    // 发送逻辑...
    return ok(());
}

// 查找用户并发送邮件（组合类型）
fn find_and_email(id: i32, msg: string) -> ()?! {
    let user = find_user(id);
    
    if user == null {
        return ok(null);  // 用户不存在（不是错误）
    }
    
    if user.email == null {
        return err("User has no email");  // 用户无邮箱（错误）
    }
    
    send_email(user.email, msg)!;  // 发送（可能失败）
    return ok(());
}

fn main() -> i32! {
    let result = find_and_email(1, "Hello");
    
    match result {
        err(msg) => {
            println(f"错误: {msg}");
            return err(msg);
        },
        ok(null) => {
            println("用户不存在，跳过");
        },
        ok(()) => {
            println("邮件发送成功");
        },
    }
    
    return ok(0);
}
```

---

## 🎯 类型转换

### 自动包装

```paw
// 普通值自动包装为Optional
let x: i32? = 42;        // 自动包装为 Some(42)

// 普通值自动包装为Result
fn returns_result() -> i32! {
    return ok(42);  // 显式包装
}
```

### null传播

```paw
// 安全访问链
user?.address?.city?.name  // 任何一个为null都返回null
```

---

## 📋 关键字和操作符总结

### 关键字

| 关键字 | 用途 | 示例 |
|-------|------|------|
| `null` | null字面量 | `let x: i32? = null;` |
| `ok` | Result成功 | `return ok(42);` |
| `err` | Result失败 | `return err("error");` |

### 后缀修饰符

| 符号 | 类型 | 说明 |
|-----|------|------|
| `?` | Optional | 可能为null |
| `!` | Result | 可能失败 |
| `?!` | Result<Optional> | 组合：失败或空 |
| `!?` | Optional<Result> | 组合：空或失败 |

### 操作符

| 操作符 | 用途 | 示例 |
|-------|------|------|
| `==null` | null检查 | `if x == null { }` |
| `expr?` | 安全访问 | `name?.len()` |
| `expr!` | Try表达式 | `divide(10,2)!` |
| `??` | null合并（未来） | `name ?? "default"` |

---

## 🎊 总结

### 设计亮点

1. ⭐ **`T?` 用于可选值** - 像Java一样简洁
2. ⭐ **`T!` 用于错误处理** - 清晰的语义
3. ⭐ **支持组合类型** - 表达复杂场景
4. ⭐ **符号直观** - `?`疑问，`!`警告
5. ⭐ **null关键字** - 符合主流语言习惯

### 类型系统完整度

```
普通类型:     ████████████████████ 100%
Optional:     ████████████████████ 100% (v1.4.0)
Result:       ████████████████████ 100%
组合类型:     ████████████████████ 100% (v1.4.0)
null支持:     ████████████████████ 100% (v1.4.0)

总体:         ████████████████████ 100%
```

---

**v1.4.0将带来完整的类型系统！** 🎊

---

*设计版本: v1.4.0*  
*更新日期: 2025-10-31*  
*核心: T? + T! + T?! + T!?*  
*状态: 设计完成，待实施* 🚀




