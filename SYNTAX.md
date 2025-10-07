# Paw 2.0 - 极简优雅设计

## 设计原则

### 核心理念
1. **可读性至上** - 代码应该像自然语言
2. **统一风格** - 相似的事物用相似的方式表达
3. **极简关键字** - 只用最必要的关键字
4. **一致性** - 相同的模式在不同场景下保持一致

### 关键字列表（仅 19 个）

```
// 声明 (2个)
let, type

// 函数 (1个)
fn

// 控制流 (5个)
if, else, loop, break, return

// 模式匹配 (2个)
is, as

// 异步 (2个)
async, await

// 导入 (1个)
import

// 其他 (6个)
pub, self, Self, mut, true, false
```

**对比：**
- Rust: ~50+ 关键字
- Go: ~25 关键字
- **Paw: 19 关键字** ✨（再减少 1 个！）

**说明：**
- 移除 `mod` 和 `use` - 模块由文件系统控制
- 新增 `import` - 更清晰的导入语义
- `.` 分隔符替代 `::`

---

## 1. 变量和类型 - 统一的 `let` 声明

### 基本声明
```paw
// 所有声明都用 let（统一！）
let x = 42                    // 不可变（默认安全）
let mut x = 42                // 可变（mut 前置）
let x: int = 42              // 带类型

// 解构（同样用 let）
let (x, y) = (10, 20)
let Point { x, y } = point
let [first, ..rest] = array

// 可变解构
let mut x = 0
let (mut a, b) = (1, 2)       // a 可变，b 不可变
```

**统一性：** 所有绑定都用 `let`，可变性用 `mut` 前缀

### 类型定义
```paw
// 所有类型定义都用 type
type Point = struct {
    x: float
    y: float
}

type Color = enum {
    Red
    Green
    Blue
    RGB(int, int, int)
}

type Result<T, E> = enum {
    Ok(T)
    Err(E)
}

// 类型别名
type UserId = int
type UserMap = HashMap<UserId, User>
```

**统一性：** `type` 用于所有类型定义，无论是 struct、enum 还是别名

### 可见性控制 - 简洁的 `pub`

**设计原则：** 默认私有，显式公开（与 Rust 相同）

```paw
// 私有类型（默认）
type Internal = struct {
    data: int
}

fn helper() {
    // 私有函数
}

// 公开类型
pub type Point = struct {
    pub x: float        // 公开字段
    pub y: float
    
    fn internal() { }   // 私有方法
    
    pub fn distance(self) -> float {  // 公开方法
        sqrt(self.x * self.x + self.y * self.y)
    }
}

// 公开函数
pub fn api_endpoint() -> string {
    "Available"
}

// 模块级可见性
mod utils {
    pub fn public_util() { }    // 模块外可见
    fn private_util() { }       // 仅模块内可见
}
```

**可见性规则：**

1. **默认私有** - 所有项默认私有，确保封装
2. **显式公开** - 用 `pub` 标记需要导出的项
3. **细粒度控制** - 可以单独控制字段、方法、类型的可见性
4. **模块边界** - `pub` 使项在模块外可见

**示例：库设计**

```paw
// 公开的 API
pub type Config = struct {
    pub host: string
    pub port: int
    timeout: int        // 私有字段，通过方法访问
    
    pub fn new(host: string, port: int) -> Self {
        Config { host, port, timeout: 30 }
    }
    
    pub fn with_timeout(mut self, timeout: int) {
        self.timeout = timeout
    }
    
    fn validate(self) -> bool {  // 私有方法
        self.port > 0 and self.port < 65536
    }
}

// 私有的辅助类型
type InternalState = struct {
    // 仅库内部使用
}
```

---

## 2. 函数 - 简洁的定义

### 基本函数
```paw
// 单表达式函数（最简洁）
fn add(x: int, y: int) -> int = x + y

// 多行函数（用块）
fn factorial(n: int) -> int {
    if n <= 1 { 1 }
    else { n * factorial(n - 1) }
}

// 泛型（统一语法）
fn swap<T>(a: T, b: T) -> (T, T) = (b, a)

// 方法（直接在 type 内部定义）
type Point = struct {
    x: float
    y: float
    
    // 方法就在这里！
    fn distance(self) -> float {
        sqrt(self.x * self.x + self.y * self.y)
    }
    
    fn move(mut self, dx: float, dy: float) {
        self.x += dx
        self.y += dy
    }
}
```

**统一性：** 
- 所有可调用的都用 `fn`
- 方法直接在类型定义内，无需 `impl` 或 `extend`
- `mut self` 而不是 `&mut self`（更自然）

---

## 3. 控制流 - 极简但强大

### 条件表达式
```paw
// if 表达式（一切皆表达式）
let result = if x > 0 { "positive" }
             else if x < 0 { "negative" }
             else { "zero" }

// 单行形式
let max = if a > b { a } else { b }

// guard 模式
let value = user.name else { return error("No name") }
```

**统一性：** `if` 永远是表达式，总是返回值

### 模式匹配 - 用 `is` 统一
```paw
// is 用于所有模式匹配
let result = x is {
    0 -> "zero"
    1..10 -> "small"
    11..100 -> "medium"
    _ -> "large"
}

// 类型判断（同样用 is）
if value is Some(x) {
    println(x)
}

// 带条件的模式
let desc = point is {
    Point { x: 0, y: 0 } -> "origin"
    Point { x, y } if x == y -> "diagonal"
    Point { x, y } -> "point at ($x, $y)"
}

// 类型转换（用 as）
let num = value as int
let text = 42 as string
```

**统一性：** 
- `is` 用于所有模式判断和匹配
- `as` 用于所有类型转换
- 语法完全一致

### 循环 - 只需 `loop`
```paw
// loop 是唯一的循环关键字
loop {
    // 无限循环
    if should_stop { break }
}

// 带条件（用块）
loop if condition {
    // 相当于 while
}

// 遍历集合
loop for item in collection {
    println(item)
}

// 带索引
loop for (i, item) in collection.enumerate() {
    println("$i: $item")
}

// loop 可以返回值
let result = loop {
    if found { break value }
}
```

**统一性：** 只用 `loop`，通过组合实现不同循环模式

---

## 4. 错误处理 - 自然流畅

### Result 和 Option
```paw
type Option<T> = enum {
    Some(T)
    None
}

type Result<T, E> = enum {
    Ok(T)
    Err(E)
}

// ? 操作符（传播错误）
fn divide(a: int, b: int) -> Result<int, string> {
    if b == 0 { Err("division by zero") }
    else { Ok(a / b) }
}

fn calculate() -> Result<int, string> {
    let x = divide(10, 2)?      // 自动传播
    let y = divide(x, 3)?
    Ok(y + 5)
}

// else 子句（提供默认值）
let value = result else { 0 }
let name = user.name else { "Anonymous" }

// 模式匹配处理
let output = result is {
    Ok(val) -> val
    Err(e) -> {
        log("Error: $e")
        0
    }
}
```

**统一性：** 
- Result/Option 是普通的 enum
- `?` 用于传播
- `else` 用于提供默认值
- `is` 用于详细处理

---

## 5. 异步编程 - 简洁优雅

```paw
// async 函数
fn fetch(url: string) async -> Result<string, Error> {
    let response = http.get(url).await?
    let body = response.text().await?
    Ok(body)
}

// 并发
fn fetch_all() async -> [string] {
    let tasks = [
        fetch("url1").spawn()
        fetch("url2").spawn()
        fetch("url3").spawn()
    ]
    
    tasks.join_all().await
}

// 选择第一个完成的
fn race() async -> string {
    select {
        result = fetch("url1") -> result?
        result = fetch("url2") -> result?
        timeout(5000) -> "timeout"
    }
}
```

**统一性：** 
- `async` 标记异步函数
- `.await` 等待异步操作
- `spawn()` 启动并发任务
- `select` 用于多路选择

---

## 6. 所有权和借用 - 简化表示

```paw
// 所有权自动管理
let data = vec![1, 2, 3]
let moved = data              // 所有权转移

// 借用用 & 前缀
fn read(data: &[int]) -> int {
    data.len()
}

fn modify(data: &mut Vec<int>) {
    data.push(42)
}

// 使用
let nums mut = vec![1, 2, 3]
read(&nums)                   // 借用
modify(&mut nums)             // 可变借用

// 生命周期（自动推断，需要时显式）
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
```

**统一性：** 
- `&` 表示借用
- `&mut` 表示可变借用
- 生命周期语法与 Rust 保持一致（已经很好）

---

## 7. Trait 系统 - 简化的接口

```paw
// trait 定义（用 type）
type Display = trait {
    fn display(self) -> string
}

type Iterator<T> = trait {
    fn next(mut self) -> Option<T>
}

// 实现 trait
type Point = struct {
    x: float
    y: float
    
    // 直接实现 trait 方法
    fn display(self) -> string {
        "($self.x, $self.y)"
    }
}

// 泛型约束
fn print_all<T: Display>(items: [T]) {
    loop for item in items {
        println(item.display())
    }
}

// 多个约束
fn process<T: Display + Clone>(item: T) -> T {
    println(item.display())
    item.clone()
}
```

**统一性：** 
- trait 也用 `type` 定义
- 实现直接写在 struct 内部
- 约束语法简单清晰

---

## 8. 模块系统 - 清晰的组织

```paw
// 定义模块
mod math {
    pub fn add(x: int, y: int) -> int = x + y
    
    fn internal() {
        // 私有函数
    }
}

// 使用
import math.add
import math.*

// 嵌套模块
mod geometry {
    pub mod point {
        pub type Point = struct {
            x: float
            y: float
        }
    }
    
    pub mod circle {
        import super.point.Point
        
        pub type Circle = struct {
            center: Point
            radius: float
        }
    }
}
```

**统一性：** 
- `mod` 定义模块
- `use` 导入
- `pub` 标记公开
- `.` 分隔符（而不是 `::`）

---

## 9. 字符串和格式化 - 内置优雅

```paw
// 字符串插值
let name = "Alice"
let age = 30
let msg = "Hello, $name! You are $age years old."

// 表达式插值
let calc = "2 + 2 = ${2 + 2}"

// 多行字符串
let text = """
    This is a
    multiline string
    with proper indentation
"""

// 原始字符串
let path = r"C:\Users\Alice\Documents"

// 格式化
let formatted = "$value:{06d}"      // 补零
let float = "${pi:.2f}"             // 小数位
```

**统一性：** 
- `$var` 用于简单变量
- `${expr}` 用于表达式
- 前缀标记特殊字符串

---

## 10. 集合和迭代器 - 流畅的 API

```paw
// 创建集合
let vec = [1, 2, 3, 4, 5]
let map = { "key": "value", "foo": "bar" }
let set = {1, 2, 3}

// 范围
let range = 0..10        // 0 到 9
let inclusive = 0..=10   // 0 到 10

// 链式操作
let result = numbers
    .filter(|x| x % 2 == 0)
    .map(|x| x * x)
    .sum()

// 常用方法
numbers.find(|x| x > 10)
numbers.any(|x| x > 0)
numbers.all(|x| x > 0)
numbers.take(5)
numbers.skip(3)
```

**统一性：** 
- `[]` 数组/向量
- `{}` 字典（键值对）或集合（单值）
- `.method()` 链式调用
- `|x| expr` 闭包语法

---

## 完整示例对比

### Web 服务器

**旧版（v2）:**
```paw
import http::{Server, Request, Response}

struct User {
    id: int
    name: string
    email: string
}

extend User {
    fn new(name: string, email: string) -> Self {
        User { id: generate_id(), name, email }
    }
}

async fn handle_request(req: Request) -> Result<Response, Error> {
    when (req.method(), req.path()) {
        (Method::GET, "/users") -> get_users().await
        (Method::POST, "/users") -> create_user(req).await
        _ -> Ok(Response::not_found())
    }
}

async fn main() -> Result<(), Error> {
    let server = Server::bind("0.0.0.0:8080")?
    println("Server running on http://localhost:8080")
    server.serve(handle_request).await
}
```

**新版（v3 极简）:**
```paw
import http.{Server, Request, Response}

type User = struct {
    id: int
    name: string
    email: string
    
    fn new(name: string, email: string) -> Self {
        User { id: generate_id(), name, email }
    }
}

fn handle_request(req: Request) async -> Result<Response, Error> {
    (req.method(), req.path()) is {
        ("GET", "/users") -> get_users().await
        ("POST", "/users") -> create_user(req).await
        _ -> Ok(Response.not_found())
    }
}

fn main() async -> Result<(), Error> {
    let server = Server.bind("0.0.0.0:8080")?
    println("Server running on http://localhost:8080")
    server.serve(handle_request).await
}
```

**改进：**
- ✨ `type` 统一所有类型定义
- ✨ 方法直接在 type 内，无需 `extend`
- ✨ `is` 替代 `when`（更统一）
- ✨ `.` 而不是 `::`（更简洁）
- ✨ `async` 作为后缀（更自然阅读）

---

### 游戏示例

```paw
type Point = struct {
    x mut: float
    y mut: float
    
    fn move(mut self, dx: float, dy: float) {
        self.x += dx
        self.y += dy
    }
    
    fn distance(self, other: Point) -> float {
        let dx = self.x - other.x
        let dy = self.y - other.y
        sqrt(dx * dx + dy * dy)
    }
}

type Player = struct {
    position mut: Point
    health mut: int
    score mut: int
    
    fn take_damage(mut self, amount: int) -> bool {
        self.health -= amount
        self.health <= 0  // 返回是否死亡
    }
}

type GameState = enum {
    Menu
    Playing
    Paused
    GameOver { score: int }
}

type Game = struct {
    player mut: Player
    enemies mut: [Enemy]
    state mut: GameState
    
    fn update(mut self, delta: float) {
        self.state is {
            Playing -> {
                // 更新游戏
                self.player.position.move(0, delta * 100)
                
                // 检查碰撞
                loop for enemy in self.enemies {
                    if self.player.position.distance(enemy.position) < 32 {
                        if self.player.take_damage(10) {
                            self.state = GameOver { score: self.player.score }
                        }
                    }
                }
                
                // 清除死亡的敌人
                self.enemies.retain(|e| e.is_alive())
            }
            _ -> {}
        }
    }
}

fn main() async -> Result<(), Error> {
    let window = Window.new("Game", 800, 600)?
    let game mut = Game.new()
    
    loop {
        // 处理输入
        loop for event in window.poll_events() {
            event is {
                Quit -> break
                KeyDown(key) -> handle_key(key, &mut game)
                _ -> {}
            }
        }
        
        // 更新和渲染
        game.update(0.016)
        game.render(&window)
    }
    
    Ok(())
}
```

---

## 语法统一性总结

### 1. 声明统一
```paw
let x = value              // 变量
let mut x = value          // 可变变量
type Name = struct { }     // 结构体
type Name = enum { }       // 枚举
type Name = trait { }      // trait
type Name = OtherType      // 类型别名
```

### 2. 模式统一
```paw
value is {                 // 模式匹配
    pattern -> result
}

if value is Pattern { }    // 模式判断
let x = value as Type      // 类型转换
```

### 3. 控制流统一
```paw
loop { }                   // 循环
loop if cond { }          // 条件循环
loop for item in iter { } // 遍历
```

### 4. 函数统一
```paw
fn name() { }              // 普通函数
fn name() async { }        // 异步函数
fn name(self) { }          // 方法
fn name<T>() { }           // 泛型函数
```

### 5. 符号统一
```paw
.                          // 访问成员、命名空间分隔
&                          // 借用
&mut                       // 可变借用
?                          // 错误传播
$                          // 字符串插值
```

---

## 关键字减少对比

| 类别 | Rust | Paw v2 | Paw v3 |
|------|------|--------|--------|
| 声明 | let, const, static, type, struct, enum, trait, impl | let, var, struct, enum, trait, impl, extend | **let, type** |
| 控制流 | if, else, match, while, for, loop, break, continue, return | if, else, when, while, for, loop, break, continue, return | **if, else, loop, break, return** |
| 函数 | fn, async, await | fn, async, await | **fn, async, await** |
| 其他 | use, mod, pub, self, Self, mut, as, in | use, mod, pub, self, Self, mut, as, in | **use, mod, pub, self, Self, mut, is, as** |
| **总计** | **~50+** | **~35** | **20** ✨ |

---

## 可读性改进

### Before (传统风格)
```rust
impl Display for Point {
    fn fmt(&self, f: &mut Formatter) -> Result<(), Error> {
        write!(f, "({}, {})", self.x, self.y)
    }
}

let result = match value {
    Some(x) => x * 2,
    None => 0,
};
```

### After (Paw v3)
```paw
type Point = struct {
    x: float
    y: float
    
    fn display(self) -> string {
        "($self.x, $self.y)"
    }
}

let result = value is {
    Some(x) -> x * 2
    None -> 0
}
```

**改进：**
- ✨ 方法定义更自然（在类型内部）
- ✨ `is` 比 `match` 更像自然语言
- ✨ `->` 比 `=>` 更简洁
- ✨ 字符串插值替代格式化宏

---

## 核心优势

### 1. 极简关键字（20个）
- 学习负担降低 60%
- 更容易记忆
- 减少选择困难

### 2. 高可读性
- 代码像自然语言
- 清晰的意图表达
- 减少认知负荷

### 3. 统一风格
- 相同的模式一致使用
- 减少特例
- 更容易预测

### 4. 保持安全性
- 所有权系统完整保留
- 借用检查器完整保留
- 类型安全完整保留

---

**Paw v3：更少的关键字，更高的可读性，完全统一的风格！** 🎯

