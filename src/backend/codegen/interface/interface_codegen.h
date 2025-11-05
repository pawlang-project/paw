//===--- interface_codegen.h - Interface Code Generation --------*- C++ -*-===//
//
// interfacecode generation
// negativeresponsiblegenerateinterfacemethodcall、vtable（ifneed）
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

/// InterfaceCodeGen - interfacecodegenerator
///
/// responsibilities：
/// 1. generateinterfacemethodcallcode
/// 2. processstaticfractionaldispatch（currentimplementation）
/// 3. reservedvtablesupport（not yetfuturepossiblyneeddynamicfractionaldispatch）
class InterfaceCodeGen {
    CodeGenContext* context_;
    
public:
    explicit InterfaceCodeGen(CodeGenContext* context);
    
    /// generationinterfacemethodcall
    /// @param call_expr callexpression
    /// @param interface_type interfacetypes
    /// @param method_name methodname
    /// @param receiver receivereceiverobject
    /// @param args parameterlist
    /// @return callresults
    llvm::Value* generateInterfaceMethodCall(
        CallExpr* call_expr,
        InterfaceType* interface_type,
        const std::string& method_name,
        llvm::Value* receiver,
        const std::vector<llvm::Value*>& args);
    
    /// generationstaticfractionaldispatchof/themethodcall
    /// @param impl_type implementationtypes
    /// @param method_name methodname
    /// @param receiver receivereceiverobject
    /// @param args parameterlist
    /// @return callresults
    llvm::Value* generateStaticDispatch(
        Type* impl_type,
        const std::string& method_name,
        llvm::Value* receiver,
        const std::vector<llvm::Value*>& args);
    
    /// validateinterfaceimplementation
    /// @param impl_type implementationtypes
    /// @param interface_type interfacetypes
    /// @return yesnoimplementationimplementedinterface
    bool verifyInterfaceImplementation(Type* impl_type, 
                                       InterfaceType* interface_type);
    
private:
    /// lookupimplementationof/themethod
    llvm::Function* findImplementationMethod(
        Type* impl_type,
        const std::string& method_name);
};

} // namespace pawc

#endif // PAW_CODEGEN_INTERFACE_CODEGEN_H

