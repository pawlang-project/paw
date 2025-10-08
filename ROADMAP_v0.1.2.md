# PawLang v0.1.2 开发计划

## 🎯 主要目标

实现完整的泛型方法系统

## 📋 功能列表

### 1. 泛型方法（P0 - 最高优先级）

#### 静态方法（关联函数）
```paw
type Vec<T> = struct {
    ptr: i32,
    len: i32,
    cap: i32,
    
    fn new() -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: 0 };
    }
    
    fn with_capacity(capacity: i32) -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: capacity };
    }
}

// 调用：
let vec = Vec<i32>::new();
```

#### 实例方法
```paw
type Box<T> = struct {
    value: T,
    
    fn get(self) -> T {
        return self.value;
    }
    
    fn set(self, new_value: T) {
        self.value = new_value;
    }
}

// 调用：
let box = Box { value: 42 };
let val = box.get();
```

### 2. 实现计划

#### 阶段 1：Parser 扩展（1 天）
- [x] 解析 `Type<T>::method()` 语法（已完成）
- [ ] 正确收集 struct 中的方法定义
- [ ] 区分静态方法和实例方法（是否有 self 参数）
- [ ] 测试验证

#### 阶段 2：Monomorphizer 扩展（0.5 天）
- [x] `GenericMethodInstance` 结构（已完成）
- [x] `recordMethodInstance` 方法（已完成）
- [ ] 收集所有方法调用
- [ ] 测试验证

#### 阶段 3：CodeGen 扩展（1 天）
- [x] 生成静态方法调用代码（基础完成）
- [ ] 生成方法的单态化版本
- [ ] 处理返回类型替换（Vec<T> -> Vec_i32）
- [ ] 处理 self 参数
- [ ] 测试验证

#### 阶段 4：标准库实现（0.5 天）
- [ ] `Vec<T>::new()`
- [ ] `Vec<T>::with_capacity(capacity: i32)`
- [ ] `Box<T>::new(value: T)`
- [ ] `Box<T>::get(self) -> T`
- [ ] 测试验证

#### 阶段 5：综合测试（0.5 天）
- [ ] 单元测试
- [ ] 集成测试
- [ ] 内存泄漏检查
- [ ] 性能测试
- [ ] 文档更新

### 3. 技术挑战

#### 挑战 1：方法定义解析
**问题**：Parser 需要区分字段和方法
**解决**：检查 `keyword_fn` token

#### 挑战 2：类型参数替换
**问题**：方法返回类型 `Vec<T>` 需要替换为 `Vec_i32`
**解决**：在单态化时替换所有泛型类型

#### 挑战 3：self 参数处理
**问题**：实例方法的 `self` 参数需要特殊处理
**解决**：生成时转换为指针类型

## 📊 预计时间

- **总计**：3-4 天
- **复杂度**：中等偏高
- **风险**：中等

## 🎯 成功标准

1. ✅ 静态方法调用正常工作
2. ✅ 实例方法调用正常工作
3. ✅ 类型参数正确替换
4. ✅ 零内存泄漏
5. ✅ 所有测试通过
6. ✅ 文档完善

## 🔮 v0.1.3 展望

- 泛型约束（trait bounds）
- 更复杂的标准库（HashMap, String）
- 性能优化
- 更多语法糖

