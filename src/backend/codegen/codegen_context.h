//===--- codegen_context.h - Code Generation Context -------------*- C++ -*-===//
//
// PawLang Compiler - LLVM Code Generation Context
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_CONTEXT_H
#define PAW_CODEGEN_CONTEXT_H

#include "middleend/types/type.h"
#include "middleend/types/primitive_types.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/generic_types.h"
#include "middleend/symbol/symbol_table.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

#include <string>
#include <unordered_map>
#include <memory>

namespace pawc {

/// CodeGenContext - LLVM代码生成上下文
///
/// 管理：
/// - LLVM上下文 (LLVMContext, Module, IRBuilder)
/// - 类型映射 (PawLang Type → LLVM Type)
/// - 符号映射 (变量、函数)
/// - 作用域管理
class CodeGenContext {
public:
    CodeGenContext(const std::string& module_name, TypeSystem* type_system,
                   SymbolTable* symbol_table);
    ~CodeGenContext() = default;
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // LLVM Core Objects
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::LLVMContext& getLLVMContext() { return context_; }
    llvm::Module* getModule() { return module_.get(); }
    llvm::IRBuilder<>& getBuilder() { return builder_; }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Type Mapping (28种类型 → LLVM类型)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::Type* getLLVMType(Type* paw_type);
    
    // 基础类型映射
    llvm::Type* getI8Type();
    llvm::Type* getI16Type();
    llvm::Type* getI32Type();
    llvm::Type* getI64Type();
    llvm::Type* getI128Type();
    
    llvm::Type* getU8Type();
    llvm::Type* getU16Type();
    llvm::Type* getU32Type();
    llvm::Type* getU64Type();
    llvm::Type* getU128Type();
    
    llvm::Type* getF8Type();    // bfloat16
    llvm::Type* getF16Type();   // half
    llvm::Type* getF32Type();   // float
    llvm::Type* getF64Type();   // double
    llvm::Type* getF128Type();  // fp128
    
    llvm::Type* getBoolType();   // i1
    llvm::Type* getCharType();   // i8
    llvm::Type* getStringType(); // i8*
    llvm::Type* getVoidType();
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Symbol Management
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    // 变量管理
    void defineVariable(const std::string& name, llvm::Value* value);
    llvm::Value* lookupVariable(const std::string& name);
    
    // 函数管理
    void registerFunction(const std::string& name, llvm::Function* func);
    llvm::Function* lookupFunction(const std::string& name);
    
    /// 基于参数类型查找重载函数（用于builtin）
    llvm::Function* lookupFunctionWithTypes(const std::string& name,
                                            const std::vector<Type*>& param_types);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Scope Management
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void enterScope();
    void exitScope();
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Helper Functions
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    // 创建alloca指令（用于变量声明）
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* func,
                                             const std::string& var_name,
                                             llvm::Type* type);
    
    // 获取当前BasicBlock
    llvm::BasicBlock* getCurrentBlock();
    
    // 创建新的BasicBlock
    llvm::BasicBlock* createBasicBlock(const std::string& name,
                                       llvm::Function* func = nullptr);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Runtime Functions
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void declareRuntimeFunctions();
    llvm::Function* getRuntimeFunction(const std::string& name);
    void registerRuntimeFunction(const std::string& name, llvm::Function* func);
    
    /// 注册所有builtin函数的LLVM声明（print, println, to_string等）
    void registerAllBuiltinFunctions();
    
    /// 生成类型签名字符串（用于函数查找）
    std::string getTypeSignature(const std::vector<Type*>& types);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Module Output
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void dump(); // 输出LLVM IR到stdout
    bool writeToFile(const std::string& filename);
    bool emitObjectFile(const std::string& filename);
    
private:
    // LLVM核心对象
    llvm::LLVMContext context_;
    std::unique_ptr<llvm::Module> module_;
    llvm::IRBuilder<> builder_;
    
    // PawLang编译器组件
    TypeSystem* type_system_;
    SymbolTable* symbol_table_;
    
    // 类型缓存 (PawLang Type → LLVM Type)
    std::unordered_map<Type*, llvm::Type*> type_cache_;
    
    // 符号表 (变量名 → LLVM Value)
    std::vector<std::unordered_map<std::string, llvm::Value*>> variable_stack_;
    
    // 函数表 (函数名 → LLVM Function)
    std::unordered_map<std::string, llvm::Function*> function_table_;
    
    // Runtime函数表
    std::unordered_map<std::string, llvm::Function*> runtime_functions_;
    
    // Builtin函数重载表: name -> [(param_types, llvm_function)]
    std::unordered_map<std::string, std::vector<std::pair<std::vector<Type*>, llvm::Function*>>> builtin_overloads_;
    
    // 辅助方法
    llvm::Type* mapPrimitiveType(Type* type);
    llvm::Type* mapArrayType(ArrayType* type);
    llvm::Type* mapTupleType(TupleType* type);
    llvm::Type* mapStructType(StructType* type);
    llvm::Type* mapOptionalType(OptionalType* type);
    llvm::Type* mapResultType(ResultType* type);
    llvm::Type* mapFunctionType(FunctionType* type);
    llvm::Type* mapEnumType(EnumType* type);
};

} // namespace pawc

#endif // PAW_CODEGEN_CONTEXT_H
