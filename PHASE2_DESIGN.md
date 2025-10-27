# Phase 2: 类型系统独立化 - 设计文档

> 📐 **设计版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 将类型系统从 AST 中独立出来，建立模块化的类型系统

---

## 1. 背景与动机

### 1.1 当前问题

**类型定义分散**：
- 类型节点定义在 `ast.h` 中（作为 AST 的一部分）
- 类型比较逻辑在 `codegen_stmt.cpp` 中
- 类型操作分散在多个文件

**维护困难**：
- 添加新类型需要修改多处
- 类型操作逻辑不统一
- 难以实现高级类型特性（如类型推导、子类型关系）

**架构不清晰**：
- 类型系统缺乏独立的抽象
- Parser、CodeGen 直接操作类型节点
- 没有统一的类型管理接口

### 1.2 目标

**建立独立的类型系统模块**：
- 类型定义集中在 `src/types/` 目录
- 提供统一的 `TypeSystem` 接口
- 类型操作（比较、转换、推导）模块化

**改善架构**：
- 单一职责原则（类型系统只负责类型）
- 依赖倒置（Parser/CodeGen 依赖抽象接口）
- 易于扩展（添加新类型只需扩展类层次）

---

## 2. 当前类型系统分析

### 2.1 类型层次

```
Type (基类)
├── PrimitiveTypeNode    - i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, f32, f64, bool, char, string, void
├── NamedTypeNode        - 用户定义类型（struct, enum, interface）
├── GenericTypeNode      - 泛型参数 T, U, V
├── SelfTypeNode         - Self 类型（在方法中）
├── OptionalTypeNode     - T? 可选类型
├── ArrayTypeNode        - [T; N] 固定大小数组
├── SliceTypeNode        - [T] 动态切片
├── TupleTypeNode        - (T, U, V) 元组
├── ReferenceTypeNode    - &T, &mut T 引用
└── FunctionTypeNode     - fn(T1, T2) -> T3 函数类型
```

### 2.2 类型操作

**当前位置**：
- `CodeGenerator::compareTypes()` - 在 `codegen_stmt.cpp` 中（~200 行）
- `CodeGenerator::typeToString()` - 在 `codegen_stmt.cpp` 中
- 类型转换逻辑分散在 `codegen_type.cpp` 中

**功能**：
- 类型相等性比较（递归比较所有类型节点）
- 类型字符串表示（用于错误信息）
- 类型到 LLVM Type 的转换

---

## 3. 新设计架构

### 3.1 目录结构

```
src/types/
├── type.h                  # Type 基类和子类定义
├── type_system.h           # TypeSystem 管理器
├── type_system.cpp         # TypeSystem 实现
├── type_registry.h         # 类型注册表（可选，未来扩展）
└── type_inference.h        # 类型推导（可选，未来扩展）
```

### 3.2 核心类设计

#### 3.2.1 Type 基类

```cpp
// src/types/type.h

namespace pawc {

/**
 * 类型基类
 */
class Type {
public:
    enum class Kind {
        Primitive,   // i32, f64, bool等
        Named,       // 用户定义类型
        Array,       // [T; N]
        Slice,       // [T]
        Tuple,       // (T, U, V)
        Reference,   // &T, &mut T
        Function,    // fn(T1, T2) -> T3
        Generic,     // T, U等
        SelfType,    // Self
        Optional     // T?
    };
    
    virtual ~Type() = default;
    
    // 获取类型种类
    virtual Kind getKind() const = 0;
    
    // 类型相等性比较
    virtual bool equals(const Type* other) const = 0;
    
    // 获取类型字符串表示
    virtual std::string toString() const = 0;
    
    // 克隆类型
    virtual std::unique_ptr<Type> clone() const = 0;
    
protected:
    Type() = default;
};

/**
 * 原始类型
 */
class PrimitiveType : public Type {
public:
    enum class Primitive {
        I8, I16, I32, I64, I128,
        U8, U16, U32, U64, U128,
        F32, F64,
        Bool, Char, String, Void
    };
    
    explicit PrimitiveType(Primitive prim);
    
    Kind getKind() const override { return Kind::Primitive; }
    bool equals(const Type* other) const override;
    std::string toString() const override;
    std::unique_ptr<Type> clone() const override;
    
    Primitive getPrimitive() const { return primitive_; }
    
private:
    Primitive primitive_;
};

/**
 * 命名类型（用户定义）
 */
class NamedType : public Type {
public:
    NamedType(std::string name, std::vector<std::unique_ptr<Type>> generic_args);
    
    Kind getKind() const override { return Kind::Named; }
    bool equals(const Type* other) const override;
    std::string toString() const override;
    std::unique_ptr<Type> clone() const override;
    
    const std::string& getName() const { return name_; }
    const std::vector<std::unique_ptr<Type>>& getGenericArgs() const { return generic_args_; }
    
private:
    std::string name_;
    std::vector<std::unique_ptr<Type>> generic_args_;
};

// ... 其他类型类似 ...

} // namespace pawc
```

#### 3.2.2 TypeSystem 管理器

```cpp
// src/types/type_system.h

namespace pawc {

/**
 * 类型系统管理器
 */
class TypeSystem {
public:
    TypeSystem();
    
    // === 类型创建 ===
    
    /**
     * 获取原始类型
     */
    Type* getPrimitiveType(PrimitiveType::Primitive prim);
    
    /**
     * 创建命名类型
     */
    Type* getNamedType(const std::string& name, 
                       std::vector<std::unique_ptr<Type>> generic_args = {});
    
    /**
     * 创建数组类型
     */
    Type* getArrayType(Type* element, int size);
    
    /**
     * 创建切片类型
     */
    Type* getSliceType(Type* element);
    
    /**
     * 创建引用类型
     */
    Type* getReferenceType(Type* pointee, bool is_mut);
    
    /**
     * 创建可选类型
     */
    Type* getOptionalType(Type* inner);
    
    /**
     * 创建元组类型
     */
    Type* getTupleType(std::vector<std::unique_ptr<Type>> elements);
    
    // === 类型查询 ===
    
    /**
     * 类型相等性比较
     */
    bool equals(const Type* a, const Type* b);
    
    /**
     * 类型是否可赋值（a 可以赋值给 b）
     */
    bool isAssignable(const Type* from, const Type* to);
    
    /**
     * 获取两个类型的公共类型（如果存在）
     */
    Type* commonType(const Type* a, const Type* b);
    
    /**
     * 是否是子类型关系
     */
    bool isSubtype(const Type* sub, const Type* super);
    
    // === 类型注册 ===
    
    /**
     * 注册用户定义类型
     */
    void registerType(const std::string& name, Type* type);
    
    /**
     * 查找类型
     */
    Type* lookupType(const std::string& name);
    
private:
    // 原始类型缓存
    std::map<PrimitiveType::Primitive, std::unique_ptr<PrimitiveType>> primitive_types_;
    
    // 类型池（管理所有创建的类型）
    std::vector<std::unique_ptr<Type>> type_pool_;
    
    // 命名类型注册表
    std::map<std::string, Type*> named_types_;
    
    // 辅助方法
    Type* internType(std::unique_ptr<Type> type);
};

} // namespace pawc
```

### 3.3 与现有 AST 的关系

**保留 AST 中的类型节点**（短期）：
- `Type` 及其子类在 `ast.h` 中保留
- 作为 Parser 的输出
- 逐步替换为新的类型系统

**迁移策略**：
1. 新类型系统与旧类型节点并存
2. 添加转换函数：`Type* fromASTType(const ast::Type*)`
3. CodeGen 使用新类型系统
4. 最终移除 AST 中的类型定义

---

## 4. 迁移计划

### 4.1 阶段 1：创建新类型系统（不破坏现有代码）

**任务**：
- [x] 创建 `src/types/` 目录
- [ ] 实现 `Type` 基类和所有子类
- [ ] 实现 `TypeSystem` 管理器
- [ ] 实现类型比较逻辑
- [ ] 添加单元测试（可选）

**时间**：1-2 小时

**验证**：
- 编译通过
- 不影响现有功能

### 4.2 阶段 2：集成到 CodeGen（逐步替换）

**任务**：
- [ ] 在 `CodeGenerator` 中添加 `TypeSystem` 实例
- [ ] 添加 `Type* fromASTType(const ast::Type*)` 转换函数
- [ ] 修改 `compareTypes()` 使用新类型系统
- [ ] 修改 `typeToString()` 使用新类型系统
- [ ] 测试验证

**时间**：1 小时

**验证**：
- 所有测试通过
- 类型比较正常工作

### 4.3 阶段 3：扩展到 Parser 和 SymbolTable（可选，未来）

**任务**：
- [ ] Parser 直接使用 `TypeSystem`
- [ ] SymbolTable 存储 `Type*` 而不是 `ast::Type*`
- [ ] 移除 AST 中的类型节点
- [ ] 完全迁移完成

**时间**：2-3 小时（可分多次完成）

**验证**：
- 所有功能正常
- 代码更简洁

---

## 5. 风险评估

### 5.1 风险

| 风险 | 等级 | 影响 | 缓解措施 |
|-----|------|------|---------|
| 破坏现有类型比较 | 中 | 编译器错误判断 | 充分测试，逐步迁移 |
| 增加编译时间 | 低 | 性能下降 | 使用类型缓存 |
| 内存泄漏 | 低 | 资源问题 | 使用智能指针 |
| 迁移时间过长 | 中 | 影响其他开发 | 分阶段进行 |

### 5.2 回滚策略

- 每个阶段单独提交到 Git
- 如果出现问题，可以回滚到上一个阶段
- 保持新旧系统并存，直到新系统完全稳定

---

## 6. 测试策略

### 6.1 单元测试（可选）

```cpp
// 测试类型相等性
TEST(TypeSystem, Equality) {
    TypeSystem ts;
    auto i32_1 = ts.getPrimitiveType(PrimitiveType::I32);
    auto i32_2 = ts.getPrimitiveType(PrimitiveType::I32);
    ASSERT_TRUE(ts.equals(i32_1, i32_2));
}

// 测试类型创建
TEST(TypeSystem, ArrayType) {
    TypeSystem ts;
    auto i32 = ts.getPrimitiveType(PrimitiveType::I32);
    auto arr = ts.getArrayType(i32, 10);
    ASSERT_EQ(arr->toString(), "[i32; 10]");
}
```

### 6.2 集成测试

**使用现有 examples**：
- 编译所有 `examples/*.paw`
- 确保输出正确
- 确保错误信息正确

---

## 7. 实施顺序

### 今天（Phase 2.1）

1. ✅ 创建设计文档
2. 创建 `src/types/` 目录
3. 实现 `Type` 基类和子类
4. 实现 `TypeSystem` 管理器
5. 更新 `CMakeLists.txt`
6. 编译测试
7. **提交到 Git**

### 下次（Phase 2.2）

1. 集成到 `CodeGen`
2. 测试验证
3. 提交到 Git

### 未来（Phase 2.3）

1. 扩展到 Parser
2. 移除旧类型定义
3. 完成 Phase 2

---

## 8. 参考资料

### 8.1 类似项目

- **Rust 编译器**：ty 模块
- **Clang**：QualType 系统
- **Swift**：Type 系统

### 8.2 设计模式

- **Visitor 模式**：类型遍历
- **Factory 模式**：类型创建
- **Flyweight 模式**：类型缓存（原始类型）

---

## 9. 附录：代码统计

### 9.1 预计代码量

| 文件 | 行数 | 说明 |
|-----|------|------|
| `type.h` | ~400 | Type 基类 + 10 个子类 |
| `type_system.h` | ~100 | TypeSystem 声明 |
| `type_system.cpp` | ~300 | TypeSystem 实现 |
| 集成代码 | ~100 | CodeGen 集成 |
| **总计** | **~900** | 新增代码 |

### 9.2 删除代码

- AST 类型定义：~200 行（阶段 3）
- CodeGen 类型比较：~200 行（阶段 2）

**净增长**：~500 行

---

## 10. 总结

Phase 2 是一个**中型重构**，将为 PawLang 建立坚实的类型系统基础。

**关键原则**：
- ✅ 增量式迁移（不破坏现有功能）
- ✅ 充分测试
- ✅ 每阶段单独提交
- ✅ 保持向后兼容

**预期收益**：
- 更清晰的架构
- 更易于扩展
- 为高级特性打基础

---

**🐾 设计文档完成！准备开始实施！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

