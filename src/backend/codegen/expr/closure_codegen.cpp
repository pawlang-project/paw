//===--- closure_codegen.cpp - Closure CodeGen ------------------*- C++ -*-===//
/// @file closure_codegen.cpp
/// @brief Code generation implementation
//
// Closure code generation: complete implementation Phase 2-3
// Phase 2: Environment capture
// Phase 3: Closure object and call
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "middleend/types/generic_types.h"
#include <llvm/IR/Function.h>
#include <sstream>
#include <iostream>

namespace pawc {

void ExprCodeGen::visit(ClosureExpr* node) {
    // Complete closure implementation (Phase 1-3)
    // Phase 1: basefunctiongenerate ✅
    // Phase 2: Environment capture ✅
    // Phase 3: Closure object ✅
    
    auto& builder = context_->getBuilder();
    static int closure_counter = 0;
    
    // Generate unique closure function name
    std::stringstream ss;
    ss << "closure_" << closure_counter++;
    std::string closure_fn_name = ss.str();
    node->setGeneratedName(closure_fn_name);
    
    // === Phase 2: Environment capture analysis ===
    // Capture variable analysis is done at Sema stage (CaptureAnalyzer)
    // Advantage: separation of concerns, semantic analysis and code generation decoupled
    // CodeGenonly needs touseanalysisresultsgenerateclosureenvironment
    const auto& captured_vars = node->getCapturedVars();
    
    // === Phase 3: Closure object design ===
    // Closure object = { function pointer, captured variables... }
    // But LLVM doesn't allow direct allocation of function types, so we:
    // 1. ifnohascapturevariable：returnfunctionpointer
    // 2. ifhascapturevariable：returnclosurestructbody/structpointer
    
    // build/constructparametertypes（notpackageincludingclosurecontext）
    std::vector<llvm::Type*> param_types;
    for (const auto& param : node->getParams()) {
        param_types.push_back(context_->getLLVMType(param.type));
    }
    
    // returntypes
    llvm::Type* return_type = node->getReturnType() ?
        context_->getLLVMType(node->getReturnType()) :
        builder.getVoidTy();
    
    // === case/situation1: nocapturevariable - simplesinglefunctionpointer ===
    if (captured_vars.empty()) {
        // createnormal/regularfunction
        auto* fn_type = llvm::FunctionType::get(return_type, param_types, false);
        llvm::Function* closure_fn = llvm::Function::Create(
            fn_type,
            llvm::Function::InternalLinkage,
            closure_fn_name,
            context_->getModule()
        );
        
        // savecurrentinsertedpoint
        llvm::BasicBlock* saved_bb = builder.GetInsertBlock();
        
        // createfunctionentryblock
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(
            builder.getContext(),
            "entry",
            closure_fn
        );
        builder.SetInsertPoint(entry_bb);
        
        // === completefunctionbody/structgenerate ===
        // 1. enterclosurefunctionscope
        context_->enterScope();
        
        // 2. bindparametertosymboltable
        size_t param_idx = 0;
        for (auto& arg : closure_fn->args()) {
            const auto& param = node->getParams()[param_idx];
            arg.setName(param.name);
            
            // createallocastorageparameter
            llvm::AllocaInst* param_alloca = context_->createEntryBlockAlloca(
                closure_fn,
                param.name,
                arg.getType()
            );
            builder.CreateStore(&arg, param_alloca);
            context_->defineVariable(param.name, param_alloca);
            
            param_idx++;
        }
        
        // 3. generateclosurebody
        llvm::Value* body_results = nullptr;
        if (node->getBody()) {
            node->getBody()->accept(this);
            body_results = results_;  // 🔧 savebodyof/thereturnvalue
        }
        
        // 4. ifnohasterminatedirective，adddefaultreturn
        if (!builder.GetInsertBlock()->getTerminator()) {
            if (return_type->isVoidTy()) {
                builder.CreateRetVoid();
            } else if (body_results) {
                // 🔧 ifbodyproducedvalue，returnit（implicitstyle/formreturn）
                builder.CreateRet(body_results);
            } else {
                // ifbodynohasproducevalue，returnzerovalue
                llvm::Value* zero = llvm::Constant::getNullValue(return_type);
                builder.CreateRet(zero);
            }
        }
        
        // 5. exitscope
        context_->exitScope();
        
        // recoverinsertedpoint
        builder.SetInsertPoint(saved_bb);
        
        // returnfunctionpointer
        results_ = closure_fn;
        return;
    }
    
    // === case/situation2: hascapturevariable - closurestructbody/struct ===
    // structbody/struct: { fn_ptr, captured_var1, captured_var2, ... }
    std::vector<llvm::Type*> closure_struct_fields;
    
    // field0: functionpointer（withclosurecontextparameter）
    std::vector<llvm::Type*> fn_param_types_with_ctx;
    fn_param_types_with_ctx.push_back(
        llvm::PointerType::getUnqual(builder.getContext())  // closurecontextpointer
    );
    for (auto* param_type : param_types) {
        fn_param_types_with_ctx.push_back(param_type);
    }
    
    auto* fn_type_with_ctx = llvm::FunctionType::get(
        return_type,
        fn_param_types_with_ctx,
        false
    );
    auto* fn_ptr_type = llvm::PointerType::getUnqual(context_->getLLVMContext());
    closure_struct_fields.push_back(fn_ptr_type);
    
    // field1+: captureof/thevariable
    for (const auto& captured : captured_vars) {
        closure_struct_fields.push_back(context_->getLLVMType(captured.type));
    }
    
    // createclosurestructbody/structtypes
    llvm::StructType* closure_struct_type = llvm::StructType::create(
        builder.getContext(),
        closure_struct_fields,
        "closure_t_" + closure_fn_name
    );
    
    // createclosurefunction（withcontextparameter）
    llvm::Function* closure_fn = llvm::Function::Create(
        fn_type_with_ctx,
        llvm::Function::InternalLinkage,
        closure_fn_name,
        context_->getModule()
    );
    
    // savecurrentinsertedpoint
    llvm::BasicBlock* saved_bb = builder.GetInsertBlock();
    
    // createfunctionentryblock
    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(
        builder.getContext(),
        "entry",
        closure_fn
    );
    builder.SetInsertPoint(entry_bb);
    
    // === completefunctionbody/structgenerate（withcapturevariable）===
    // 1. enterclosurefunctionscope
    context_->enterScope();
    
    // 2. bindparametertosymboltable（skipfirstcontextparameter）
    auto args_iter = closure_fn->arg_begin();
    args_iter++;  // skipclosurecontextpointer
    
    size_t param_idx = 0;
    for (; args_iter != closure_fn->arg_end(); ++args_iter) {
        const auto& param = node->getParams()[param_idx];
        args_iter->setName(param.name);
        
        // createallocastorageparameter
        llvm::AllocaInst* param_alloca = context_->createEntryBlockAlloca(
            closure_fn,
            param.name,
            args_iter->getType()
        );
        builder.CreateStore(&(*args_iter), param_alloca);
        context_->defineVariable(param.name, param_alloca);
        
        param_idx++;
    }
    
    // 3. fromclosurecontextextractcapturevariableandbindtosymboltable
    if (!captured_vars.empty()) {
        // getclosurecontextpointer（firstparameter）
        llvm::Value* ctx_ptr = &(*closure_fn->arg_begin());
        ctx_ptr->setName("closure_ctx");
        
        // fromclosurestructbody/structextracteach/everycapturevariable
        for (size_t i = 0; i < captured_vars.size(); ++i) {
            const auto& captured = captured_vars[i];
            
            // getvariablefieldpointer（field0yesfunctionpointer，field1+yescapturevariable）
            llvm::Value* var_ptr = builder.CreateStructGEP(
                closure_struct_type,
                ctx_ptr,
                i + 1,  // +1skipfunctionpointer
                captured.name
            );
            
            // willcapturevariableaddtosymboltable（pointing tostructbody/structinsideof/thevalue）
            context_->defineVariable(captured.name, var_ptr);
        }
    }
    
    // 4. generateclosurebody
    llvm::Value* body_results = nullptr;
    if (node->getBody()) {
        node->getBody()->accept(this);
        body_results = results_;  // 🔧 savebodyof/thereturnvalue
    }
    
    // 5. ifnohasterminatedirective，adddefaultreturn
    if (!builder.GetInsertBlock()->getTerminator()) {
        if (return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else if (body_results) {
            // 🔧 ifbodyproducedvalue，returnit（implicitstyle/formreturn）
            builder.CreateRet(body_results);
        } else {
            llvm::Value* zero = llvm::Constant::getNullValue(return_type);
            builder.CreateRet(zero);
        }
    }
    
    // 6. exitscope
    context_->exitScope();
    
    // recoverinsertedpoint
    builder.SetInsertPoint(saved_bb);
    
    // === Phase 3: createclosureobject ===
    // in/atheapup/aboveallocateclosurestructbody/struct（becauseis/asclosurepossiblyescape）
    llvm::Value* closure_size = llvm::ConstantExpr::getSizeOf(closure_struct_type);
    
    // callmallocallocateinsidememory
    llvm::FunctionType* malloc_type = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(builder.getContext()),
        {builder.getInt64Ty()},
        false
    );
    llvm::FunctionCallee malloc_fn = context_->getModule()->getOrInsertFunction(
        "malloc",
        malloc_type
    );
    
    llvm::Value* closure_ptr = builder.CreateCall(
        malloc_fn,
        {builder.CreateIntCast(closure_size, builder.getInt64Ty(), false)}
    );
    
    // Cast to closure struct pointer (opaque pointer model)
    llvm::Value* typed_closure_ptr = closure_ptr;
    
    // setfunctionpointerfield
    llvm::Value* fn_ptr_field = builder.CreateStructGEP(
        closure_struct_type,
        typed_closure_ptr,
        0,
        "fn_ptr"
    );
    builder.CreateStore(closure_fn, fn_ptr_field);
    
    // capturevariable（fromcurrentscope）
    for (size_t i = 0; i < captured_vars.size(); ++i) {
        const auto& captured = captured_vars[i];
        
        // lookupvariable
        llvm::Value* var_value = context_->lookupVariable(captured.name);
        if (!var_value || !captured.type) {
            continue;
        }
        
        // getvariablefieldpointer
        llvm::Value* var_field = builder.CreateStructGEP(
            closure_struct_type,
            typed_closure_ptr,
            i + 1,  // +1becauseis/asthe0individual/pieceyesfunctionpointer
            "captured_" + captured.name
        );
        
        // addload andstoragevalue
        llvm::Value* loaded_value = builder.CreateLoad(
            context_->getLLVMType(captured.type),
            var_value,
            captured.name + "_val"
        );
        builder.CreateStore(loaded_value, var_field);
    }
    
    // returnclosurepointer
    results_ = typed_closure_ptr;
}

} // namespace pawc
