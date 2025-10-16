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

class CodeGenerator {
public:
    CodeGenerator(const std::string& module_name);
    CodeGenerator(const std::string& module_name, SymbolTable* symbol_table);
    
    // 生成代码
    bool generate(const Program& program);
    
    // 输出IR
    void printIR();
    void saveIR(const std::string& filename);
    
    // 编译到目标文件
    bool compileToObject(const std::string& filename);
    
    // 获取模块
    llvm::Module* getModule() { return module_.get(); }
    
private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    std::unique_ptr<Builtins> builtins_;  // 内置函数管理器
    
    // 符号表
    std::map<std::string, llvm::Value*> named_values_;
    std::map<std::string, llvm::Type*> variable_types_;      // 变量类型映射（用于数组等）
    
    // 循环控制：维护循环标签栈 (continue_target, break_target)
    std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loop_stack_;
    
    std::map<std::string, llvm::Function*> functions_;
    std::map<std::string, llvm::StructType*> struct_types_;  // struct类型映射
    std::map<std::string, const StructStmt*> struct_defs_;   // struct定义
    std::map<std::string, const EnumStmt*> enum_defs_;       // enum定义
    std::map<std::string, std::map<std::string, llvm::Function*>> struct_methods_;  // struct方法映射
    
    // 泛型支持
    std::map<std::string, const FunctionStmt*> generic_functions_;  // 泛型函数定义
    std::map<std::string, const StructStmt*> generic_structs_;      // 泛型struct定义
    std::map<std::string, const EnumStmt*> generic_enums_;          // 泛型enum定义
    std::map<std::string, std::map<std::string, llvm::Type*>> type_param_map_;  // 类型参数映射
    
    // 当前函数和类型上下文
    llvm::Function* current_function_;
    const Type* current_function_return_type_;  // 当前函数的返回类型（用于ok/err类型推导）
    const StructStmt* current_struct_;  // 当前处理的struct（用于self）
    std::string current_struct_name_;   // 当前struct名称
    bool current_is_method_;            // 当前函数是否是实例方法（有self参数）
    
    // 模块系统
    std::string module_name_;           // 当前模块名称
    SymbolTable* symbol_table_;         // 符号表（可能为nullptr）
    
    // 类型转换
    llvm::Type* convertType(const Type* type);
    llvm::Type* convertPrimitiveType(PrimitiveType type);
    llvm::StructType* getOrCreateStructType(const std::string& name);
    llvm::Type* getEnumType(const std::string& name);
    
    // Optional类型辅助函数
    llvm::StructType* createOptionalType(llvm::Type* value_type);
    
    // 代码生成 - 表达式
    llvm::Value* generateExpr(const Expr* expr);
    llvm::Value* generateIdentifierExpr(const IdentifierExpr* expr);
    llvm::Value* generateBinaryExpr(const BinaryExpr* expr);
    llvm::Value* generateUnaryExpr(const UnaryExpr* expr);
    llvm::Value* generateCallExpr(const CallExpr* expr);
    llvm::Value* generateAssignExpr(const AssignExpr* expr);
    llvm::Value* generateBuiltinCall(const std::string& name, const std::vector<ExprPtr>& args);
    llvm::Value* generateMemberAccessExpr(const MemberAccessExpr* expr);
    llvm::Value* generateStructLiteralExpr(const StructLiteralExpr* expr);
    llvm::Value* generateEnumVariantExpr(const EnumVariantExpr* expr);
    llvm::Value* generateArrayLiteralExpr(const ArrayLiteralExpr* expr);
    llvm::Value* generateIndexExpr(const IndexExpr* expr);
    llvm::Value* generateMatchExpr(const MatchExpr* expr);
    llvm::Value* generateIsExpr(const IsExpr* expr);
    llvm::Value* generateIfExpr(const IfExpr* expr);
    llvm::Value* generateCastExpr(const CastExpr* expr);
    llvm::Value* generateTryExpr(const TryExpr* expr);
    llvm::Value* generateOkExpr(const OkExpr* expr);
    llvm::Value* generateErrExpr(const ErrExpr* expr);
    
    // 代码生成 - 语句
    void generateStmt(const Stmt* stmt);
    void generateFunctionStmt(const FunctionStmt* stmt);
    void generateExternStmt(const ExternStmt* stmt);
    void generateStructStmt(const StructStmt* stmt);
    void generateEnumStmt(const EnumStmt* stmt);
    void generateImplStmt(const ImplStmt* stmt);
    void generateLetStmt(const LetStmt* stmt);
    void generateReturnStmt(const ReturnStmt* stmt);
    void generateIfStmt(const IfStmt* stmt);
    void generateLoopStmt(const LoopStmt* stmt);
    void generateBreakStmt(const BreakStmt* stmt);
    void generateContinueStmt(const ContinueStmt* stmt);
    void generateBlockStmt(const BlockStmt* stmt);
    void generateExprStmt(const ExprStmt* stmt);
    
    // 模式匹配辅助
    bool matchPattern(llvm::Value* value, const Pattern* pattern, 
                     std::map<std::string, llvm::Value*>& bindings);
    
    // 泛型辅助
    std::string mangleGenericName(const std::string& base_name, const std::vector<TypePtr>& type_args);
    llvm::Type* resolveGenericType(const Type* type);
    bool isGenericFunction(const std::string& name);
    llvm::Function* instantiateGenericFunction(const std::string& name, const std::vector<TypePtr>& type_args);
    llvm::Type* instantiateGenericStruct(const std::string& name, const std::vector<TypePtr>& type_args);
    llvm::Type* instantiateGenericEnum(const std::string& name, const std::vector<TypePtr>& type_args);
    
    // 跨模块类型转换
    llvm::Type* convertTypeToCurrentContext(llvm::Type* type);
    
    // 从其他模块导入类型定义（重建Struct/Enum）
    void importTypeFromModule(const std::string& type_name, const std::string& from_module);
};

} // namespace pawc

#endif // PAWC_CODEGEN_H



