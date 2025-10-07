# Paw 模块系统

## 核心理念

**文件即模块** - 简洁直观的组织方式

---

## 基本规则

### 1. 一个文件 = 一个模块

```
项目结构：
src/
├── main.paw           # 主模块
├── user.paw          # user 模块
├── database.paw      # database 模块
└── utils/            # utils 模块（目录）
    ├── mod.paw       # utils 模块入口
    ├── string.paw    # utils.string 子模块
    └── math.paw      # utils.math 子模块
```

### 2. 导入使用 `import`

```paw
// main.paw
import user.User
import database.Database
import utils.string.format

fn main() -> int {
    let user = User.new("Alice")
    let db = Database.connect("localhost")
    0
}
```

---

## 文件结构示例

### 单文件模块

**user.paw:**
```paw
// 公开的类型
pub type User = struct {
    pub id: int
    pub name: string
    email: string        // 私有字段
    
    pub fn new(name: string) -> Self {
        User { id: 0, name, email: "" }
    }
    
    pub fn with_email(mut self, email: string) -> Self {
        self.email = email
        self
    }
}

// 私有辅助函数
fn validate_name(name: string) -> bool {
    !name.is_empty()
}
```

**main.paw:**
```paw
import user.User

fn main() -> int {
    let user = User.new("Alice")
        .with_email("alice@example.com")
    
    println("User: ${user.name}")
    0
}
```

---

### 目录模块

**目录结构：**
```
database/
├── mod.paw        # 模块入口（必需）
├── postgres.paw   # 子模块
└── sqlite.paw     # 子模块
```

**database/mod.paw:**
```paw
// 重新导出子模块
pub import postgres.PostgresDB
pub import sqlite.SqliteDB

// 统一接口
pub type Database = trait {
    fn connect(url: string) async -> Result<Self, Error>
    fn query(self, sql: string) async -> Result<[Row], Error>
}

// 工厂函数
pub fn connect(url: string) async -> Result<Box<Database>, Error> {
    if url.starts_with("postgres://") {
        Ok(box PostgresDB.connect(url).await?)
    } else if url.starts_with("sqlite://") {
        Ok(box SqliteDB.connect(url).await?)
    } else {
        Err("Unknown database type")
    }
}
```

**database/postgres.paw:**
```paw
pub type PostgresDB = struct {
    conn: Connection
    
    pub fn connect(url: string) async -> Result<Self, Error> {
        // PostgreSQL 连接逻辑
    }
    
    pub fn query(self, sql: string) async -> Result<[Row], Error> {
        // 查询实现
    }
}

type Connection = struct {
    // 私有实现
}
```

**使用：**
```paw
import database.{Database, connect, PostgresDB}

fn main() async -> Result<(), Error> {
    // 方式1：使用工厂函数
    let db = connect("postgres://localhost/mydb").await?
    
    // 方式2：直接使用具体类型
    let pg = PostgresDB.connect("postgres://localhost/mydb").await?
    
    Ok(())
}
```

---

## 导入语法

### 基本导入

```paw
// 单个导入
import user.User
import database.Database

// 多个导入
import user.{User, UserService, validate}

// 全部导入
import user.*

// 别名导入
import database.Database as DB
import user.User as AppUser
```

### 路径

```paw
// 相对导入
import super.utils      // 父模块
import self.helper      // 当前模块的子模块

// 绝对导入
import std.collections.Vec
import myproject.api.routes
```

### 重新导出

```paw
// api/mod.paw
pub import v1.User           // 重新导出
pub import v1.endpoints.*    // 重新导出所有

// 用户可以这样导入
import api.User              // 实际来自 api.v1.User
```

---

## 标准库组织

```
std/
├── mod.paw           # 标准库入口
├── collections/
│   ├── mod.paw
│   ├── vec.paw
│   ├── hashmap.paw
│   └── hashset.paw
├── io/
│   ├── mod.paw
│   ├── read.paw
│   └── write.paw
├── net/
│   ├── mod.paw
│   ├── tcp.paw
│   └── http.paw
└── sync/
    ├── mod.paw
    ├── mutex.paw
    └── channel.paw
```

**使用标准库：**
```paw
import std.collections.Vec
import std.collections.HashMap
import std.io.{read_file, write_file}
import std.net.http.Server
```

---

## 实际项目示例

### 项目结构

```
myapp/
├── main.paw          # 入口
├── config.paw        # 配置模块
├── models/
│   ├── mod.paw
│   ├── user.paw
│   └── post.paw
├── api/
│   ├── mod.paw
│   ├── routes.paw
│   └── middleware.paw
├── database/
│   ├── mod.paw
│   └── postgres.paw
└── utils/
    ├── mod.paw
    ├── validation.paw
    └── crypto.paw
```

### main.paw

```paw
import config.Config
import api.create_server
import database.Database

fn main() async -> Result<(), Error> {
    let config = Config.load("config.toml")?
    let db = Database.connect(config.database_url).await?
    let server = create_server(db, config)?
    
    println("Server running on ${config.host}:${config.port}")
    server.run().await
}
```

### models/mod.paw

```paw
// 重新导出所有模型
pub import user.User
pub import post.Post

// 共享的 trait
pub type Model = trait {
    fn id(self) -> int
    fn created_at(self) -> DateTime
}
```

### models/user.paw

```paw
import super.Model
import utils.validation.validate_email

pub type User = struct {
    pub id: int
    pub name: string
    pub email: string
    created_at: DateTime
    
    pub fn new(name: string, email: string) -> Result<Self, String> {
        validate_email(email)?
        Ok(User {
            id: 0
            name
            email
            created_at: DateTime.now()
        })
    }
}

// 实现共享 trait
impl Model for User {
    fn id(self) -> int { self.id }
    fn created_at(self) -> DateTime { self.created_at }
}
```

### api/routes.paw

```paw
import super.middleware.log
import models.User
import database.Database

pub fn setup_routes(db: Database) -> Router {
    Router.new()
        .middleware(log)
        .get("/users", |req| get_users(req, &db))
        .post("/users", |req| create_user(req, &db))
}

fn get_users(req: Request, db: &Database) async -> Response {
    let users = db.query("SELECT * FROM users").await else {
        return Response.error(500, "Database error")
    }
    Response.json(users)
}

fn create_user(req: Request, db: &Database) async -> Response {
    let user = req.json::<User>().await else {
        return Response.error(400, "Invalid JSON")
    }
    // ...
}
```

---

## 关键字变化

### 更新后的 19 个关键字

```
声明 (2):   let, type
函数 (1):   fn
控制 (5):   if, else, loop, break, return
模式 (2):   is, as
异步 (2):   async, await
导入 (1):   import  ← 改变（use → import）
其他 (6):   pub, self, Self, mut, true, false
```

**移除 `mod`** - 模块由文件系统控制，减少到 **19 个关键字**！

---

## 导入模式对照

| 场景 | Rust | Paw |
|------|------|-----|
| 单个导入 | `use std::io::Read;` | `import std.io.Read` |
| 多个导入 | `use std::{io, fs};` | `import std.{io, fs}` |
| 全部导入 | `use std::io::*;` | `import std.io.*` |
| 别名 | `use std::io::Read as R;` | `import std.io.Read as R` |
| 重新导出 | `pub use user::User;` | `pub import user.User` |

---

## 模块可见性规则

### 1. 文件级私有

```paw
// user.paw

// 私有类型（仅本文件可见）
type InternalUser = struct {
    data: string
}

// 公开类型（其他模块可见）
pub type User = struct {
    pub name: string
}
```

### 2. 目录级组织

```paw
// api/mod.paw 控制整个 api 目录的导出

pub import routes.*       // 公开所有路由
pub import middleware.log // 仅公开 log 中间件

// 不导出 internal.paw 中的内容（保持私有）
```

### 3. 跨模块访问

```paw
// a.paw
pub type TypeA = struct { pub x: int }

// b.paw
import a.TypeA

pub type TypeB = struct {
    pub data: TypeA    // 使用其他模块的公开类型
}
```

---

## 最佳实践

### 1. 模块命名

```
小写下划线：
  user_service.paw
  http_client.paw
  string_utils.paw

对应的模块路径：
  import user_service.UserService
  import http_client.Client
  import string_utils.format
```

### 2. mod.paw 作为模块入口

```paw
// utils/mod.paw

// 重新导出所有公开 API
pub import string.*
pub import math.*
pub import validation.{validate_email, validate_phone}

// 提供统一的命名空间
pub let VERSION = "1.0.0"
```

### 3. 循环依赖避免

```paw
// ❌ 不好 - 循环依赖
// a.paw
import b.TypeB

// b.paw
import a.TypeA

// ✅ 好 - 提取共享类型
// types.paw
pub type SharedType = struct { }

// a.paw
import types.SharedType

// b.paw
import types.SharedType
```

---

## 完整示例

### 项目：Todo API

```
todo_api/
├── main.paw
├── config.paw
├── models/
│   ├── mod.paw
│   └── todo.paw
├── api/
│   ├── mod.paw
│   └── handlers.paw
└── db/
    ├── mod.paw
    └── postgres.paw
```

**main.paw:**
```paw
import config.Config
import api.create_app
import db.Database

fn main() async -> Result<(), Error> {
    let config = Config.from_file("config.toml")?
    let db = Database.connect(config.db_url).await?
    let app = create_app(db)
    
    app.listen(config.port).await
}
```

**models/todo.paw:**
```paw
pub type Todo = struct {
    pub id: int
    pub title: string
    pub completed: bool
    
    pub fn new(title: string) -> Self {
        Todo { id: 0, title, completed: false }
    }
}
```

**api/handlers.paw:**
```paw
import models.Todo
import db.Database

pub fn get_todos(db: &Database) async -> Response {
    let todos = db.query_all::<Todo>().await else {
        return Response.error(500, "DB error")
    }
    Response.json(todos)
}

pub fn create_todo(req: Request, db: &Database) async -> Response {
    let todo = req.json::<Todo>().await else {
        return Response.error(400, "Invalid JSON")
    }
    
    let id = db.insert(todo).await else {
        return Response.error(500, "Insert failed")
    }
    
    Response.created(id)
}
```

**api/mod.paw:**
```paw
import handlers.{get_todos, create_todo}
import db.Database

pub fn create_app(db: Database) -> App {
    App.new()
        .get("/todos", |req| get_todos(&db))
        .post("/todos", |req| create_todo(req, &db))
}
```

---

## 标准库导入

```paw
// 基础集合
import std.collections.{Vec, HashMap, HashSet}

// I/O
import std.io.{read_file, write_file}
import std.fs.{create_dir, remove_file}

// 网络
import std.net.http.{Server, Request, Response}
import std.net.tcp.TcpStream

// 并发
import std.sync.{Mutex, Arc, Channel}
import std.thread.spawn

// 时间
import std.time.{Duration, Instant, sleep}
```

---

## 与 Rust 的对比

| 特性 | Rust | Paw |
|------|------|-----|
| 文件即模块 | ✓ | ✓ |
| mod.rs/mod.paw | ✓ | ✓ |
| 导入关键字 | `use` | `import` |
| 路径分隔符 | `::` | `.` |
| 公开标记 | `pub` | `pub` |
| 重新导出 | `pub use` | `pub import` |
| 别名 | `use ... as` | `import ... as` |

**主要区别：**
- ✨ `import` 替代 `use`（更清晰）
- ✨ `.` 替代 `::`（更简洁）

---

## 总结

### 新的关键字列表（19个）

```
声明 (2):   let, type
函数 (1):   fn
控制 (5):   if, else, loop, break, return
模式 (2):   is, as
异步 (2):   async, await
导入 (1):   import  ← 改变
其他 (6):   pub, self, Self, mut, true, false
```

**移除：** `mod`, `use`  
**新增：** `import`  
**总计：** 19 个关键字（再减少 1 个！）⭐

### 模块系统优势

1. ✅ **文件即模块** - 直观的组织
2. ✅ **目录结构清晰** - 易于导航
3. ✅ **import 语义明确** - 比 use 更清楚
4. ✅ **与 Rust 兼容** - 相似的设计
5. ✅ **减少关键字** - 19 个而不是 20 个

**模块系统：简洁、直观、强大！** ✨

