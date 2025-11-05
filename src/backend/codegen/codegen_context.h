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

/// CodeGenContext - LLVMcode generationcontext
///
/// manage：
/// - LLVMcontext (LLVMContext, Module, IRBuilder)
/// - typesmap (PawLang Type → LLVM Type)
/// - symbolmap (variable、function)
/// - scopemanage
class CodeGenContext {
public:
    CodeGenContext(const std::string& module_name, TypeSystem* type_system,
                   SymbolTable* symbol_table);
    ~CodeGenContext();  // 🔧 Bug Fix: explicitstyle/formdestruct/destructionwith/tocontrolLLVMobjectdestruct/destructionsequential
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // LLVM Core Objects
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::LLVMContext& getLLVMContext() { return context_; }
    llvm::Module* getModule() { return module_.get(); }
    llvm::IRBuilder<>& getBuilder() { return builder_; }
    TypeSystem* getTypeSystem() { return type_system_; }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Type Mapping (28types/kindstypes → LLVMtypes)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    llvm::Type* getLLVMType(Type* paw_type);
    
    // basetypesmap
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
    
    // variablemanage
    void defineVariable(const std::string& name, llvm::Value* value);
    llvm::Value* lookupVariable(const std::string& name);
    
    // functionmanage
    void registerFunction(const std::string& name, llvm::Function* func);
    llvm::Function* lookupFunction(const std::string& name);
    
    /// based onparametertypeslookupoverloadfunction（used forbuiltin）
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
    
    // createallocadirective（used forvariabledeclaration）
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* func,
                                             const std::string& var_name,
                                             llvm::Type* type);
    
    // getcurrentBasicBlock
    llvm::BasicBlock* getCurrentBlock();
    
    // createnewof/theBasicBlock
    llvm::BasicBlock* createBasicBlock(const std::string& name,
                                       llvm::Function* func = nullptr);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Runtime Functions
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void declareRuntimeFunctions();
    llvm::Function* getRuntimeFunction(const std::string& name);
    void registerRuntimeFunction(const std::string& name, llvm::Function* func);
    
    /// registerAllbuiltinfunctionof/theLLVMdeclaration（print, println, to_stringetc）
    void registerAllBuiltinFunctions();
    
    /// generationtypessignaturecharacterstring（used forfunctionlookup）
    std::string getTypeSignature(const std::vector<Type*>& types);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Module Output
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void dump(); // outputLLVM IRtostdout
    bool writeToFile(const std::string& filename);
    bool emitObjectFile(const std::string& filename);
    
private:
    // LLVMcoreobject
    // Note：context_ mustfirstFirstdeclaration，so that/this wayitwilllastdestruct/destruction
    // Module and IRBuilder dependentat/in LLVMContext，so context_ mustLive longer than them
    llvm::LLVMContext context_;
    std::unique_ptr<llvm::Module> module_;
    llvm::IRBuilder<> builder_;
    
    // PawLangcompilercomponent
    TypeSystem* type_system_;
    SymbolTable* symbol_table_;
    
    // typecache (PawLang Type → LLVM Type)
    std::unordered_map<Type*, llvm::Type*> type_cache_;
    
    // symboltable (variablename → LLVM Value)
    std::vector<std::unordered_map<std::string, llvm::Value*>> variable_stack_;
    
    // functiontable (function name → LLVM Function)
    std::unordered_map<std::string, llvm::Function*> function_table_;
    
    // Runtimefunctiontable
    std::unordered_map<std::string, llvm::Function*> runtime_functions_;
    
    // Builtinfunctionoverloadtable: name -> [(param_types, llvm_function)]
    std::unordered_map<std::string, std::vector<std::pair<std::vector<Type*>, llvm::Function*>>> builtin_overloads_;
    
    // helpermethod
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
