# PawLang 标准库

PawLang的标准库采用分层模块化设计，提供丰富的功能和清晰的组织结构。

## 📦 标准库结构

```
std/
├── collections/           # 集合类型和操作
│   ├── collections.paw   # 数组和切片的统一操作库
│   └── types.paw         # 泛型数据结构（Pair, Triple, Range, Box）
├── string.paw            # 字符串操作
├── math.paw              # 数学函数和常量
└── mem.paw               # 内存管理
```

---

## 🔧 使用方式

### 导入模块

```paw
// 导入集合操作
import std::collections::collections;

// 导入泛型类型
import std::collections::types;

// 导入字符串工具
import std::string;

// 导入数学工具
import std::math;

// 导入内存管理
import std::mem;
```

---

## 📚 模块详情

### 1️⃣ std::collections::collections

**统一的数组和切片操作库 - 完全泛型设计**

#### 核心函数（支持所有类型 `T`）

**聚合操作**
- `sum<T>(items: [T]) -> T` - 计算和
- `product<T>(items: [T]) -> T` - 计算乘积
- `average<T>(items: [T]) -> T` - 计算平均值
- `abs_sum<T>(items: [T]) -> T` - 绝对值和

**查找操作**
- `max_value<T>(items: [T]) -> T` - 查找最大值
- `min_value<T>(items: [T]) -> T` - 查找最小值
- `first<T>(items: [T]) -> T` - 获取第一个元素
- `last<T>(items: [T]) -> T` - 获取最后一个元素
- `range<T>(items: [T]) -> T` - 最大最小值范围

**搜索操作**
- `contains<T>(items: [T], value: T) -> bool` - 检查是否包含
- `index_of<T>(items: [T], value: T) -> i64` - 查找索引（-1表示未找到）
- `count<T>(items: [T], value: T) -> i64` - 统计出现次数

**谓词操作**
- `all_positive<T>(items: [T]) -> bool` - 检查全为正数
- `any_negative<T>(items: [T]) -> bool` - 检查有无负数

**辅助函数**
- `size<T>(items: [T]) -> i64` - 获取大小（等同于`len`）
- `empty<T>(items: [T]) -> bool` - 检查是否为空（等同于`is_empty`）
- `minmax<T>(items: [T]) -> T` - 范围（等同于`range`）

**示例**：
```paw
import std::collections::collections;

fn main() -> i32 {
    // 整数数组
    let nums: [i32; 5] = [1, 2, 3, 4, 5];
    let total: i32 = sum<i32>(nums);          // 15
    let avg: i32 = average<i32>(nums);        // 3
    let max: i32 = max_value<i32>(nums);      // 5
    
    // 浮点数组
    let floats: [f64; 3] = [1.5, 2.5, 3.5];
    let sum_f: f64 = sum<f64>(floats);        // 7.5
    let avg_f: f64 = average<f64>(floats);    // 2.5
    
    // 查找操作
    let has_3: bool = contains<i32>(nums, 3); // true
    let idx: i64 = index_of<i32>(nums, 4);    // 3
    
    // 谓词操作
    let all_pos: bool = all_positive<i32>(nums);  // true
    
    return 0;
}
```

---

### 2️⃣ std::collections::types

**泛型数据结构**

**注意**：对于简单的键值对，推荐使用内置元组类型：
```paw
// 使用元组（推荐）
let pair: (i32, string) = (42, "hello");
let triple: (i32, f64, bool) = (100, 3.14, true);
```

#### Range<T> - 范围
```paw
let r: Range<i32> = Range::new(0, 10);
let in_range: bool = r.contains(5);  // true
```

#### Box<T> - 值包装器
```paw
let b: Box<i32> = Box::new(42);
let value: i32 = b.get();
```

---

### 3️⃣ std::string

**字符串操作工具**

- `trim(s: string) -> string` - 去除首尾空白
- `is_alpha(c: char) -> bool` - 检查字母
- `is_digit(c: char) -> bool` - 检查数字
- `is_whitespace(c: char) -> bool` - 检查空白字符
- `to_upper(c: char) -> char` - 转大写
- `to_lower(c: char) -> char` - 转小写

**示例**：
```paw
import std::string;

let s: string = "  hello  ";
let trimmed: string = trim(s);  // "hello"
```

---

### 4️⃣ std::math

**数学函数和常量**

#### 常量
- `pi: f64` - 圆周率 (3.14159265358979323846)
- `e: f64` - 自然对数底 (2.71828182845904523536)

#### 几何函数
- `distance(x1: f64, y1: f64, x2: f64, y2: f64) -> f64` - 两点距离
- `circle_area(radius: f64) -> f64` - 圆面积
- `circle_circumference(radius: f64) -> f64` - 圆周长

#### 角度转换
- `deg_to_rad(degrees: f64) -> f64` - 角度转弧度
- `rad_to_deg(radians: f64) -> f64` - 弧度转角度

#### 实用函数
- `approx_eq(a: f64, b: f64, epsilon: f64) -> bool` - 近似相等
- `lerp(a: f64, b: f64, t: f64) -> f64` - 线性插值

#### 整数工具
- `is_even(n: i32) -> bool` - 偶数判断
- `is_odd(n: i32) -> bool` - 奇数判断
- `powi(base: i32, exp: i32) -> i32` - 整数幂
- `gcd(a: i32, b: i32) -> i32` - 最大公约数
- `lcm(a: i32, b: i32) -> i32` - 最小公倍数
- `factorial(n: i32) -> i32` - 阶乘

#### 范围检查
- `in_range(value: f64, min: f64, max: f64) -> bool` - 范围检查

---

### 5️⃣ std::mem

**内存管理**

#### C库函数封装
- `malloc(size: i64) -> ptr` - 分配内存
- `calloc(count: i64, size: i64) -> ptr` - 分配并清零
- `realloc(ptr: ptr, new_size: i64) -> ptr` - 重新分配
- `free(ptr: ptr)` - 释放内存
- `memset(ptr: ptr, value: i32, size: i64) -> ptr` - 内存填充
- `memcpy(dest: ptr, src: ptr, size: i64) -> ptr` - 内存拷贝

---

## 🎯 内置函数

PawLang提供了20个内置函数，无需导入即可直接使用：

### 输入输出
- `print(...)` - 打印（不换行）
- `println(...)` - 打印（换行）
- `eprint(...)` - 错误输出（不换行）
- `eprintln(...)` - 错误输出（换行）
- `debug(value)` - 调试输出

### 错误处理
- `panic(msg: string)` - 程序崩溃
- `assert(condition: bool, msg: string)` - 断言
- `unreachable(msg: string)` - 不可达代码
- `todo(msg: string)` - 待实现
- `unimplemented(msg: string)` - 未实现

### 集合操作
- `len(arr/slice/string)` - 获取长度
- `is_empty(arr/slice/string)` - 检查是否为空

### 数学工具
- `abs(x)` - 绝对值
- `min(a, b)` - 最小值
- `max(a, b)` - 最大值
- `clamp(val, min, max)` - 限制范围
- `pow(base, exp)` - 幂运算
- `sqrt(x)` - 平方根
- `floor(x)` - 向下取整
- `ceil(x)` - 向上取整
- `round(x)` - 四舍五入

---

## ✨ 设计理念

### 分层明确
- **顶层**：通用功能（string, math, mem）
- **子层**：集合相关（collections/）

### 统一接口
- 数组和切片共享同一套操作函数
- 泛型优先，专用函数作为便利包装

### 类型安全
- 返回 `T?` 处理可能失败的操作
- 编译时类型检查

### 零成本抽象
- 内置函数编译时优化
- 泛型单态化

---

## 📖 示例

### 综合示例

```paw
import std::collections::collections;
import std::collections::types;
import std::math;

fn main() -> i32 {
    // 使用完全泛型的集合操作
    let nums: [i32; 5] = [1, 2, 3, 4, 5];
    
    // 所有函数都是泛型的
    let total: i32 = sum<i32>(nums);            // 15
    let avg: i32 = average<i32>(nums);          // 3
    let max: i32 = max_value<i32>(nums);        // 5
    let has_3: bool = contains<i32>(nums, 3);   // true
    
    println("总和: ");
    debug(total);
    println("平均值: ");
    debug(avg);
    
    // 浮点数也能用同样的泛型函数
    let floats: [f64; 3] = [1.5, 2.5, 3.5];
    let sum_f: f64 = sum<f64>(floats);          // 7.5
    
    // 使用内置元组类型（推荐）
    let pair: (i32, string) = (42, "answer");
    let triple: (i32, f64, bool) = (100, 3.14, true);
    
    // 使用数学函数
    let area: f64 = circle_area(5.0);
    
    return 0;
}
```

---

**PawLang v0.2.1** - 优雅、强大、易用的标准库 ✨

