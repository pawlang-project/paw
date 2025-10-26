#ifndef PAWC_BUILTINS_H
#define PAWC_BUILTINS_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <map>
#include <unordered_map>

namespace pawc {

// 内置函数管理器
class Builtins {
public:
    Builtins(llvm::LLVMContext& context, llvm::Module& module);
    
    // 声明所有内置函数
    void declareAll();
    
    // 获取内置函数
    llvm::Function* getFunction(const std::string& name);
    
    // 检查是否是内置函数
    bool isBuiltin(const std::string& name) const;
    
    // 检查是否是编译器intrinsic（需要特殊处理）
    bool isIntrinsic(const std::string& name) const;
    
private:
    llvm::LLVMContext& context_;
    llvm::Module& module_;
    std::unordered_map<std::string, llvm::Function*> builtins_;
    
    // 声明各个内置函数
    void declarePrintf();   // 声明libc的printf
    void declareStrcat();   // 声明libc的strcat
    void declareStrcpy();   // 声明libc的strcpy
    void declareStrlen();   // 声明libc的strlen
    void declareMalloc();   // 声明libc的malloc
    void declareMemcpy();   // 声明libc的memcpy
    void declarePrint();
    void declarePrintln();
    void declareEprint();
    void declareEprintln();
    
    // 错误处理和调试函数
    void declarePanic();         // panic(message) - 终止程序
    void declareAssert();        // assert(condition, message) - 断言
    void declareUnreachable();   // unreachable(message) - 标记不可达代码
    void declareTodo();          // todo(message) - 标记待实现代码
    void declareUnimplemented(); // unimplemented(message) - 标记未实现功能
    
    // 编译器intrinsics（标记为builtin，但在codegen中内联展开）
    void declareLen();      // len() - 获取数组/切片/字符串长度
    void declareIsEmpty();  // is_empty() - 检查是否为空
    void declareDebug();    // debug(value) - 调试输出
    void declareAbs();      // abs(x) - 绝对值
    void declareMin();      // min(a, b) - 最小值
    void declareMax();      // max(a, b) - 最大值
    void declareClamp();    // clamp(val, min, max) - 范围限制
    void declarePow();      // pow(base, exp) - 幂运算
    void declareSqrt();     // sqrt(x) - 平方根
    void declareFloor();    // floor(x) - 向下取整
    void declareCeil();     // ceil(x) - 向上取整
    void declareRound();    // round(x) - 四舍五入
    
    // 辅助函数：创建函数类型
    llvm::FunctionType* createPrintFunctionType();
    
    // 辅助函数：检查是否是切片类型
    bool isSliceType(llvm::Type* type) const;

public:
    // ========== Intrinsic统一接口 ==========
    
    /**
     * @brief 生成编译器intrinsic的LLVM IR（统一入口）
     * @param name intrinsic名称（len, is_empty等）
     * @param builder IRBuilder引用
     * @param args 参数值列表
     * @param arg_names 参数名列表（与args对应）
     * @param variable_types 变量类型映射
     * @param named_values 变量值映射
     * @return intrinsic的结果值
     */
    llvm::Value* generateIntrinsic(
        const std::string& name,
        llvm::IRBuilder<>& builder,
        const std::vector<llvm::Value*>& args,
        const std::vector<std::string>& arg_names,
        const std::map<std::string, llvm::Type*>& variable_types,
        const std::map<std::string, llvm::Value*>& named_values
    );

private:
    // ========== Intrinsic具体实现 ==========
    
    /**
     * @brief 生成len()的LLVM IR
     */
    llvm::Value* generateLen(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg,
        const std::string& arg_name,
        const std::map<std::string, llvm::Type*>& variable_types,
        const std::map<std::string, llvm::Value*>& named_values
    );
    
    /**
     * @brief 生成is_empty()的LLVM IR
     */
    llvm::Value* generateIsEmpty(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg,
        const std::string& arg_name,
        const std::map<std::string, llvm::Type*>& variable_types,
        const std::map<std::string, llvm::Value*>& named_values
    );
    
    /**
     * @brief 生成debug()的LLVM IR
     */
    llvm::Value* generateDebug(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg,
        const std::string& arg_name,
        const std::map<std::string, llvm::Type*>& variable_types,
        const std::map<std::string, llvm::Value*>& named_values
    );
    
    /**
     * @brief 生成abs()的LLVM IR
     */
    llvm::Value* generateAbs(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg
    );
    
    /**
     * @brief 生成min()的LLVM IR
     */
    llvm::Value* generateMin(
        llvm::IRBuilder<>& builder,
        llvm::Value* a,
        llvm::Value* b
    );
    
    /**
     * @brief 生成max()的LLVM IR
     */
    llvm::Value* generateMax(
        llvm::IRBuilder<>& builder,
        llvm::Value* a,
        llvm::Value* b
    );
    
    /**
     * @brief 生成clamp()的LLVM IR
     */
    llvm::Value* generateClamp(
        llvm::IRBuilder<>& builder,
        llvm::Value* val,
        llvm::Value* min_val,
        llvm::Value* max_val
    );
    
    /**
     * @brief 生成pow()的LLVM IR
     */
    llvm::Value* generatePow(
        llvm::IRBuilder<>& builder,
        llvm::Value* base,
        llvm::Value* exp
    );
    
    /**
     * @brief 生成sqrt()的LLVM IR
     */
    llvm::Value* generateSqrt(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg
    );
    
    /**
     * @brief 生成floor()的LLVM IR
     */
    llvm::Value* generateFloor(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg
    );
    
    /**
     * @brief 生成ceil()的LLVM IR
     */
    llvm::Value* generateCeil(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg
    );
    
    /**
     * @brief 生成round()的LLVM IR
     */
    llvm::Value* generateRound(
        llvm::IRBuilder<>& builder,
        llvm::Value* arg
    );
};

} // namespace pawc

#endif // PAWC_BUILTINS_H

