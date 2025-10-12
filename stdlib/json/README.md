# 🔧 JSON Module

**路径**: `stdlib/json/mod.paw`  
**版本**: v0.2.0  
**状态**: 🚧 开发中

---

## 📋 概述

JSON 模块提供 JSON 数据的解析和序列化功能。

### 当前状态

- ✅ JsonValue enum 定义
- ✅ Token enum 定义  
- ✅ Lexer 词法分析（部分）
- ✅ Parser 语法分析（基础）
- ⏳ 嵌套结构支持（待实现）

---

## 🎯 API 设计

### JsonValue 类型

```paw
pub type JsonValue = enum {
    Null,
    Bool(bool),
    Number(f64),
    String(string),
    // TODO: v0.3.0
    // Array(Vec<JsonValue>),
    // Object(HashMap<string, JsonValue>),
}
```

---

### 解析 API

```paw
// 解析 JSON 字符串
pub fn parse(json_str: string) -> JsonValue

// 示例:
let json = parse("42");
let result = json is {
    Number(n) => n,
    _ => 0,
};
```

**当前限制**:
- ⚠️ 仅支持基本类型（null, bool, number, string）
- ⚠️ 不支持数组 `[1, 2, 3]`
- ⚠️ 不支持对象 `{"key": "value"}`

---

### 序列化 API

```paw
// 序列化为 JSON 字符串
pub fn stringify(value: JsonValue) -> string

// 示例:
let value = JsonValue::Number(42.0);
let json_str = stringify(value);  // "42"
```

**当前限制**:
- ⚠️ 数字转字符串简化实现
- ⚠️ 字符串转义不完整

---

## 💡 使用示例

### 示例 1: 解析基础类型

```paw
import json;

fn test_parse() -> i32 {
    // 解析 null
    let null_val = json::parse("null");
    
    // 解析 bool
    let true_val = json::parse("true");
    let false_val = json::parse("false");
    
    // 解析数字
    let num = json::parse("42");
    
    // 解析字符串
    let str_val = json::parse("\"hello\"");
    
    return 0;
}
```

---

### 示例 2: 使用模式匹配

```paw
import json;

fn get_number(json_str: string) -> i32 {
    let value = json::parse(json_str);
    
    return value is {
        Number(n) => n as i32,
        Null => 0,
        Bool(b) => if b { 1 } else { 0 },
        String(s) => -1,
        _ => -99,
    };
}

fn main() -> i32 {
    let result = get_number("42");  // 42
    return result;
}
```

---

### 示例 3: 序列化

```paw
import json;

fn create_response() -> string {
    let value = JsonValue::Bool(true);
    return json::stringify(value);  // "true"
}
```

---

## 🔧 实现细节

### Lexer（词法分析）

**功能**: 将 JSON 字符串分解为 token

```paw
type Token = enum {
    LeftBrace,      // {
    RightBrace,     // }
    LeftBracket,    // [
    RightBracket,   // ]
    Colon,          // :
    Comma,          // ,
    StringToken(string),
    NumberToken(f64),
    TrueToken,
    FalseToken,
    NullToken,
    EOF,
    Error,
}
```

**状态**: ✅ 部分实现

---

### Parser（语法分析）

**功能**: 使用 **is 表达式**进行模式匹配

```paw
pub fn parse_value(mut self) -> JsonValue {
    let token: Token = self.current_token;
    
    return token is {
        NullToken => {
            self.advance();
            return JsonValue::Null;
        },
        NumberToken(num) => {
            self.advance();
            return JsonValue::Number(num);
        },
        // ...
        _ => JsonValue::Null,
    };
}
```

**状态**: ✅ 基础实现（简单类型）

---

## 🚧 当前限制

### 1. 不支持嵌套结构

```paw
// ❌ 当前不支持
parse("[1, 2, 3]")
parse("{\"name\": \"Alice\"}")
parse("{\"data\": [1, 2, 3]}")
```

**原因**:
- 需要动态 Vec<JsonValue>
- 需要 HashMap<string, JsonValue>
- 等待 FFI 支持

---

### 2. 字符串处理简化

```paw
// ⚠️ 转义处理不完整
parse("\"hello\\nworld\"")  // 可能不正确
```

**原因**:
- 需要完整的转义序列处理
- 需要 StringBuilder 支持

---

### 3. 数字转字符串

```paw
// ⚠️ 临时实现
stringify(JsonValue::Number(3.14))  // "42" (错误)
```

**原因**:
- 需要 f64 转 string 函数
- 当前使用 i32 转换

---

## 🔮 v0.3.0 计划

### 完整的 JSON 解析器

**功能**:
```paw
// ✅ 嵌套数组
let arr = parse("[1, 2, [3, 4]]");

// ✅ 嵌套对象
let obj = parse("{\"user\": {\"name\": \"Alice\"}}");

// ✅ 混合结构
let data = parse("{\"items\": [1, 2, 3], \"count\": 3}");
```

**需要**:
- Vec<JsonValue> 动态数组
- HashMap<string, JsonValue> 键值对
- 递归解析

---

### 完整的转义处理

```paw
// ✅ 所有转义序列
parse("\"Line 1\\nLine 2\"")
parse("\"Tab\\there\"")
parse("\"Quote: \\\"Hello\\\"\"")
parse("\"Unicode: \\u4F60\\u597D\"")  // 未来
```

---

### 完整的数字支持

```paw
// ✅ 各种数字格式
parse("42")           // 整数
parse("3.14")         // 小数
parse("-17")          // 负数
parse("1.5e10")       // 科学计数法
parse("1.23e-4")      // 负指数
```

---

## 📚 使用场景

### 场景 1: 配置文件

```paw
import json;
import fs;

fn load_config() -> JsonValue {
    if fs::exists("config.json") {
        let content = fs::read_file("config.json");
        return json::parse(content);
    }
    return JsonValue::Null;
}
```

---

### 场景 2: API 响应

```paw
import json;

fn create_response(success: bool, data: i32) -> string {
    // v0.2.0: 简化版本
    let value = JsonValue::Bool(success);
    return json::stringify(value);
    
    // v0.3.0: 完整版本
    // let response = JsonValue::Object({
    //     "success": JsonValue::Bool(success),
    //     "data": JsonValue::Number(data),
    // });
    // return json::stringify(response);
}
```

---

### 场景 3: 数据持久化

```paw
import json;
import fs;

fn save_data(data: JsonValue) -> bool {
    let json_str = json::stringify(data);
    return fs::write_file("data.json", json_str);
}
```

---

## 🔧 实现进度

| 功能 | 状态 | 完成度 |
|------|------|--------|
| JsonValue 定义 | ✅ | 100% |
| Token 定义 | ✅ | 100% |
| Lexer 基础 | ✅ | 70% |
| Parser 基础 | ✅ | 60% |
| Stringify 基础 | ✅ | 50% |
| 嵌套结构 | ⏳ | 0% |
| 完整转义 | ⏳ | 30% |
| 数字转换 | ⏳ | 40% |

**总进度**: 🚧 **55%**

---

## ✅ 总结

### 当前可用

- ✅ 基础类型解析（null, bool, number, string）
- ✅ is 表达式模式匹配
- ✅ 简单的序列化

### 等待实现

- ⏳ 嵌套数组和对象
- ⏳ 完整的转义处理
- ⏳ 精确的数字转换

### 何时可用

- **v0.2.0**: 基础类型可用（当前）
- **v0.3.0**: 完整实现（FFI 后）

---

**维护者**: PawLang 核心开发团队  
**最后更新**: 2025-10-12  
**版本**: v0.2.0  
**许可**: MIT

