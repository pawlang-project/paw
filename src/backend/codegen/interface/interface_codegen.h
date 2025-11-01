//===--- interface_codegen.h - Interface Code Generation --------*- C++ -*-===//
//
// 接口代码生成
// 负责生成接口方法调用、vtable（如果需要）
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_INTERFACE_CODEGEN_H
#define PAW_CODEGEN_INTERFACE_CODEGEN_H

#include <string>
#include <vector>

namespace llvm {
    class Value;
    class Function;
    class Type;
}

namespace pawc {

class CodeGenContext;
class Type;
class InterfaceType;
class CallExpr;

/// InterfaceCodeGen - 接口代码生成器
///
/// 职责：
/// 1. 生成接口方法调用代码
/// 2. 处理静态分发（当前实现）
/// 3. 预留vtable支持（未来可能需要动态分发）
class InterfaceCodeGen {
    CodeGenContext* context_;
    
public:
    explicit InterfaceCodeGen(CodeGenContext* context);
    
    /// 生成接口方法调用
    /// @param call_expr 调用表达式
    /// @param interface_type 接口类型
    /// @param method_name 方法名称
    /// @param receiver 接收者对象
    /// @param args 参数列表
    /// @return 调用结果
    llvm::Value* generateInterfaceMethodCall(
        CallExpr* call_expr,
        InterfaceType* interface_type,
        const std::string& method_name,
        llvm::Value* receiver,
        const std::vector<llvm::Value*>& args);
    
    /// 生成静态分发的方法调用
    /// @param impl_type 实现类型
    /// @param method_name 方法名称
    /// @param receiver 接收者对象
    /// @param args 参数列表
    /// @return 调用结果
    llvm::Value* generateStaticDispatch(
        Type* impl_type,
        const std::string& method_name,
        llvm::Value* receiver,
        const std::vector<llvm::Value*>& args);
    
    /// 验证接口实现
    /// @param impl_type 实现类型
    /// @param interface_type 接口类型
    /// @return 是否实现了接口
    bool verifyInterfaceImplementation(Type* impl_type, 
                                       InterfaceType* interface_type);
    
private:
    /// 查找实现的方法
    llvm::Function* findImplementationMethod(
        Type* impl_type,
        const std::string& method_name);
};

} // namespace pawc

#endif // PAW_CODEGEN_INTERFACE_CODEGEN_H

