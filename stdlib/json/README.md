# JSON 解析器模块

PawLang v0.2.0 内置的 JSON 解析和序列化模块。

## 📦 模块状态

**版本**: v0.2.0  
**状态**: ✅ 基础功能完成  
**支持的JSON类型**: Null, Boolean, Number, String  
**待完善**: 嵌套对象和数组（需要动态Vec<T>支持）

## 🚀 快速开始

### 导入模块

```paw
import json.{parse, stringify, JsonValue};
```

### 基本用法

#### 1. 解析 JSON

```paw
// 解析 null
let json_str = "null";
let value = parse(json_str);

// 解析布尔值
let json_bool = "true";
let value = parse(json_bool);

// 解析数字
let json_num = "42";
let value = parse(json_num);

// 解析负数
let json_neg = "-123";
let value = parse(json_neg);

// 解析浮点数
let json_float = "3.14";
let value = parse(json_float);

// 解析字符串
let json_str = "\"hello\"";
let value = parse(json_str);
```

#### 2. 处理解析结果

使用 `is` 表达式进行模式匹配：

```paw
let result = parse("42");

let value = result is {
    Null => {
        println("Got null");
        return 0;
    },
    Bool(b) => {
        if b {
            println("Got true");
        } else {
            println("Got false");
        }
        return 1;
    },
    Number(n) => {
        let int_val = n as i32;
        println("Got number");
        return int_val;
    },
    String(s) => {
        println("Got string");
        return 1;
    },
    _ => {
        println("Unknown type");
        return -1;
    },
};
```

#### 3. 序列化为 JSON

```paw
// 创建 JSON 值
let null_val = JsonValue::Null;
let bool_val = JsonValue::Bool(true);
let num_val = JsonValue::Number(42.0);
let str_val = JsonValue::String("hello");

// 转换为字符串
let json_str = stringify(num_val);
```

## 📚 API 参考

### 类型定义

#### `JsonValue` (enum)

表示 JSON 值的枚举类型：

```paw
pub type JsonValue = enum {
    Null,
    Bool(bool),
    Number(f64),
    String(string),
}
```

**变体说明**：
- `Null`: JSON null 值
- `Bool(bool)`: JSON 布尔值（true/false）
- `Number(f64)`: JSON 数字（整数和浮点数）
- `String(string)`: JSON 字符串

### 公共函数

#### `parse(json_str: string) -> JsonValue`

解析 JSON 字符串。

**参数**：
- `json_str`: 要解析的 JSON 字符串

**返回**：
- `JsonValue`: 解析后的 JSON 值

**示例**：
```paw
let value = parse("42");
let value2 = parse("true");
let value3 = parse("\"hello\"");
```

#### `stringify(value: JsonValue) -> string`

将 JSON 值序列化为字符串。

**参数**：
- `value`: 要序列化的 JSON 值

**返回**：
- `string`: JSON 字符串表示

**注意**: 当前版本由于 StringBuilder 限制，返回简化的字符串表示。

**示例**：
```paw
let value = JsonValue::Number(42.0);
let json_str = stringify(value);
```

#### `is_valid(json_str: string) -> bool`

验证 JSON 字符串格式（待实现）。

#### `get_field(obj: JsonValue, key: string) -> JsonValue`

从 JSON 对象获取字段（待实现）。

#### `get_index(arr: JsonValue, index: i32) -> JsonValue`

从 JSON 数组获取元素（待实现）。

## 🎯 功能特性

### ✅ 已实现

- ✅ **词法分析**：完整的 JSON Token 识别
- ✅ **语法解析**：递归下降解析器
- ✅ **Null 解析**：`null` 关键字
- ✅ **布尔解析**：`true`/`false` 关键字
- ✅ **数字解析**：整数和浮点数（包括负数）
- ✅ **字符串解析**：带引号的字符串（基本转义支持）
- ✅ **序列化**：基本的 JSON 值转字符串
- ✅ **转义处理**：`\n`, `\r`, `\t`, `\"`, `\\`

### 🚧 待实现

- ⏳ **嵌套对象**：`{"key": "value"}` （需要 HashMap）
- ⏳ **嵌套数组**：`[1, 2, 3]` （需要动态 Vec<T>）
- ⏳ **Unicode 转义**：`\uXXXX`
- ⏳ **科学计数法**：`1.23e10`
- ⏳ **完整字符串切片**：动态子字符串提取

## 📝 示例程序

### 完整示例

```paw
import json.{parse, stringify, JsonValue};

fn main() -> i32 {
    // 解析不同类型的 JSON
    let null_val = parse("null");
    let bool_val = parse("true");
    let num_val = parse("42");
    let float_val = parse("3.14");
    let str_val = parse("\"hello\"");
    
    // 使用模式匹配处理
    let result = num_val is {
        Number(n) => {
            let i = n as i32;
            println("Number: 42");
            return i;
        },
        _ => 0,
    };
    
    // 序列化
    let value = JsonValue::Number(123.0);
    let json_str = stringify(value);
    
    return 0;
}
```

### 测试文件

查看 `tests/json/test_json_complete.paw` 获取完整的测试套件。

查看 `examples/json_demo_v2.paw` 获取交互式演示。

## 🔧 技术实现

### 架构设计

```
JSON 字符串
    ↓
Lexer (词法分析)
    ↓
Token 流
    ↓
Parser (语法分析)
    ↓
JsonValue (AST)
    ↓
stringify (序列化)
    ↓
JSON 字符串
```

### 核心组件

1. **Lexer**: 将 JSON 字符串分解为 Token
   - 支持空白符跳过
   - 数字解析（整数/浮点数/负数）
   - 字符串解析（带转义）
   - 关键字识别（null/true/false）

2. **Parser**: 递归下降解析器
   - `parse_value()`: 解析任意 JSON 值
   - 使用 `is` 表达式进行类型分发

3. **Stringify**: 序列化器
   - 使用 `StringBuilder` 构建字符串
   - 处理字符转义
   - f64 到字符串转换

### 限制和权衡

**当前限制**：
1. **动态字符串**: PawLang 暂不支持动态字符串分配，字符串解析使用固定缓冲区
2. **Vec<T>**: 动态数组功能有限，暂不支持嵌套结构
3. **StringBuilder**: 无法转换回 string，序列化功能简化

**设计原则**：
- 纯 PawLang 实现，不依赖外部库
- 使用固定缓冲区避免动态内存
- 渐进式设计，为未来扩展预留空间

## 🧪 测试

### 运行测试

```bash
# 运行完整测试套件
pawc tests/json/test_json_complete.paw --run

# 运行演示程序
pawc examples/json_demo_v2.paw --run
```

### 测试覆盖

- ✅ Null 解析
- ✅ 布尔值解析（true/false）
- ✅ 整数解析
- ✅ 负数解析
- ✅ 浮点数解析
- ✅ 字符串解析
- ✅ Stringify 基础功能
- ✅ 往返测试（parse → stringify）

## 🗺️ 未来路线图

### v0.2.1（计划中）

- [ ] 完整的字符串切片支持
- [ ] StringBuilder.to_string() 方法
- [ ] Unicode 转义序列
- [ ] 科学计数法支持

### v0.3.0（计划中）

- [ ] JSON 对象支持（需要 HashMap<K,V>）
- [ ] JSON 数组支持（需要动态 Vec<T>）
- [ ] 嵌套结构解析
- [ ] JSON Schema 验证

### v0.4.0（计划中）

- [ ] JSON Path 查询
- [ ] 流式解析（大文件）
- [ ] 格式化输出（pretty print）
- [ ] 错误位置报告

## 📖 相关文档

- [PawLang 快速开始](../../docs/QUICKSTART.md)
- [标准库文档](../README.md)
- [字符串模块](../string/README.md)
- [Vec<T> 文档](../collections/README.md)

## 🤝 贡献

欢迎贡献！如果你想改进 JSON 模块：

1. 报告 Bug：通过 GitHub Issues
2. 功能建议：描述使用场景
3. 代码贡献：Fork → 开发 → Pull Request

## 📄 许可证

MIT License - 与 PawLang 主项目相同

---

**Built with ❤️ for PawLang v0.2.0** 🐾
