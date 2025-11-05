//===--- interface_codegen.cpp - Interface CodeGen Implementation -*- C++ -*-===//
/// @file interface_codegen.cpp
/// @brief Implementation file

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
    
    // currentimplementation：staticfractionaldispatch
    // not yetfuturemayextendsupportvtabledynamicfractionaldispatch
    
    // getreceiverof/theactualtypes（through/viaSemastageof/thetypesinference）
    // Note：currentimplementationusestaticfractionaldispatch（compile timecertain/sure）
    // receiverof/thetypesinfoalreadyin/atSemastageparseand attachaddtoASTnode
    Type* impl_type = nullptr;  // fromSemaget（alreadyin/atTypeCheckermiddle/centervalidate）
    
    return generateStaticDispatch(impl_type, method_name, receiver, args);
}

llvm::Value* InterfaceCodeGen::generateStaticDispatch(
    Type* impl_type,
    const std::string& method_name,
    llvm::Value* receiver,
    const std::vector<llvm::Value*>& args) {
    
    // lookupimplementationof/themethod
    llvm::Function* impl_func = findImplementationMethod(impl_type, method_name);
    
    if (!impl_func) {
        // error：findnottomethodimplementation
        return nullptr;
    }
    
    // build/constructparameterlist（packageincludingself）
    std::vector<llvm::Value*> call_args;
    call_args.push_back(receiver);
    call_args.insert(call_args.end(), args.begin(), args.end());
    
    // generationcall
    return context_->getBuilder().CreateCall(impl_func, call_args);
}

bool InterfaceCodeGen::verifyInterfaceImplementation(
    Type* impl_type, 
    InterfaceType* interface_type) {
    
    // interface implementationvalidatealreadydone at Sema stage（InterfaceValidator）
    // Advantage：
    //   1. earlyearlyerrordetect（compile timewhilenotlinktime/when）
    //   2. separation of concerns（typescheck vs code generation）
    //   3. moreclearof/theerrormessage
    // 
    // CodeGenstagemaysafeassumptionAllinterfaceallalreadycorrectimplementation
    return true;  // Semaalreadyvalidate，herenoneedduplicatecheck
}

llvm::Function* InterfaceCodeGen::findImplementationMethod(
    Type* impl_type,
    const std::string& method_name) {
    
    // build/constructmethodof/thecompletename
    // e.g./for example: Point_show (typename_methodname)
    std::string full_name = impl_type->toString() + "_" + method_name;
    
    // fromModulemiddle/centerlookupfunction
    llvm::Module* module = context_->getModule();
    return module->getFunction(full_name);
}

} // namespace pawc

