/**
 * @file codegen.h
 * @brief PawLang LLVM代码生成器
 * 
 * 负责将AST转换为LLVM IR，支持完整的PawLang语言特性：
 * - 泛型系统（函数、Struct、Enum）
 * - 泛型struct内部方法
 * - 模块系统和跨模块调用
 * - 错误处理（T?类型和?操作符）
 * - 模式匹配
 * - 类型推导
 * 
 * @author PawLang Project
 * @date 2025-10-16
 */

#ifndef PAWC_CODEGEN_H
#define PAWC_CODEGEN_H

#include "../parser/ast.h"
#include "../builtins/builtins.h"
#include "../module/symbol_table.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include <map>
#include <string>

namespace pawc {

/**
 * @class CodeGenerator
 * @brief LLVM IR代码生成器
 * 
 * 将PawLang AST转换为LLVM IR的核心类。
 * 支持单态化泛型、跨模块编译、错误处理等高级特性。
 */
class CodeGenerator {
public:
    /**
     * @brief 构造函数（单文件模式）
     * @param module_name 模块名称
     */
    CodeGenerator(const std::string& module_name);
    
    /**
     * @brief 构造函数（多文件模式）
     * @param module_name 模块名称
     * @param symbol_table 符号表指针（用于跨模块查找）
     */
    CodeGenerator(const std::string& module_name, SymbolTable* symbol_table);
    
    /**
     * @brief 生成LLVM IR代码
     * @param program AST程序节点
     * @return true 成功，false 失败
     */
    bool generate(const Program& program);
    
    /**
     * @brief 打印生成的LLVM IR到stdout
     */
    void printIR();
    
    /**
     * @brief 保存LLVM IR到文件
     * @param filename 输出文件名（.ll文件）
     */
    void saveIR(const std::string& filename);
    
    /**
     * @brief 编译LLVM IR到目标文件
     * @param filename 输出文件名（.o文件）
     * @return true 成功，false 失败
     */
    bool compileToObject(const std::string& filename);
    
    /**
     * @brief 获取LLVM模块
     * @return llvm::Module* LLVM模块指针
     */
    llvm::Module* getModule() { return module_.get(); }
    
private:
    // ========== LLVM核心组件 ==========
    std::unique_ptr<llvm::LLVMContext> context_;  ///< LLVM上下文
    std::unique_ptr<llvm::Module> module_;        ///< LLVM模块
    std::unique_ptr<llvm::IRBuilder<>> builder_;  ///< LLVM IR构建器
    std::unique_ptr<Builtins> builtins_;          ///< 内置函数管理器
    
    // ========== 符号表 ==========
    std::map<std::string, llvm::Value*> named_values_;      ///< 变量名 -> LLVM值
    std::map<std::string, llvm::Type*> variable_types_;     ///< 变量名 -> LLVM类型（用于GEP等）
    std::map<std::string, llvm::Type*> array_element_types_;///< 数组参数 -> 元素类型（泛型）
    
    // ========== 循环控制 ==========
    /// 循环标签栈：(continue_target, break_target)
    std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loop_stack_;
    
    // ========== 函数和类型注册 ==========
    std::map<std::string, llvm::Function*> functions_;                              ///< 函数映射
    std::map<std::string, llvm::StructType*> struct_types_;                         ///< Struct类型映射
    std::map<std::string, const StructStmt*> struct_defs_;                          ///< Struct定义
    std::map<std::string, const EnumStmt*> enum_defs_;                              ///< Enum定义
    std::map<std::string, std::map<std::string, llvm::Function*>> struct_methods_;  ///< Struct方法映射
    
    // ========== 泛型系统 ==========
    std::map<std::string, const FunctionStmt*> generic_functions_;  ///< 泛型函数定义
    std::map<std::string, const StructStmt*> generic_structs_;      ///< 泛型struct定义
    std::map<std::string, const EnumStmt*> generic_enums_;          ///< 泛型enum定义
    std::map<std::string, std::map<std::string, llvm::Type*>> type_param_map_;  ///< 类型参数映射
    std::map<std::string, const FunctionStmt*> generic_struct_methods_;  ///< 泛型struct方法定义
    
    // ========== 上下文状态 ==========
    llvm::Function* current_function_;              ///< 当前正在生成的函数
    const Type* current_function_return_type_;      ///< 当前函数返回类型（用于ok/err推导）
    const StructStmt* current_struct_;              ///< 当前处理的struct（用于self）
    std::string current_struct_name_;               ///< 当前struct名称
    bool current_is_method_;                        ///< 是否是实例方法（有self参数）
    
    // ========== 模块系统 ==========
    std::string module_name_;                       ///< 当前模块名称
    SymbolTable* symbol_table_;                     ///< 符号表（跨模块查找）
    
    // ========== 类型转换 ==========
    
    /**
     * @brief 将AST类型转换为LLVM类型
     * @param type AST类型节点
     * @return llvm::Type* LLVM类型，失败返回nullptr
     */
    llvm::Type* convertType(const Type* type);
    
    /**
     * @brief 转换基础类型
     * @param type 基础类型枚举
     * @return llvm::Type* LLVM类型
     */
    llvm::Type* convertPrimitiveType(PrimitiveType type);
    
    /**
     * @brief 获取或创建Struct类型
     * @param name Struct名称
     * @return llvm::StructType* Struct类型指针
     */
    llvm::StructType* getOrCreateStructType(const std::string& name);
    
    /**
     * @brief 获取Enum类型
     * @param name Enum名称
     * @return llvm::Type* Enum类型（tagged union）
     */
    llvm::Type* getEnumType(const std::string& name);
    
    // ========== Optional类型（T?错误处理） ==========
    
    /**
     * @brief 创建Optional<T>类型
     * @param value_type 值类型
     * @return llvm::StructType* Optional类型结构
     */
    llvm::StructType* createOptionalType(llvm::Type* value_type);
    
    /**
     * @brief 确保Optional enum定义存在
     * @param value_type 值类型
     * @param type_name Optional类型名称
     */
    void ensureOptionalEnumDef(llvm::Type* value_type, const std::string& type_name);
    
    // ========== 表达式生成 ==========
    
    /**
     * @brief 表达式生成调度函数
     * @param expr 表达式AST节点
     * @return llvm::Value* 生成的LLVM值
     */
    llvm::Value* generateExpr(const Expr* expr);
    
    /**
     * @brief 生成标识符表达式
     * @param expr 标识符表达式节点
     * @return llvm::Value* 变量的值
     */
    llvm::Value* generateIdentifierExpr(const IdentifierExpr* expr);
    
    /**
     * @brief 生成二元表达式（算术、比较、逻辑）
     * @param expr 二元表达式节点
     * @return llvm::Value* 运算结果
     */
    llvm::Value* generateBinaryExpr(const BinaryExpr* expr);
    
    /**
     * @brief 生成一元表达式（-, !）
     * @param expr 一元表达式节点
     * @return llvm::Value* 运算结果
     */
    llvm::Value* generateUnaryExpr(const UnaryExpr* expr);
    
    /**
     * @brief 生成函数调用表达式
     * @param expr 函数调用节点
     * @return llvm::Value* 调用返回值
     * 
     * 支持:
     * - 普通函数调用
     * - 泛型函数调用
     * - 跨模块调用
     * - Struct静态方法（Pair::new<K,V>）
     * - Struct实例方法（p.method()）
     */
    llvm::Value* generateCallExpr(const CallExpr* expr);
    
    /**
     * @brief 生成赋值表达式
     * @param expr 赋值表达式节点
     * @return llvm::Value* 赋值后的值
     */
    llvm::Value* generateAssignExpr(const AssignExpr* expr);
    
    /**
     * @brief 生成内置函数调用
     * @param name 内置函数名
     * @param args 参数列表
     * @return llvm::Value* 调用返回值
     */
    llvm::Value* generateBuiltinCall(const std::string& name, const std::vector<ExprPtr>& args);
    
    /**
     * @brief 处理函数参数值（数组传递为指针）
     * @param arg 参数表达式
     * @return llvm::Value* 参数值
     */
    llvm::Value* generateArgumentValue(const Expr* arg);
    
    /**
     * @brief 生成成员访问表达式（obj.field）
     * @param expr 成员访问节点
     * @return llvm::Value* 成员值
     */
    llvm::Value* generateMemberAccessExpr(const MemberAccessExpr* expr);
    
    /**
     * @brief 生成Struct字面量（Point { x: 10, y: 20 }）
     * @param expr Struct字面量节点
     * @return llvm::Value* Struct指针（堆分配）
     */
    llvm::Value* generateStructLiteralExpr(const StructLiteralExpr* expr);
    
    /**
     * @brief 生成Enum变体（Option::Some(42)）
     * @param expr Enum变体节点
     * @return llvm::Value* Enum值指针
     */
    llvm::Value* generateEnumVariantExpr(const EnumVariantExpr* expr);
    
    /**
     * @brief 生成数组字面量（[1, 2, 3]）
     * @param expr 数组字面量节点
     * @return llvm::Value* 数组指针
     */
    llvm::Value* generateArrayLiteralExpr(const ArrayLiteralExpr* expr);
    
    /**
     * @brief 生成数组索引（arr[i]）
     * @param expr 索引表达式节点
     * @return llvm::Value* 元素值
     */
    llvm::Value* generateIndexExpr(const IndexExpr* expr);
    
    /**
     * @brief 生成match表达式
     * @param expr Match表达式节点
     * @return llvm::Value* Match结果值
     */
    llvm::Value* generateMatchExpr(const MatchExpr* expr);
    
    /**
     * @brief 生成is表达式（模式匹配）
     * @param expr Is表达式节点
     * @return llvm::Value* 匹配结果（bool）
     */
    llvm::Value* generateIsExpr(const IsExpr* expr);
    
    /**
     * @brief 生成if表达式（if cond { a } else { b }）
     * @param expr If表达式节点
     * @return llvm::Value* 条件表达式结果
     */
    llvm::Value* generateIfExpr(const IfExpr* expr);
    
    /**
     * @brief 生成类型转换表达式（x as T）
     * @param expr Cast表达式节点
     * @return llvm::Value* 转换后的值
     */
    llvm::Value* generateCastExpr(const CastExpr* expr);
    
    /**
     * @brief 生成错误传播表达式（expr?）
     * @param expr Try表达式节点
     * @return llvm::Value* 成功值或触发return
     */
    llvm::Value* generateTryExpr(const TryExpr* expr);
    
    /**
     * @brief 生成ok表达式（ok(value)）
     * @param expr Ok表达式节点
     * @return llvm::Value* Optional Value变体
     */
    llvm::Value* generateOkExpr(const OkExpr* expr);
    
    /**
     * @brief 生成err表达式（err("message")）
     * @param expr Err表达式节点
     * @return llvm::Value* Optional Error变体
     */
    llvm::Value* generateErrExpr(const ErrExpr* expr);
    
    // ========== 语句生成 ==========
    
    /**
     * @brief 语句生成调度函数
     * @param stmt 语句AST节点
     */
    void generateStmt(const Stmt* stmt);
    
    /**
     * @brief 生成函数定义
     * @param stmt 函数定义节点
     */
    void generateFunctionStmt(const FunctionStmt* stmt);
    
    /**
     * @brief 生成extern声明
     * @param stmt Extern声明节点
     */
    void generateExternStmt(const ExternStmt* stmt);
    
    /**
     * @brief 生成Struct定义
     * @param stmt Struct定义节点
     * 
     * 处理:
     * - 泛型struct定义注册
     * - Struct字段类型
     * - Struct内部方法生成
     */
    void generateStructStmt(const StructStmt* stmt);
    
    /**
     * @brief 生成Enum定义
     * @param stmt Enum定义节点
     */
    void generateEnumStmt(const EnumStmt* stmt);
    
    /**
     * @brief 生成impl块（已废弃，保留兼容）
     * @param stmt Impl语句节点
     */
    void generateImplStmt(const ImplStmt* stmt);
    
    /**
     * @brief 生成变量声明（let/let mut）
     * @param stmt Let语句节点
     * 
     * 处理:
     * - 类型推导
     * - 可变性检查
     * - Struct引用语义
     * - 数组初始化
     */
    void generateLetStmt(const LetStmt* stmt);
    
    /**
     * @brief 生成return语句
     * @param stmt Return语句节点
     */
    void generateReturnStmt(const ReturnStmt* stmt);
    
    /**
     * @brief 生成if语句
     * @param stmt If语句节点
     */
    void generateIfStmt(const IfStmt* stmt);
    
    /**
     * @brief 生成loop语句
     * @param stmt Loop语句节点
     * 
     * 支持:
     * - 范围循环（loop i in 0..100）
     * - 迭代器循环（loop item in arr）
     * - 条件循环（loop condition）
     * - 无限循环（loop）
     */
    void generateLoopStmt(const LoopStmt* stmt);
    
    /**
     * @brief 生成break语句
     * @param stmt Break语句节点
     */
    void generateBreakStmt(const BreakStmt* stmt);
    
    /**
     * @brief 生成continue语句
     * @param stmt Continue语句节点
     */
    void generateContinueStmt(const ContinueStmt* stmt);
    
    /**
     * @brief 生成代码块
     * @param stmt Block语句节点
     */
    void generateBlockStmt(const BlockStmt* stmt);
    
    /**
     * @brief 生成表达式语句
     * @param stmt 表达式语句节点
     */
    void generateExprStmt(const ExprStmt* stmt);
    
    // ========== 模式匹配 ==========
    
    /**
     * @brief 模式匹配辅助函数
     * @param value 要匹配的值
     * @param pattern 模式节点
     * @param bindings 绑定的变量表
     * @return true 匹配成功，false 匹配失败
     */
    bool matchPattern(llvm::Value* value, const Pattern* pattern, 
                     std::map<std::string, llvm::Value*>& bindings);
    
    // ========== 泛型系统 ==========
    
    /**
     * @brief 生成泛型名称修饰（name mangling）
     * @param base_name 基础名称
     * @param type_args 类型参数列表
     * @return std::string 修饰后的名称（如sum_i32）
     */
    std::string mangleGenericName(const std::string& base_name, const std::vector<TypePtr>& type_args);
    
    /**
     * @brief 解析泛型struct名称
     * @param mangled_name 修饰名称
     * @return std::string 解析后的名称
     */
    std::string resolveGenericStructName(const std::string& mangled_name);
    
    /**
     * @brief 解析泛型类型（替换类型参数）
     * @param type AST类型节点
     * @return llvm::Type* 解析后的LLVM类型
     */
    llvm::Type* resolveGenericType(const Type* type);
    
    /**
     * @brief 检查是否是泛型函数
     * @param name 函数名
     * @return true 是泛型函数
     */
    bool isGenericFunction(const std::string& name);
    
    /**
     * @brief 实例化泛型函数（单态化）
     * @param name 泛型函数名
     * @param type_args 类型参数列表
     * @return llvm::Function* 实例化的函数
     */
    llvm::Function* instantiateGenericFunction(const std::string& name, const std::vector<TypePtr>& type_args);
    
    /**
     * @brief 实例化泛型Struct（单态化）
     * @param name 泛型Struct名
     * @param type_args 类型参数列表
     * @return llvm::Type* 实例化的Struct类型
     */
    llvm::Type* instantiateGenericStruct(const std::string& name, const std::vector<TypePtr>& type_args);
    
    /**
     * @brief 实例化泛型Enum（单态化）
     * @param name 泛型Enum名
     * @param type_args 类型参数列表
     * @return llvm::Type* 实例化的Enum类型
     */
    llvm::Type* instantiateGenericEnum(const std::string& name, const std::vector<TypePtr>& type_args);
    
    /**
     * @brief 实例化泛型struct的所有方法
     * @param generic_struct 泛型struct定义
     * @param struct_mangled_name Struct修饰名称
     * @param struct_type Struct LLVM类型
     * @param type_args 类型参数列表
     * 
     * 当泛型struct被实例化时调用，生成所有方法的具体版本。
     */
    void instantiateGenericStructMethods(
        const StructStmt* generic_struct,
        const std::string& struct_mangled_name,
        llvm::StructType* struct_type,
        const std::vector<TypePtr>& type_args
    );
    
    // ========== 跨模块支持 ==========
    
    /**
     * @brief 将其他模块的类型转换到当前Context
     * @param type 其他模块的LLVM类型
     * @return llvm::Type* 当前Context中的类型
     */
    llvm::Type* convertTypeToCurrentContext(llvm::Type* type);
    
    /**
     * @brief 从其他模块导入类型定义
     * @param type_name 类型名称
     * @param from_module 源模块名
     */
    void importTypeFromModule(const std::string& type_name, const std::string& from_module);
};

} // namespace pawc

#endif // PAWC_CODEGEN_H
