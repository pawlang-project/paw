# Paw 可见性控制指�?

## 核心理念

**默认私有，显式公开** - 确保封装性和安全�?

---

## 基本规则

### 1. 默认行为

```paw
// 所有声明默认私�?
type Internal = struct { x: int }
fn helper() { }
let CONSTANT = 42

// 这些都是私有的，仅在当前模块内可�?
```

### 2. 公开声明

```paw
// �?pub 关键字标记公开
pub type Point = struct { x: float, y: float }
pub fn api_call() -> string { "ok" }
pub let CONFIG = "value"
```

---

## 类型可见�?

### 结构�?

```paw
// 私有结构�?
type Internal = struct {
    data: int
}

// 公开结构体，私有字段
pub type User = struct {
    id: int             // 私有字段
    name: string        // 私有字段
    
    pub fn new(name: string) -> Self {  // 公开方法
        User { id: generate_id(), name }
    }
    
    pub fn name(self) -> string {  // 公开�?getter
        self.name
    }
}

// 公开结构体，混合字段
pub type Point = struct {
    pub x: float        // 公开字段 - 直接访问
    pub y: float        // 公开字段 - 直接访问
    internal: int       // 私有字段 - 封装
    
    pub fn new(x: float, y: float) -> Self {
        Point { x, y, internal: 0 }
    }
    
    fn update_internal(mut self) {  // 私有方法
        self.internal += 1
    }
}
```

### 枚举

```paw
// 私有枚举
type InternalError = enum {
    NetworkError
    ParseError
}

// 公开枚举（所有变体自动公开�?
pub type Status = enum {
    Active      // 公开变体
    Inactive    // 公开变体
    Pending     // 公开变体
}

// 公开枚举，带方法
pub type Result<T, E> = enum {
    Ok(T)
    Err(E)
    
    pub fn is_ok(self) -> bool {  // 公开方法
        self is { Ok(_) -> true, Err(_) -> false }
    }
    
    fn internal_check(self) -> bool {  // 私有方法
        // ...
    }
}
```

### Trait

```paw
// 私有 trait
type InternalTrait = trait {
    fn process(self) -> int
}

// 公开 trait
pub type Display = trait {
    fn display(self) -> string
}

// 实现公开 trait
pub type Point = struct {
    pub x: float
    pub y: float
    
    // 实现 Display trait（公开�?
    pub fn display(self) -> string {
        "($self.x, $self.y)"
    }
}
```

---

## 函数可见�?

```paw
// 私有函数（默认）
fn internal_helper() -> int {
    42
}

// 公开函数
pub fn public_api() -> string {
    "Hello"
}

// 公开函数调用私有函数（允许）
pub fn api_wrapper() -> int {
    internal_helper()  // OK - 同模块内
}

// 泛型公开函数
pub fn process<T>(item: T) -> T {
    item
}

// 公开异步函数
pub fn fetch(url: string) async -> Result<string, Error> {
    http.get(url).await
}
```

---

## 模块可见�?

```paw
database {
    // 私有类型
    type Connection = struct {
        handle: int
    }
    
    // 公开�?API
    pub type Database = struct {
        conn: Connection    // 使用私有类型（OK�?
        
        pub fn connect(url: string) async -> Result<Self, Error> {
            let conn = Connection { handle: 0 }
            Ok(Database { conn })
        }
        
        pub fn query(self, sql: string) async -> Result<[Row], Error> {
            // ...
        }
    }
    
    // 私有辅助函数
    fn validate_sql(sql: string) -> bool {
        !sql.is_empty()
    }
}

// 使用
import database.Database  // OK - Database 是公开�?
// import database.Connection  // 错误！Connection 是私有的
```

---

## 嵌套模块

```paw
pub api {
    pub v1 {
        pub type User = struct {
            pub id: int
            pub name: string
        }
        
        pub fn get_user(id: int) -> Option<User> {
            // ...
        }
    }
    
    internal {
        // 私有模块 - �?api 模块内可�?
        pub fn helper() { }
    }
}

// 使用
import api.v1.User        // OK
import api.v1.get_user    // OK
// import api.internal.helper  // 错误！internal 是私有模�?
```

---

## 最佳实�?

### 1. 最小暴露原�?

```paw
pub type UserService = struct {
    users: [User]       // 私有 - 隐藏实现细节
    cache: HashMap<int, User>  // 私有
    
    pub fn new() -> Self {  // 公开构造器
        UserService { users: [], cache: HashMap.new() }
    }
    
    pub fn add_user(mut self, user: User) -> Result<(), Error> {
        self.validate_user(&user)?
        self.users.push(user)
        self.update_cache(user)
        Ok(())
    }
    
    pub fn get_user(self, id: int) -> Option<User> {
        self.cache.get(id)
    }
    
    fn validate_user(self, user: &User) -> Result<(), Error> {
        // 私有验证逻辑
    }
    
    fn update_cache(mut self, user: User) {
        // 私有缓存更新
    }
}
```

### 2. 渐进式公开

```paw
// 第一版：全部私有，仅测试使用
type Config = struct {
    host: string
    port: int
    
    fn new() -> Self { }
    fn validate(self) -> bool { }
}

// 第二版：公开基本 API
pub type Config = struct {
    host: string        // 仍然私有
    port: int          // 仍然私有
    
    pub fn new() -> Self { }  // 公开构造器
    fn validate(self) -> bool { }  // 保持私有
}

// 第三版：根据需求公开字段
pub type Config = struct {
    pub host: string    // 公开字段
    pub port: int       // 公开字段
    
    pub fn new() -> Self { }
    pub fn validate(self) -> bool { }  // 现在公开
}
```

### 3. 接口分离

```paw
// 公开的接�?
pub type Database = trait {
    fn query(self, sql: string) async -> Result<[Row], Error>
    fn execute(self, sql: string) async -> Result<int, Error>
}

// 私有的实�?
type PostgresDB = struct {
    conn: Connection
    
    // 实现公开�?trait
    pub fn query(self, sql: string) async -> Result<[Row], Error> {
        // ...
    }
    
    pub fn execute(self, sql: string) async -> Result<int, Error> {
        // ...
    }
    
    // 私有的辅助方�?
    fn prepare_statement(self, sql: string) -> Statement {
        // ...
    }
}
```

---

## 实际示例：HTTP �?

```paw
// ========================================
// 公开�?API
// ========================================

pub type Server = struct {
    listener: Listener  // 私有实现细节
    
    pub fn bind(addr: string) -> Result<Self, Error> {
        let listener = Listener.bind(addr)?
        Ok(Server { listener })
    }
    
    pub fn serve(self, handler: fn(Request) async -> Response) async -> Result<(), Error> {
        loop {
            let conn = self.listener.accept().await?
            spawn(handle_connection(conn, handler))
        }
    }
}

pub type Request = struct {
    method: string
    path: string
    headers: HashMap<string, string>
    body: [u8]
    
    pub fn method(self) -> string { self.method }
    pub fn path(self) -> string { self.path }
    pub fn header(self, name: string) -> Option<string> {
        self.headers.get(name)
    }
    pub fn json<T>(self) async -> Result<T, Error> {
        json.decode(self.body)
    }
}

pub type Response = struct {
    status: int
    headers: HashMap<string, string>
    body: [u8]
    
    pub fn ok(body: string) -> Self {
        Response {
            status: 200
            headers: HashMap.new()
            body: body.as_bytes()
        }
    }
    
    pub fn json<T>(data: T) -> Self {
        Response {
            status: 200
            headers: hashmap!{ "Content-Type" => "application/json" }
            body: json.encode(data)
        }
    }
    
    pub fn not_found() -> Self {
        Response { status: 404, headers: HashMap.new(), body: [] }
    }
}

// ========================================
// 私有的实现细�?
// ========================================

type Listener = struct {
    socket: Socket
    
    fn bind(addr: string) -> Result<Self, Error> {
        // ...
    }
    
    fn accept(self) async -> Result<Connection, Error> {
        // ...
    }
}

type Connection = struct {
    stream: TcpStream
}

fn handle_connection(
    conn: Connection,
    handler: fn(Request) async -> Response
) async {
    let request = parse_request(conn) else { return }
    let response = handler(request).await
    send_response(conn, response).await
}

fn parse_request(conn: Connection) -> Option<Request> {
    // 私有解析逻辑
}

fn send_response(conn: Connection, response: Response) async {
    // 私有发送逻辑
}
```

---

## 可见性总结

### 规则�?

| �?| 默认 | 公开方式 | 作用�?|
|---|------|---------|--------|
| 类型 | 私有 | `pub type` | 模块 |
| 字段 | 私有 | `pub field: Type` | 类型 |
| 方法 | 私有 | `pub fn` | 类型 |
| 函数 | 私有 | `pub fn` | 模块 |
| 常量 | 私有 | `pub let` | 模块 |
| 模块 | 私有 | `pub mod` | 父模�?|

### 访问规则

```paw
outer {
    type Private = struct { x: int }
    pub type Public = struct { pub x: int }
    
    inner {
        // 可以访问 outer 的所有项
        fn use_private() {
            let p = super.Private { x: 42 }  // OK
        }
    }
}

// 外部代码
fn main() -> int {
    // let p = outer.Private { x: 42 }  // 错误！Private 是私有的
    let p = outer.Public { x: 42 }      // OK - Public 是公开�?
    println("${p.x}")                   // OK - x 是公开字段
    0
}
```

---

## 关键�?

1. �?**默认私有** - 安全的默认选择
2. �?**显式公开** - `pub` 关键字清晰明�?
3. �?**细粒度控�?* - 字段、方法、类型独立控�?
4. �?**模块边界** - 清晰的可见性边�?
5. �?**渐进公开** - 可以逐步扩大 API

**�?Rust 的兼容性：** Paw 的可见性系统与 Rust 基本一致，降低学习成本！✨

