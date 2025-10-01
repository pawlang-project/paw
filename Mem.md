* 在 runtime（Rust）实现：retain/release/make_unique, arena_alloc/reset，对象头与常量池。

* 在 codegen：在定义/覆盖/作用域/return插入 ARC；实现 peephole + last-use 消除。

* Dev/Release 配置：Dev 开断言，Release panic=abort、opt=speed。

* 标准库：String/Vec 统一 CoW & 预分配路径；提供 weak<T>；并发通道。

* V1.1：函数内逃逸分析→ 栈/arena 替代堆；跨线程原子 RC；更多 ARC contraction。

* 工具：pawc bench 与火焰图；可选 ASan 集成。