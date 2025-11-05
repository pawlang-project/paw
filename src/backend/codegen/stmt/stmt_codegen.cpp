//===--- stmt_codegen.cpp - Statement CodeGen Implementation -----*- C++ -*-===//
/// @file stmt_codegen.cpp
/// @brief Statement and declaration code generation implementation
///
/// Generates LLVM IR for statements, control flow, and type declarations.
/// Implements two-pass function generation for forward references.

#include "stmt_codegen.h"
#include "../expr/expr_codegen.h"
#include "../type/type_codegen.h"
#include "middleend/types/generic_types.h"
#include "middleend/types/type_system.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <iostream>
#include <set>

namespace pawc {

/// Initialize statement code generator and link with expression generator
StmtCodeGen::StmtCodeGen(CodeGenContext* context, ExprCodeGen* expr_codegen)
    : CodeGenBase(context), expr_codegen_(expr_codegen), results_(nullptr) {
    // Link ExprCodeGen with StmtCodeGen (for BlockExpr)
    if (expr_codegen_) {
        expr_codegen_->setStmtCodeGen(this);
    }
}

llvm::Value* StmtCodeGen::generate(ASTNode* node) {
    if (auto* stmt = dynamic_cast<Stmt*>(node)) {
        stmt->accept(this);
        return results_;
    }
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Statement generation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void StmtCodeGen::visit(ExprStmt* node) {
    std::cerr << "[ExprStmt] Generating expression statement" << std::endl;
    results_ = expr_codegen_->generate(node->getExpr());
    std::cerr << "[ExprStmt] Result: " << (results_ ? "valid" : "null") << std::endl;
}

void StmtCodeGen::visit(VarDecl* node) {
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    auto& builder = context_->getBuilder();
    
    // === Special handling: FunctionType (closure) ===
    // Closure types cannot be directly alloca'd, need to store closure pointer or closure object
    if (node->getType() && node->getType()->getKind() == Type::Kind::Function) {
        // Generate closure initialization expression
        if (node->getInit()) {
            llvm::Value* closure_value = expr_codegen_->generate(node->getInit());
            
            if (closure_value) {
                // Closure value can be:
                // 1. Function pointer (no captures)
                // 2. Closure struct pointer (with captures)
                
                // Create alloca of pointer type to store closure
                llvm::Type* closure_storage_type = closure_value->getType();
                
                llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
                    func,
                    node->getName(),
                    closure_storage_type
                );
                
                builder.CreateStore(closure_value, alloca);
                context_->defineVariable(node->getName(), alloca);
                results_ = alloca;
            }
        }
        return;
    }
    
    // === Normal type process ===
    // Get variable type - use CodeGenContext's unified type map
    llvm::Type* var_type = node->getType() ?
        context_->getLLVMType(node->getType()) :
        llvm::Type::getInt32Ty(context_->getLLVMContext());
    
    // Create alloca
    llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
        func,
        node->getName(),
        var_type
    );
    
    // If there's an initial value, generate and store it
    if (node->getInit()) {
        llvm::Value* init_value = expr_codegen_->generate(node->getInit());
        if (init_value) {
            builder.CreateStore(init_value, alloca);
        }
    }
    
    // Register variable
    context_->defineVariable(node->getName(), alloca);
    
    results_ = alloca;
}

void StmtCodeGen::visit(DestructuringDecl* node) {
    // Tuple destructuring: let (a, b) = tuple_expr;
    auto& builder = context_->getBuilder();
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!node->getInit()) {
        results_ = nullptr;
        return;
    }
    
    // Generate tuple expression
    llvm::Value* tuple_value = expr_codegen_->generate(node->getInit());
    if (!tuple_value) {
        results_ = nullptr;
        return;
    }
    
    // Create alloca for each variable and extract corresponding tuple element
    for (size_t i = 0; i < node->getNames().size(); ++i) {
        // Extract tuple element
        llvm::Value* element = builder.CreateExtractValue(tuple_value, i, "tuple.elem." + std::to_string(i));
        
        // Create alloca for variable
        llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
            func,
            node->getNames()[i],
            element->getType()
        );
        
        // Store element value
        builder.CreateStore(element, alloca);
        
        // Register variable
        context_->defineVariable(node->getNames()[i], alloca);
    }
    
    results_ = tuple_value;
}

void StmtCodeGen::visit(StructDestructuringDecl* node) {
    // Struct destructuring: let Point { x, y } = p;
    auto& builder = context_->getBuilder();
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!node->getInit()) {
        results_ = nullptr;
        return;
    }
    
    // Generate struct expression
    llvm::Value* struct_value = expr_codegen_->generate(node->getInit());
    if (!struct_value) {
        results_ = nullptr;
        return;
    }
    
    // Create alloca for each field and extract corresponding struct field
    // Use ExtractValue instead of GEP (assuming struct_value is passed by value)
    for (size_t i = 0; i < node->getFieldNames().size(); ++i) {
        // Extract field directly from struct value
        llvm::Value* field_value = builder.CreateExtractValue(
            struct_value,
            i,
            "struct.field." + node->getFieldNames()[i]
        );
        
        // Create alloca for variable
        llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
            func,
            node->getFieldNames()[i],
            field_value->getType()
        );
        
        // Store field value
        builder.CreateStore(field_value, alloca);
        
        // Register variable
        context_->defineVariable(node->getFieldNames()[i], alloca);
    }
    
    results_ = struct_value;
}

// 🔧 Generate function declaration (signature only, supports forward references)
void StmtCodeGen::generateFunctionDeclaration(FunctionDecl* node) {
    // Check if function already exists
    std::string check_name = (node->getName() == "main") ? "paw.main" : node->getName();
    if (!current_support_type_.empty()) {
        check_name = current_support_type_ + "_" + node->getName();
    }
    
    if (context_->getModule()->getFunction(check_name)) {
        // Function already exists, skip
        return;
    }
    
    // createfunctiontypes
    std::vector<llvm::Type*> param_types;
    llvm::Type* ret_type = nullptr;
    
    if (node->hasResolvedTypes()) {
        for (auto* type : node->getResolvedParamTypes()) {
            param_types.push_back(context_->getLLVMType(type));
        }
        ret_type = context_->getLLVMType(node->getResolvedReturnType());
    } else {
        for (const auto& param : node->getParams()) {
            param_types.push_back(context_->getLLVMType(param.type));
        }
        ret_type = context_->getLLVMType(node->getReturnType());
    }
    
    auto* func_type = llvm::FunctionType::get(ret_type, param_types, false);
    
    // Create function (declaration only, don't generate function body)
    llvm::Function::LinkageTypes linkage = (node->getName() == "main") ?
        llvm::Function::InternalLinkage : llvm::Function::ExternalLinkage;
    
    std::string func_name = check_name;
    
    llvm::Function* func = llvm::Function::Create(
        func_type,
        linkage,
        func_name,
        context_->getModule()
    );
    
    // Register function to context
    context_->registerFunction(node->getName(), func);
}

void StmtCodeGen::visit(FunctionDecl* node) {
    // Look up already created function (created during first pass)
    std::string func_name = (node->getName() == "main") ? "paw.main" : node->getName();
    
    if (!current_support_type_.empty()) {
        func_name = current_support_type_ + "_" + node->getName();
    }
    
    llvm::Function* func = context_->getModule()->getFunction(func_name);
    
    if (!func) {
        // If first pass was skipped (e.g., in support block), create now
        generateFunctionDeclaration(node);
        func = context_->getModule()->getFunction(func_name);
    }
    
    if (!func) {
        results_ = nullptr;
        return;
    }
    
    // Create entry basic block (if not already present)
    if (func->empty()) {
        llvm::BasicBlock* entry = context_->createBasicBlock("entry", func);
        context_->getBuilder().SetInsertPoint(entry);
    } else {
        // Already has basic block, set insertion point to first block
        context_->getBuilder().SetInsertPoint(&func->getEntryBlock());
    }
    
    // Enter function scope
    context_->enterScope();
    
    // Add parameters to symbol table
    size_t i = 0;
    for (auto& arg : func->args()) {
        const auto& param = node->getParams()[i];
        arg.setName(param.name);
        
        // 🔧 Fix: Use resolved_param_types (if available) to create alloca
        Type* param_type = param.type;
        if (node->hasResolvedTypes() && i < node->getResolvedParamTypes().size()) {
            param_type = node->getResolvedParamTypes()[i];
        }
        
        // Create alloca and store parameter value - use CodeGenContext's type map to ensure consistency
        llvm::AllocaInst* alloca = context_->createEntryBlockAlloca(
            func,
            param.name,
            context_->getLLVMType(param_type)
        );
        context_->getBuilder().CreateStore(&arg, alloca);
        context_->defineVariable(param.name, alloca);
        
        i++;
    }
    
    // generate function body
    if (node->getBody()) {
        node->getBody()->accept(this);
    }
    
    // If no return, add default return
    if (!context_->getCurrentBlock()->getTerminator()) {
        llvm::Type* return_type = func->getReturnType();
        if (return_type->isVoidTy()) {
            context_->getBuilder().CreateRetVoid();
        }
    }
    
    // Exit function scope
    context_->exitScope();
    
    // Special handling: Generate C ABI wrapper for main()
    if (node->getName() == "main") {
        generateMainWrapper(func, func->getReturnType());
    }
    
    results_ = func;
}

void StmtCodeGen::generateMainWrapper(llvm::Function* paw_main, llvm::Type* paw_return_type) {
    // Generate C ABI-compatible main function: int main() { ... return 0/results; }
    auto& builder = context_->getBuilder();
    auto* i32_type = llvm::Type::getInt32Ty(context_->getLLVMContext());
    auto* main_type = llvm::FunctionType::get(i32_type, {}, false);
    
    llvm::Function* c_main = llvm::Function::Create(
        main_type,
        llvm::Function::ExternalLinkage,
        "main",
        context_->getModule()
    );
    
    // Create entry block
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(
        context_->getLLVMContext(), "entry", c_main);
    
    // Save current insertion point
    auto saved_block = builder.GetInsertBlock();
    
    // Switch to C main
    builder.SetInsertPoint(entry);
    
    // Call PawLang's main
    llvm::Value* paw_results = builder.CreateCall(paw_main, {});
    
    // Process return value based on return type
    if (paw_return_type->isVoidTy()) {
        // void main() -> return 0
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
    } else if (paw_return_type->isIntegerTy(32)) {
        // i32 main() -> return results
        builder.CreateRet(paw_results);
    } else if (paw_return_type->isStructTy()) {
        // Result<i32> main() -> extract value and return
        // Result struct: { i1 is_ok, T value, ptr error_message }
        // If is_ok, return value; otherwise return 1 (error code)
        
        // === Key fix: First alloca Result, then perform GEP ===
        // Create temporary alloca to store Result value
        llvm::AllocaInst* results_alloca = llvm::IRBuilder<>(
            &c_main->getEntryBlock(),
            c_main->getEntryBlock().begin()
        ).CreateAlloca(paw_return_type, nullptr, "results_tmp");
        
        // Store Result value
        builder.CreateStore(paw_results, results_alloca);
        
        // Extract is_ok field
        llvm::Value* is_ok_ptr = builder.CreateStructGEP(
            paw_return_type,
            results_alloca,  // Use alloca pointer
            0
        );
        llvm::Value* is_ok = builder.CreateLoad(
            builder.getInt1Ty(),
            is_ok_ptr,
            "is_ok"
        );
        
        // Create conditional branch
        llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "main.ok", c_main);
        llvm::BasicBlock* err_bb = llvm::BasicBlock::Create(
            context_->getLLVMContext(), "main.err", c_main);
        
        builder.CreateCondBr(is_ok, ok_bb, err_bb);
        
        // OK branch: Extract value and return
        builder.SetInsertPoint(ok_bb);
        llvm::Value* value_ptr = builder.CreateStructGEP(
            paw_return_type,
            results_alloca,  // Use alloca pointer
            1
        );
        llvm::Type* value_type = paw_return_type->getStructElementType(1);
        llvm::Value* value = builder.CreateLoad(value_type, value_ptr, "value");
        
        // If value is i32, return directly; otherwise return 0
        if (value_type->isIntegerTy(32)) {
            builder.CreateRet(value);
        } else {
            builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
        }
        
        // ERR branch: Return 1 (error code)
        builder.SetInsertPoint(err_bb);
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 1));
    } else {
        // Other types -> return 0
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
    }
    
    // Restore insertion point
    if (saved_block) {
        builder.SetInsertPoint(saved_block);
    }
}

void StmtCodeGen::visit(ReturnStmt* node) {
    auto& builder = context_->getBuilder();
    
    if (node->getValue()) {
        llvm::Value* ret_val = expr_codegen_->generate(node->getValue());
        
        if (!ret_val) {
            results_ = nullptr;
            return;
        }
        
        // === self/fromdynamicwrapOptionalandResult ===
        // getcurrentfunctionof/thereturntypes
        llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
        llvm::Type* fn_ret_type = current_fn->getReturnType();
        
        // Checkyesnoneedwrapis/asOptionalorResult
        Type* ret_val_type = node->getValue()->getType();
        
        // iffunctionreturnOptional<T>，andreturnvalueyesT（notOptional），self/fromdynamicwrap
        if (fn_ret_type->isStructTy() && fn_ret_type->getStructNumElements() == 2) {
            // Possibly Optional<T> = { i1 has_value, T value }
            // Check if return value type matches Optional's value type
            llvm::Type* expected_value_type = fn_ret_type->getStructElementType(1);
            
            if (ret_val->getType() == expected_value_type) {
                // Auto-wrap as Optional: { true, value }
                llvm::Value* optional_value = llvm::UndefValue::get(fn_ret_type);
                
                // Set has_value = true
                optional_value = builder.CreateInsertValue(optional_value,
                    llvm::ConstantInt::get(builder.getInt1Ty(), 1),  // true
                    0);
                
                // Set value
                optional_value = builder.CreateInsertValue(optional_value, ret_val, 1);
                
                results_ = builder.CreateRet(optional_value);
                return;
            }
            
            // === Special handling: null return (Optional<void> → Optional<T>) ===
            // If return value is Optional type (possibly null) and first field type matches
            if (ret_val->getType()->isStructTy() &&
                ret_val->getType()->getStructNumElements() == 2 &&
                ret_val->getType()->getStructElementType(0)->isIntegerTy(1)) {
                
                // This might be null (Optional<void>), needs to be converted to Optional<T>
                // Extract has_value field
                llvm::Value* has_value = builder.CreateExtractValue(ret_val, 0, "has_value");
                
                // Create correctly typed Optional<T>
                llvm::Value* converted_optional = llvm::UndefValue::get(fn_ret_type);
                
                // Copy has_value (should be false for null)
                converted_optional = builder.CreateInsertValue(converted_optional, has_value, 0);
                
                // Value field remains undef (not used when has_value=false)
                
                results_ = builder.CreateRet(converted_optional);
                return;
            }
        }
        
        // Otherwise return directly
        results_ = builder.CreateRet(ret_val);
    } else {
        results_ = builder.CreateRetVoid();
    }
}

void StmtCodeGen::visit(IfStmt* node) {
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    // Generate condition
    llvm::Value* cond = expr_codegen_->generate(node->getCondition());
    
    // Create basic blocks
    llvm::BasicBlock* then_bb = context_->createBasicBlock("if.then", func);
    llvm::BasicBlock* else_bb = node->getElseStmt() ?
        context_->createBasicBlock("if.else", func) : nullptr;
    llvm::BasicBlock* merge_bb = context_->createBasicBlock("if.end", func);
    
    // Create branch
    if (else_bb) {
        context_->getBuilder().CreateCondBr(cond, then_bb, else_bb);
    } else {
        context_->getBuilder().CreateCondBr(cond, then_bb, merge_bb);
    }
    
    // Generate then branch
    context_->getBuilder().SetInsertPoint(then_bb);
    node->getThenStmt()->accept(this);
    bool then_has_terminator = context_->getCurrentBlock()->getTerminator() != nullptr;
    if (!then_has_terminator) {
        context_->getBuilder().CreateBr(merge_bb);
    }
    
    // Generate else branch
    bool else_has_terminator = false;
    if (else_bb) {
        context_->getBuilder().SetInsertPoint(else_bb);
        node->getElseStmt()->accept(this);
        else_has_terminator = context_->getCurrentBlock()->getTerminator() != nullptr;
        if (!else_has_terminator) {
            context_->getBuilder().CreateBr(merge_bb);
        }
    }
    
    // Only set insertion point if merge block is reachable
    // If both branches have terminators (both return), merge block is unreachable, need to add unreachable
    if (then_has_terminator && else_has_terminator && else_bb) {
        // Both branches have terminators, merge block is unreachable
        context_->getBuilder().SetInsertPoint(merge_bb);
        context_->getBuilder().CreateUnreachable();
    } else {
        // Merge block is reachable, normally set as insertion point
        context_->getBuilder().SetInsertPoint(merge_bb);
    }
    
    results_ = nullptr;
}

void StmtCodeGen::visit(LoopStmt* node) {
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    llvm::BasicBlock* loop_bb = context_->createBasicBlock("loop", func);
    llvm::BasicBlock* after_bb = context_->createBasicBlock("afterloop", func);
    
    // Save break/continue targets
    llvm::BasicBlock* old_break = break_target_;
    llvm::BasicBlock* old_continue = continue_target_;
    break_target_ = after_bb;
    continue_target_ = loop_bb;
    
    // Jump to loop
    context_->getBuilder().CreateBr(loop_bb);
    
    // Generate loop body
    context_->getBuilder().SetInsertPoint(loop_bb);
    node->getBody()->accept(this);
    
    // If no terminator, continue looping
    if (!context_->getCurrentBlock()->getTerminator()) {
        context_->getBuilder().CreateBr(loop_bb);
    }
    
    // Restore break/continue targets
    break_target_ = old_break;
    continue_target_ = old_continue;
    
    // Continue at after block
    context_->getBuilder().SetInsertPoint(after_bb);
    results_ = nullptr;
}

void StmtCodeGen::visit(WhileStmt* node) {
    // while is equivalent to: loop { if !cond { break } ... }
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    llvm::BasicBlock* loop_bb = context_->createBasicBlock("while.loop", func);
    llvm::BasicBlock* body_bb = context_->createBasicBlock("while.body", func);
    llvm::BasicBlock* after_bb = context_->createBasicBlock("while.end", func);
    
    // Jump to loop
    context_->getBuilder().CreateBr(loop_bb);
    
    // Loop header: Check condition
    context_->getBuilder().SetInsertPoint(loop_bb);
    llvm::Value* cond = expr_codegen_->generate(node->getCondition());
    context_->getBuilder().CreateCondBr(cond, body_bb, after_bb);
    
    // Loop body
    context_->getBuilder().SetInsertPoint(body_bb);
    
    llvm::BasicBlock* old_break = break_target_;
    llvm::BasicBlock* old_continue = continue_target_;
    break_target_ = after_bb;
    continue_target_ = loop_bb;
    
    node->getBody()->accept(this);
    
    if (!context_->getCurrentBlock()->getTerminator()) {
        context_->getBuilder().CreateBr(loop_bb);
    }
    
    break_target_ = old_break;
    continue_target_ = old_continue;
    
    // Continue at after block
    context_->getBuilder().SetInsertPoint(after_bb);
    results_ = nullptr;
}

void StmtCodeGen::visit(BreakStmt* node) {
    if (break_target_) {
        results_ = context_->getBuilder().CreateBr(break_target_);
    }
}

void StmtCodeGen::visit(ContinueStmt* node) {
    if (continue_target_) {
        results_ = context_->getBuilder().CreateBr(continue_target_);
    }
}

void StmtCodeGen::visit(BlockStmt* node) {
    context_->enterScope();
    
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
        
        // If already has terminator, stop generating subsequent code
        if (context_->getCurrentBlock()->getTerminator()) {
            break;
        }
    }
    
    context_->exitScope();
    results_ = nullptr;
}

void StmtCodeGen::visit(ForStmt* node) {
    // for i in 0..10 { ... } expands to traditional loop
    
    auto& builder = context_->getBuilder();
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Generate iterator expression (should be a Range)
    llvm::Value* iterator = expr_codegen_->generate(node->getIterator());
    if (!iterator) {
        results_ = nullptr;
        return;
    }
    
    // If iterator is a Range struct {start, end, inclusive}
    llvm::Type* iter_type = iterator->getType();
    
    if (iter_type->isStructTy()) {
        llvm::StructType* range_struct = llvm::cast<llvm::StructType>(iter_type);
        
        // Extract start, end, inclusive
        llvm::Value* start = builder.CreateExtractValue(iterator, 0, "range.start");
        llvm::Value* end = builder.CreateExtractValue(iterator, 1, "range.end");
        llvm::Value* inclusive = builder.CreateExtractValue(iterator, 2, "range.inclusive");
        
        // Create loop variable
        llvm::AllocaInst* loop_var = context_->createEntryBlockAlloca(
            func,
            node->getVarName(),
            start->getType()
        );
        builder.CreateStore(start, loop_var);
        context_->defineVariable(node->getVarName(), loop_var);
        
        // Create basic blocks
        llvm::BasicBlock* cond_bb = context_->createBasicBlock("for.cond", func);
        llvm::BasicBlock* body_bb = context_->createBasicBlock("for.body", func);
        llvm::BasicBlock* inc_bb = context_->createBasicBlock("for.inc", func);
        llvm::BasicBlock* end_bb = context_->createBasicBlock("for.end", func);
        
        // Jump to condition check
        builder.CreateBr(cond_bb);
        
        // Condition check: i < end or i <= end
        builder.SetInsertPoint(cond_bb);
        llvm::Value* current = builder.CreateLoad(start->getType(), loop_var, "i");
        llvm::Value* cond;
        
        // Choose comparison operator based on inclusive flag
        llvm::BasicBlock* lt_bb = context_->createBasicBlock("for.lt", func);
        llvm::BasicBlock* le_bb = context_->createBasicBlock("for.le", func);
        llvm::BasicBlock* cmp_bb = context_->createBasicBlock("for.cmp", func);
        
        builder.CreateCondBr(inclusive, le_bb, lt_bb);
        
        // i < end
        builder.SetInsertPoint(lt_bb);
        llvm::Value* cond_lt = builder.CreateICmpSLT(current, end, "cond.lt");
        builder.CreateBr(cmp_bb);
        
        // i <= end
        builder.SetInsertPoint(le_bb);
        llvm::Value* cond_le = builder.CreateICmpSLE(current, end, "cond.le");
        builder.CreateBr(cmp_bb);
        
        // PHI node selects condition
        builder.SetInsertPoint(cmp_bb);
        llvm::PHINode* phi = builder.CreatePHI(builder.getInt1Ty(), 2, "cond");
        phi->addIncoming(cond_lt, lt_bb);
        phi->addIncoming(cond_le, le_bb);
        
        builder.CreateCondBr(phi, body_bb, end_bb);
        
        // Loop body
        builder.SetInsertPoint(body_bb);
        
        llvm::BasicBlock* old_break = break_target_;
        llvm::BasicBlock* old_continue = continue_target_;
        break_target_ = end_bb;
        continue_target_ = inc_bb;
        
        node->getBody()->accept(this);
        
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(inc_bb);
        }
        
        break_target_ = old_break;
        continue_target_ = old_continue;
        
        // Increment: i = i + 1
        builder.SetInsertPoint(inc_bb);
        llvm::Value* current_inc = builder.CreateLoad(start->getType(), loop_var, "i.inc");
        llvm::Value* next = builder.CreateAdd(current_inc, 
                                               llvm::ConstantInt::get(start->getType(), 1), 
                                               "i.next");
        builder.CreateStore(next, loop_var);
        builder.CreateBr(cond_bb);
        
        // Continue at end block
        builder.SetInsertPoint(end_bb);
    }
    
    results_ = nullptr;
}

void StmtCodeGen::visit(StructDecl* node) {
    // Type definition doesn't need to generate LLVM IR
    // TypeSystem already registered types in TypeChecker phase
    results_ = nullptr;
}

void StmtCodeGen::visit(EnumDecl* node) {
    // Type definition doesn't need to generate LLVM IR
    // TypeSystem already registered types in TypeChecker phase
    results_ = nullptr;
}

void StmtCodeGen::visit(InterfaceDecl* node) {
    // Interface definition doesn't need to generate LLVM IR
    // TypeSystem already registered types in TypeChecker phase
    results_ = nullptr;
}

void StmtCodeGen::visit(SupportDecl* node) {
    // Support block: Generate interface method implementations
    // 🔧 Self/support context: Set current type name, allowing methods to use mangled name
    
    std::string saved_support_type = current_support_type_;
    current_support_type_ = node->getTypeName();
    
    // 1. Generate user-implemented methods
    for (const auto& method : node->getMethods()) {
        method->accept(this);
    }
    
    // 2. 🔧 Generate wrappers for interface default methods
    Type* interface_type = context_->getTypeSystem()->lookupType(node->getInterfaceName());
    if (interface_type && interface_type->getKind() == Type::Kind::Interface) {
        auto* iface = static_cast<InterfaceType*>(interface_type);
        
        // Get list of already implemented method names
        std::set<std::string> implemented_methods;
        for (const auto& method : node->getMethods()) {
            implemented_methods.insert(method->getName());
        }
        
        // Find methods with default implementation that haven't been implemented
        // 🔧 Build generic parameter substitution map (for default methods)
        std::map<std::string, Type*> generic_substitution;
        if (iface->isGeneric() && node->isInterfaceGeneric()) {
            const auto& interface_def_param_names = iface->getGenericParamNames();
            const auto& interface_inst_params = node->getInterfaceGenericParams();
            
            for (size_t i = 0; i < interface_def_param_names.size() && i < interface_inst_params.size(); ++i) {
                Type* concrete_type = context_->getTypeSystem()->lookupType(interface_inst_params[i].name);
                if (concrete_type) {
                    generic_substitution[interface_def_param_names[i]] = concrete_type;
                }
            }
        }
        
        for (const auto& iface_method : iface->getMethods()) {
            if (iface_method.has_default_impl && 
                implemented_methods.find(iface_method.name) == implemented_methods.end()) {
                // Generate wrapper for default method (pass generic substitution map)
                generateDefaultMethod(node->getTypeName(), iface_method, generic_substitution);
            }
        }
    }
    
    // Restore context
    current_support_type_ = saved_support_type;
    
    results_ = nullptr;
}

void StmtCodeGen::generateDefaultMethod(const std::string& type_name, 
                                        const InterfaceType::MethodSignature& method,
                                        const std::map<std::string, Type*>& generic_substitution) {
    // 🔧 Generate wrapper for interface default method for type
    // Method name format: type_name::method_name (consistent with other methods)
    
    auto& builder = context_->getBuilder();
    std::string mangled_name = type_name + "::" + method.name;
    
    // Check if already exists
    if (context_->getModule()->getFunction(mangled_name)) {
        return;  // Already exists, skip
    }
    
    std::cerr << "[DefaultMethod] Generating default method: " << mangled_name << std::endl;
    if (!generic_substitution.empty()) {
        std::cerr << "[DefaultMethod] Using generic substitution" << std::endl;
    }
    
    // build/constructfunctiontypes
    std::vector<llvm::Type*> param_types;
    std::vector<Type*> paw_param_types;  // save PawLang types（used forbindparameter）
    
    // firstparameter：self（typesis/as type_name）
    Type* self_type = context_->getTypeSystem()->lookupType(type_name);
    if (!self_type) {
        std::cerr << "[DefaultMethod] Error: Type not found: " << type_name << std::endl;
        return;
    }
    
    // according tofirstparameterof/thetypescertain/sure self of/thepasswaystyle/form
    bool is_self_ref = false;
    bool is_self_mut = false;
    
    if (!method.param_types.empty() && method.param_types[0]->getKind() == Type::Kind::Reference) {
        is_self_ref = true;
        auto* ref_type = static_cast<ReferenceType*>(method.param_types[0]);
        is_self_mut = ref_type->isMutable();
    }
    
    if (is_self_ref) {
        // self yesreferencetypes，passpointer
        param_types.push_back(llvm::PointerType::getUnqual(builder.getContext()));
        paw_param_types.push_back(context_->getTypeSystem()->getReferenceType(self_type, is_self_mut));
    } else {
        // self yesvaluetypes
        param_types.push_back(context_->getLLVMType(self_type));
        paw_param_types.push_back(self_type);
    }
    
    // 🔧 otherparameter - applygenericsubstitution！
    for (size_t i = 1; i < method.param_types.size(); ++i) {
        Type* param_type = method.param_types[i];
        
        // applygenericsubstitution
        if (!generic_substitution.empty()) {
            // simplesinglesubstitution：ifyesgenerictypes，frommapmiddle/centerlookupsubstitution
            if (param_type->getKind() == Type::Kind::Generic) {
                auto* generic = static_cast<GenericType*>(param_type);
                auto it = generic_substitution.find(generic->getName());
                if (it != generic_substitution.end()) {
                    param_type = it->second;
                    std::cerr << "[DefaultMethod] Substituted param type: " << generic->getName() 
                              << " -> " << param_type->toString() << std::endl;
                }
            }
        }
        
        paw_param_types.push_back(param_type);
        param_types.push_back(context_->getLLVMType(param_type));
    }
    
    // returntypes
    llvm::Type* return_type = context_->getLLVMType(method.return_type);
    
    // createfunction
    auto* fn_type = llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function* wrapper_fn = llvm::Function::Create(
        fn_type,
        llvm::Function::ExternalLinkage,
        mangled_name,
        context_->getModule()
    );
    
    // savecurrentinsertedpoint
    llvm::BasicBlock* saved_bb = builder.GetInsertBlock();
    
    // createfunctionbody/struct
    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(
        builder.getContext(),
        "entry",
        wrapper_fn
    );
    builder.SetInsertPoint(entry_bb);
    
    // 🔧 Key：generatecalldefaultimplementationof/thecode
    // enterscope
    context_->enterScope();
    
    // bindparametertosymboltable（packageincluding self）
    auto args_iter = wrapper_fn->arg_begin();
    
    // firstparameteryes self
    args_iter->setName("self");
    std::cerr << "[DefaultMethod] Self arg type: " << (args_iter->getType()->isPointerTy() ? "pointer" : "value") << std::endl;
    
    llvm::AllocaInst* self_alloca = context_->createEntryBlockAlloca(
        wrapper_fn,
        "self",
        args_iter->getType()
    );
    builder.CreateStore(&(*args_iter), self_alloca);
    context_->defineVariable("self", self_alloca);
    
    std::cerr << "[DefaultMethod] Self variable defined" << std::endl;
    ++args_iter;
    
    // 🔧 otherparameter - usetrueactualparametername！
    size_t param_idx = 1;  // skip self
    for (; args_iter != wrapper_fn->arg_end(); ++args_iter, ++param_idx) {
        // 🔧 usetrueactualparametername（like/such as "other"）
        std::string param_name = (param_idx < method.param_names.size()) 
                               ? method.param_names[param_idx] 
                               : ("arg" + std::to_string(param_idx));
        args_iter->setName(param_name);
        
        llvm::AllocaInst* param_alloca = context_->createEntryBlockAlloca(
            wrapper_fn,
            param_name,
            args_iter->getType()
        );
        builder.CreateStore(&(*args_iter), param_alloca);
        context_->defineVariable(param_name, param_alloca);
        
        std::cerr << "[DefaultMethod] Bound parameter: " << param_name << std::endl;
    }
    
    // generationdefaultimplementationof/the body
    if (method.default_body) {
        std::cerr << "[DefaultMethod] Generating body..." << std::endl;
        
        // 🔧 check body yesnoyes BlockExpr
        if (auto* block_expr = dynamic_cast<BlockExpr*>(method.default_body)) {
            std::cerr << "[DefaultMethod] Body is BlockExpr with " << block_expr->getStmts().size() << " statements" << std::endl;
            
            // BlockExpr containsmany/muchindividual/piecestatement，needone by one/eachindividual/pieceexecute
            llvm::Value* last_value = nullptr;
            for (size_t i = 0; i < block_expr->getStmts().size(); ++i) {
                const auto& stmt = block_expr->getStmts()[i];
                bool is_last = (i == block_expr->getStmts().size() - 1);
                
                stmt->accept(this);  // use StmtCodeGen executestatement
                
                // 🔧 Last statement's return value used as BlockExpr's return value
                if (is_last) {
                    last_value = results_;  // from StmtCodeGen of/the results_ get
                }
            }
            
            // returnlastone/aindividual/pieceexpressionof/thevalue
            if (last_value && !return_type->isVoidTy()) {
                std::cerr << "[DefaultMethod] Returning last value from BlockExpr" << std::endl;
                builder.CreateRet(last_value);
            } else if (return_type->isVoidTy()) {
                builder.CreateRetVoid();
            } else {
                builder.CreateRet(llvm::Constant::getNullValue(return_type));
            }
        } else {
            // singleindividual/pieceexpression
            method.default_body->accept(expr_codegen_);
            llvm::Value* body_results = expr_codegen_->getResult();
            
            std::cerr << "[DefaultMethod] Body results: " << (body_results ? "valid" : "null") << std::endl;
            
            // returnresults
            if (body_results && !return_type->isVoidTy()) {
                std::cerr << "[DefaultMethod] Returning body results" << std::endl;
                builder.CreateRet(body_results);
            } else if (return_type->isVoidTy()) {
                std::cerr << "[DefaultMethod] Returning void" << std::endl;
                builder.CreateRetVoid();
            } else {
                std::cerr << "[DefaultMethod] Returning null value" << std::endl;
                // ifnohasresults，returnzerovalue
                builder.CreateRet(llvm::Constant::getNullValue(return_type));
            }
        }
    } else {
        std::cerr << "[DefaultMethod] No body!" << std::endl;
        // no body（notshouldoccur）
        if (return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::Constant::getNullValue(return_type));
        }
    }
    
    // exitscope
    context_->exitScope();
    
    // recoverinsertedpoint
    if (saved_bb) {
        builder.SetInsertPoint(saved_bb);
    }
    
    std::cerr << "[DefaultMethod] Generated: " << mangled_name << std::endl;
}

} // namespace pawc
