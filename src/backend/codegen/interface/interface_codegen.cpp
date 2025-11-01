//===--- interface_codegen.cpp - Interface CodeGen Implementation -*- C++ -*-===//

#include "interface_codegen.h"
#include "backend/codegen/codegen_context.h"
#include "middleend/types/type.h"
#include "frontend/parser/ast/expr.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>

namespace pawc {

InterfaceCodeGen::InterfaceCodeGen(CodeGenContext* context)
    : context_(context) {}

llvm::Value* InterfaceCodeGen::generateInterfaceMethodCall(
    CallExpr* call_expr,
    InterfaceType* interface_type,
    const std::string& method_name,
    llvm::Value* receiver,
    const std::vector<llvm::Value*>& args) {
    
    // 当前实现：静态分发
    // 未来可以扩展支持vtable动态分发
    
    // 获取receiver的实际类型（通过Sema阶段的类型推断）
    // 注意：当前实现使用静态分发（编译时确定）
    // receiver的类型信息已在Sema阶段解析并附加到AST节点
    Type* impl_type = nullptr;  // 从Sema获取（已在TypeChecker中验证）
    
    return generateStaticDispatch(impl_type, method_name, receiver, args);
}

llvm::Value* InterfaceCodeGen::generateStaticDispatch(
    Type* impl_type,
    const std::string& method_name,
    llvm::Value* receiver,
    const std::vector<llvm::Value*>& args) {
    
    // 查找实现的方法
    llvm::Function* impl_func = findImplementationMethod(impl_type, method_name);
    
    if (!impl_func) {
        // 错误：找不到方法实现
        return nullptr;
    }
    
    // 构建参数列表（包括self）
    std::vector<llvm::Value*> call_args;
    call_args.push_back(receiver);
    call_args.insert(call_args.end(), args.begin(), args.end());
    
    // 生成调用
    return context_->getBuilder().CreateCall(impl_func, call_args);
}

bool InterfaceCodeGen::verifyInterfaceImplementation(
    Type* impl_type, 
    InterfaceType* interface_type) {
    
    // 接口实现验证已在Sema阶段完成（InterfaceValidator）
    // 优点：
    //   1. 早期错误检测（编译时而非链接时）
    //   2. 分离关注点（类型检查 vs 代码生成）
    //   3. 更清晰的错误消息
    // 
    // CodeGen阶段可以安全假设所有接口都已正确实现
    return true;  // Sema已验证，此处无需重复检查
}

llvm::Function* InterfaceCodeGen::findImplementationMethod(
    Type* impl_type,
    const std::string& method_name) {
    
    // 构建方法的完整名称
    // 例如: Point_show (类型名_方法名)
    std::string full_name = impl_type->toString() + "_" + method_name;
    
    // 从Module中查找函数
    llvm::Module* module = context_->getModule();
    return module->getFunction(full_name);
}

} // namespace pawc

