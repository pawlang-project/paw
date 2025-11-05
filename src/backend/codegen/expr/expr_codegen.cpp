//===--- expr_codegen.cpp - Expression CodeGen Implementation ----*- C++ -*-===//
/// @file expr_codegen.cpp
/// @brief Expression code generation implementation
///
/// Generates LLVM IR for all expression types including literals, operators,
/// calls, pattern matching, and closures.

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "../stmt/stmt_codegen.h"
#include "frontend/parser/ast/stmt.h"
#include "frontend/parser/ast/pattern.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <iostream>

namespace pawc {

/// Initialize expression code generator
ExprCodeGen::ExprCodeGen(CodeGenContext* context)
    : CodeGenBase(context), results_(nullptr) {}

llvm::Value* ExprCodeGen::generate(ASTNode* node) {
    if (auto* expr = dynamic_cast<Expr*>(node)) {
        // Debug: Print expression type
        if (dynamic_cast<BinaryExpr*>(expr)) {
            std::cerr << "[ExprCodeGen::generate] BinaryExpr" << std::endl;
        } else if (dynamic_cast<MemberExpr*>(expr)) {
            std::cerr << "[ExprCodeGen::generate] MemberExpr" << std::endl;
        } else if (dynamic_cast<SelfExpr*>(expr)) {
            std::cerr << "[ExprCodeGen::generate] SelfExpr" << std::endl;
        } else if (dynamic_cast<IntLiteral*>(expr)) {
            std::cerr << "[ExprCodeGen::generate] IntLiteral" << std::endl;
        } else {
            std::cerr << "[ExprCodeGen::generate] Other expression type" << std::endl;
        }
        
        results_ = nullptr;  // Reset
        expr->accept(this);
        std::cerr << "[ExprCodeGen::generate] Result: " << (results_ ? "valid" : "null") << std::endl;
        return results_;
    }
    std::cerr << "[ExprCodeGen::generate] Not an expression!" << std::endl;
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Literal generation - moved to literal_codegen.cpp
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Variables and identifiers
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(SelfExpr* node) {
    // self expression: find self in function parameters
    llvm::Value* self_var = context_->lookupVariable("self");
    if (self_var) {
        // Key fix: Properly handle reference type self
        // self_var is alloca, needs Load
        // But Load type depends on whether self is value or reference
        
        Type* self_paw_type = node->getType();
        
        if (self_paw_type) {
            auto& builder = context_->getBuilder();
            
            // Check if self type is a reference
            if (self_paw_type->isReference()) {
                // self is reference type (&self)
                // self_var alloca stores pointer
                // Load to get pointer
                results_ = builder.CreateLoad(
                    llvm::PointerType::getUnqual(builder.getContext()),
                    self_var,
                    "self.ptr"
                );
            } else {
                // self is value type（self）
                // self_var alloca storageof/theyesvalue
                // Load get/detovalue
                llvm::Type* self_llvm_type = context_->getLLVMType(self_paw_type);
                results_ = builder.CreateLoad(
                    self_llvm_type,
                    self_var,
                    "self.val"
                );
            }
        } else {
            // Fallback solution: assume is pointer
            results_ = context_->getBuilder().CreateLoad(
                llvm::PointerType::getUnqual(context_->getBuilder().getContext()),
                self_var,
                "self"
            );
        }
    } else {
        results_ = nullptr;
    }
}

void ExprCodeGen::visit(IdentifierExpr* node) {
    const std::string& name = node->getName();
    
    llvm::Value* var = context_->lookupVariable(name);
    if (var) {
        // === specialhandle：FunctionType（closure）===
        // closurevariablestorageof/theyesfunctionpointer，directlyreturnalloca（pointer），notload
        // CallExprwillnegativeresponsiblecorrectprocess
        if (node->getType() && node->getType()->getKind() == Type::Kind::Function) {
            results_ = var;  // returnpointing tofunctionpointerof/thepointer（alloca）
            return;
        }
        
        // === normal/regulartypes：Loadvariablevalue ===
        llvm::Type* load_type = node->getType() ?
            context_->getLLVMType(node->getType()) :
            context_->getI32Type();
        
        results_ = context_->getBuilder().CreateLoad(load_type, var, name);
    } else {
        results_ = nullptr;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// binary operationsandunary operations - moved to binary_codegen.cpp
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// functioncall（moved tocall_codegen.cpp）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// CallExpr::visitimplementationin/atcall_codegen.cppmiddle/center

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// complex/complicatedexpression
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(StaticAccessExpr* node) {
    // 🔧 M7: staticvisitenum variant（like/such asOption::None）
    auto& builder = context_->getBuilder();
    Type* type = node->getType();
    
    // Checkyesnoyesenumtypes（noparametervariant，like/such asOption::None）
    if (type && type->isEnum()) {
        EnumType* enum_type = static_cast<EnumType*>(type);
        
        // lookupvariantindex
        std::string variant_name = node->getMember();
        int variant_index = -1;
        
        const auto& variants = enum_type->getVariants();
        for (size_t i = 0; i < variants.size(); i++) {
            if (variants[i].first == variant_name) {
                variant_index = static_cast<int>(i);
                break;
            }
        }
        
        if (variant_index < 0) {
            results_ = nullptr;
            return;
        }
        
        // Get enum's LLVM type
        llvm::Type* enum_llvm_type = context_->getLLVMType(enum_type);
        
        // createenum struct: {i32 variant_index, data}
        llvm::Value* enum_value = llvm::UndefValue::get(enum_llvm_type);
        
        // setvariantindex
        enum_value = builder.CreateInsertValue(
            enum_value,
            builder.getInt32(variant_index),
            {0}
        );
        
        // None variantnohasdata，data fieldmaintain/keephold/maintainundef
        
        results_ = enum_value;
        return;
    }
    
    // Other typesstaticvisit（functiontypesetc）
    results_ = nullptr;
}

void ExprCodeGen::visit(MemberExpr* node) {
    // membervisit: obj.field or tuple.0
    std::cerr << "[MemberExpr] Visiting member: " << node->getMember() << std::endl;
    
    node->getObject()->accept(this);
    llvm::Value* object = results_;
    
    if (!object) {
        std::cerr << "[MemberExpr] Object is null!" << std::endl;
        results_ = nullptr;
        return;
    }
    
    std::cerr << "[MemberExpr] Object generated successfully" << std::endl;
    
    auto& builder = context_->getBuilder();
    Type* obj_type = node->getObject()->getType();
    if (!obj_type) {
        std::cerr << "[MemberExpr] Object type is null!" << std::endl;
        results_ = nullptr;
        return;
    }
    
    std::cerr << "[MemberExpr] Object type: " << obj_type->toString() << std::endl;
    
    // 🔧 referencetypesprocess：ifobjectis reference type，getpointeetypes
    if (obj_type->isReference()) {
        auto* ref_type = static_cast<ReferenceType*>(obj_type);
        obj_type = ref_type->getPointeeType();
    }
    
    // Checkyesnoyestuplefieldvisit（membernameyesnumber）
    const std::string& member = node->getMember();
    bool is_tuple_access = !member.empty() && std::isdigit(member[0]);
    
    if (is_tuple_access && obj_type->isTuple()) {
        // tuplefieldvisit: tuple.0, tuple.1 etc
        auto* tuple_type = static_cast<TupleType*>(obj_type);
        
        // parsingfieldindex
        int field_idx = std::stoi(member);
        
        // validateindexhassignificant/effectiveperformance
        if (field_idx < 0 || field_idx >= static_cast<int>(tuple_type->getElementTypes().size())) {
            results_ = nullptr;
            return;
        }
        
        // useCreateExtractValueextracttupleelement
        results_ = builder.CreateExtractValue(object, field_idx, "tuple.field." + member);
        
    } else if (obj_type->isStruct()) {
        // structbody/structmembervisit: struct.field_name
        auto* struct_type = static_cast<StructType*>(obj_type);
        llvm::Type* llvm_struct_type = context_->getLLVMType(struct_type);
        
        std::cerr << "[MemberExpr] Accessing field '" << member << "' on struct " << struct_type->getName() << std::endl;
        std::cerr << "[MemberExpr] Object is pointer: " << object->getType()->isPointerTy() << std::endl;
        
        // lookupfieldindex
        const auto& fields = struct_type->getFields();
        int field_idx = -1;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].first == member) {
                field_idx = static_cast<int>(i);
                break;
            }
        }
        
        if (field_idx < 0) {
            std::cerr << "[MemberExpr] Field not found!" << std::endl;
            results_ = nullptr;
            return;
        }
        
        std::cerr << "[MemberExpr] Field index: " << field_idx << std::endl;
        
        // ifobjectyesvaluewhilenotyespointer，needFirststoragetostackup/above
        llvm::Value* object_ptr = object;
        if (!object->getType()->isPointerTy()) {
            std::cerr << "[MemberExpr] Object is value, creating temp alloca" << std::endl;
            llvm::AllocaInst* temp = builder.CreateAlloca(object->getType(), nullptr, "struct.tmp");
            builder.CreateStore(object, temp);
            object_ptr = temp;
        }
        
        // useCreateStructGEPvisitfield
        llvm::Value* field_ptr = builder.CreateStructGEP(
            llvm_struct_type,
            object_ptr,
            field_idx,
            member
        );
        
        std::cerr << "[MemberExpr] Got field pointer" << std::endl;
        
        // Loadfieldvalue
        llvm::Type* field_type = context_->getLLVMType(fields[field_idx].second);
        results_ = builder.CreateLoad(field_type, field_ptr);
        
    } else {
        results_ = nullptr;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Matchexpressionandpattern matching - moved to match_codegen.cpp
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// visit(MatchExpr*)、generatePatternMatch()、bindPatternVariables()
// with/toandAllPatternrelatedmethodmoved tomatch_codegen.cpp

void ExprCodeGen::visit(IndexExpr* node) {
    // array/sliceindex: arr[i]
    node->getObject()->accept(this);
    llvm::Value* array = results_;
    
    node->getIndex()->accept(this);
    llvm::Value* index = results_;
    
    if (!array || !index) {
        results_ = nullptr;
        return;
    }
    
    auto& builder = context_->getBuilder();
    llvm::Value* array_ptr = array;
    llvm::Type* array_type = array->getType();
    
    // ifarrayyesvaluewhilenotyespointer，needFirststoragetostackup/above
    if (!array_type->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(array_type, nullptr, "array.tmp");
        builder.CreateStore(array, temp);
        array_ptr = temp;
        // array_typemaintain/keephold/maintainnotvariable，used forGEP
    } else {
        // ifyespointer，getpointing toof/thetypes
        // right/correctat/inopaquepointer，array_typealreadyyescorrectof/the
    }
    
    // generationGEP
    llvm::Value* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    
    llvm::Value* elem_ptr = builder.CreateGEP(
        array_type,
        array_ptr,
        {zero, index},
        "arrayidx"
    );
    
    // Loadelementvalue
    Type* elem_type = node->getType();
    if (elem_type) {
        llvm::Type* llvm_elem_type = context_->getLLVMType(elem_type);
        results_ = builder.CreateLoad(llvm_elem_type, elem_ptr);
    } else {
        results_ = nullptr;
    }
}

void ExprCodeGen::visit(IfExpr* node) {
    // ifexpression: if cond { then_expr } else { else_expr }
    llvm::Function* func = context_->getBuilder().GetInsertBlock()->getParent();
    
    // generationcondition
    node->getCondition()->accept(this);
    llvm::Value* cond = results_;
    
    // Create basic block
    llvm::BasicBlock* then_bb = context_->createBasicBlock("if.then", func);
    llvm::BasicBlock* else_bb = context_->createBasicBlock("if.else", func);
    llvm::BasicBlock* merge_bb = context_->createBasicBlock("if.end", func);
    
    // createbranch
    context_->getBuilder().CreateCondBr(cond, then_bb, else_bb);
    
    // generationthenbranch
    context_->getBuilder().SetInsertPoint(then_bb);
    node->getThenExpr()->accept(this);
    llvm::Value* then_val = results_;
    llvm::BasicBlock* then_end_bb = context_->getCurrentBlock();
    if (!then_end_bb->getTerminator()) {
        context_->getBuilder().CreateBr(merge_bb);
    }
    
    // generationelsebranch
    context_->getBuilder().SetInsertPoint(else_bb);
    llvm::Value* else_val = nullptr;
    if (node->getElseExpr()) {
        node->getElseExpr()->accept(this);
        else_val = results_;
    }
    llvm::BasicBlock* else_end_bb = context_->getCurrentBlock();
    if (!else_end_bb->getTerminator()) {
        context_->getBuilder().CreateBr(merge_bb);
    }
    
    // mergeblock - usePHInode
    context_->getBuilder().SetInsertPoint(merge_bb);
    
    if (then_val && else_val && then_val->getType() == else_val->getType()) {
        llvm::PHINode* phi = context_->getBuilder().CreatePHI(
            then_val->getType(), 2, "iftmp");
        phi->addIncoming(then_val, then_end_bb);
        phi->addIncoming(else_val, else_end_bb);
        results_ = phi;
    } else {
        results_ = nullptr;
    }
}

void ExprCodeGen::visit(BlockExpr* node) {
    // blockexpression: { stmt1; stmt2; expr }
    context_->enterScope();
    
    const auto& stmts = node->getStmts();
    llvm::Value* last_value = nullptr;
    
    if (stmt_codegen_ && !stmts.empty()) {
        // generationAllstatement
        for (size_t i = 0; i < stmts.size(); ++i) {
            auto* stmt = stmts[i].get();
            
            // If last statement is ExprStmt, extract its value as block return value
            if (i == stmts.size() - 1) {
                if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
                    // lastone/aindividual/pieceyesexpressionstatement，generateitsvalue
                    if (expr_stmt->getExpr()) {
                        last_value = generate(expr_stmt->getExpr());
                    }
                    continue;
                }
            }
            
            // Other typesstatementpositivenormallygenerate
            stmt_codegen_->generate(stmt);
        }
    }
    
    context_->exitScope();
    results_ = last_value;
}

void ExprCodeGen::visit(ArrayLiteral* node) {
    // arrayliteralcompletecode generation [1, 2, 3]
    auto& builder = context_->getBuilder();
    
    Type* array_type = node->getType();
    if (!array_type) {
        results_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_array_type = context_->getLLVMType(array_type);
    if (!llvm_array_type) {
        results_ = nullptr;
        return;
    }
    
    // emptyarray
    if (node->getElements().empty()) {
        results_ = llvm::ConstantAggregateZero::get(llvm_array_type);
        return;
    }
    
    // in/atstackup/aboveallocatearray
    llvm::AllocaInst* array_alloca = builder.CreateAlloca(llvm_array_type, nullptr, "array.tmp");
    
    // initializeeach/everyelement
    size_t idx = 0;
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
        if (!results_) {
            results_ = nullptr;
            return;
        }
        
        // GEPcomputeelementpointer
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0),
            llvm::ConstantInt::get(builder.getInt32Ty(), idx++)
        };
        llvm::Value* elem_ptr = builder.CreateGEP(llvm_array_type, array_alloca, indices, "array.elem.ptr");
        builder.CreateStore(results_, elem_ptr);
    }
    
    // Load entire array
    results_ = builder.CreateLoad(llvm_array_type, array_alloca, "array");
}

void ExprCodeGen::visit(TupleExpr* node) {
    // tuplecompletecode generation (1, "hello", true)
    auto& builder = context_->getBuilder();
    
    Type* tuple_type = node->getType();
    if (!tuple_type) {
        results_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_tuple_type = context_->getLLVMType(tuple_type);
    if (!llvm_tuple_type || !llvm_tuple_type->isStructTy()) {
        results_ = nullptr;
        return;
    }
    
    llvm::StructType* struct_type = llvm::cast<llvm::StructType>(llvm_tuple_type);
    
    // emptytuple
    if (node->getElements().empty()) {
        results_ = llvm::UndefValue::get(struct_type);
        return;
    }
    
    // in/atstackup/aboveallocatetuple
    llvm::AllocaInst* tuple_alloca = builder.CreateAlloca(struct_type, nullptr, "tuple.tmp");
    
    // initializeeach/everyfield
    for (size_t i = 0; i < node->getElements().size(); ++i) {
        node->getElements()[i]->accept(this);
        if (!results_) {
            results_ = nullptr;
            return;
        }
        
        llvm::Value* field_ptr = builder.CreateStructGEP(struct_type, tuple_alloca, i, "tuple.field.ptr");
        builder.CreateStore(results_, field_ptr);
    }
    
    // Load entire tuple
    results_ = builder.CreateLoad(struct_type, tuple_alloca, "tuple");
}

void ExprCodeGen::visit(RangeExpr* node) {
    // Rangeexpressioncompletecode generation 0..10
    auto& builder = context_->getBuilder();
    
    llvm::Value* start_val = nullptr;
    llvm::Value* end_val = nullptr;
    
    // generationstartvalue
    if (node->getStart()) {
        node->getStart()->accept(this);
        start_val = results_;
    } else {
        start_val = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // generationendvalue
    if (node->getEnd()) {
        node->getEnd()->accept(this);
        end_val = results_;
    } else {
        end_val = llvm::ConstantInt::get(builder.getInt32Ty(), INT32_MAX);
    }
    
    if (!start_val || !end_val) {
        results_ = nullptr;
        return;
    }
    
    // createRangestructbody/struct {start, end, inclusive}
    llvm::Type* i32_type = builder.getInt32Ty();
    llvm::Type* i1_type = builder.getInt1Ty();
    llvm::StructType* range_struct = llvm::StructType::get(
        builder.getContext(),
        {i32_type, i32_type, i1_type},
        false
    );
    
    llvm::AllocaInst* range_alloca = builder.CreateAlloca(range_struct, nullptr, "range.tmp");
    
    // initializestart
    llvm::Value* start_ptr = builder.CreateStructGEP(range_struct, range_alloca, 0, "range.start.ptr");
    builder.CreateStore(start_val, start_ptr);
    
    // initializeend
    llvm::Value* end_ptr = builder.CreateStructGEP(range_struct, range_alloca, 1, "range.end.ptr");
    builder.CreateStore(end_val, end_ptr);
    
    // initializeinclusiveflag
    llvm::Value* inclusive_ptr = builder.CreateStructGEP(range_struct, range_alloca, 2, "range.inclusive.ptr");
    llvm::Value* inclusive_val = llvm::ConstantInt::get(i1_type, node->isInclusive() ? 1 : 0);
    builder.CreateStore(inclusive_val, inclusive_ptr);
    
    // Load Range struct
    results_ = builder.CreateLoad(range_struct, range_alloca, "range");
}

void ExprCodeGen::visit(StructLiteral* node) {
    // structbody/structliteralcompletecode generation Point { x: 10, y: 20 }
    auto& builder = context_->getBuilder();
    
    Type* struct_type = node->getType();
    if (!struct_type || struct_type->getKind() != Type::Kind::Struct) {
        results_ = nullptr;
        return;
    }
    
    // useCodeGenContextof/thetypesmapensureconsistency
    llvm::Type* llvm_struct_type = context_->getLLVMType(struct_type);
    if (!llvm_struct_type || !llvm_struct_type->isStructTy()) {
        results_ = nullptr;
        return;
    }
    
    llvm::StructType* llvm_st = llvm::cast<llvm::StructType>(llvm_struct_type);
    StructType* paw_st = static_cast<StructType*>(struct_type);
    const auto& struct_fields = paw_st->getFields();
    
    // in/atstackup/aboveallocatestructbody/struct
    llvm::AllocaInst* struct_alloca = builder.CreateAlloca(llvm_st, nullptr, "struct.tmp");
    
    // is/aseach/everyfieldgenerateinitializecode
    for (auto& field_init : node->getFields()) {
        // foundfieldin/atstructbodyof/theindex
        size_t field_index = 0;
        bool found = false;
        
        for (size_t i = 0; i < struct_fields.size(); ++i) {
            if (struct_fields[i].first == field_init.name) {
                field_index = i;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // TypeChecker should already catch this error
            continue;
        }
        
        // generationfieldvalue
        field_init.value->accept(this);
        if (!results_) {
            results_ = nullptr;
            return;
        }
        
        // getfieldpointerandstorage
        llvm::Value* field_ptr = builder.CreateStructGEP(
            llvm_st, 
            struct_alloca, 
            field_index, 
            "struct.field." + field_init.name
        );
        builder.CreateStore(results_, field_ptr);
    }
    
    // Load entire struct
    results_ = builder.CreateLoad(llvm_st, struct_alloca, "struct");
}

void ExprCodeGen::visit(CastExpr* node) {
    // astypesconvert: expr as TargetType
    node->getExpr()->accept(this);
    llvm::Value* source_value = results_;
    
    if (!source_value) {
        results_ = nullptr;
        return;
    }
    
    auto& builder = context_->getBuilder();
    
    Type* source_type = node->getExpr()->getType();
    Type* target_type = node->getTargetType();
    
    if (!source_type || !target_type) {
        results_ = nullptr;
        return;
    }
    
    llvm::Type* llvm_target_type = context_->getLLVMType(target_type);
    
    // numbervaluetypesconvert
    if (source_type->isInteger() && target_type->isInteger()) {
        // integerbetweenof/theconvert
        llvm::Type* source_llvm = source_value->getType();
        unsigned source_bits = source_llvm->getIntegerBitWidth();
        unsigned target_bits = llvm_target_type->getIntegerBitWidth();
        
        if (source_bits < target_bits) {
            // extend
            if (source_type->isSignedInteger()) {
                results_ = builder.CreateSExt(source_value, llvm_target_type, "cast.sext");
            } else {
                results_ = builder.CreateZExt(source_value, llvm_target_type, "cast.zext");
            }
        } else if (source_bits > target_bits) {
            // truncate
            results_ = builder.CreateTrunc(source_value, llvm_target_type, "cast.trunc");
        } else {
            // bitssame，directlyuse
            results_ = source_value;
        }
    }
    else if (source_type->isInteger() && target_type->isFloat()) {
        // integertofloating-point
        if (source_type->isSignedInteger()) {
            results_ = builder.CreateSIToFP(source_value, llvm_target_type, "cast.sitofp");
        } else {
            results_ = builder.CreateUIToFP(source_value, llvm_target_type, "cast.uitofp");
        }
    }
    else if (source_type->isFloat() && target_type->isInteger()) {
        // floating-pointtointeger（truncate）
        if (target_type->isSignedInteger()) {
            results_ = builder.CreateFPToSI(source_value, llvm_target_type, "cast.fptosi");
        } else {
            results_ = builder.CreateFPToUI(source_value, llvm_target_type, "cast.fptoui");
        }
    }
    else if (source_type->isFloat() && target_type->isFloat()) {
        // floating-pointbetweenof/theconvert
        unsigned source_bits = source_value->getType()->getScalarSizeInBits();
        unsigned target_bits = llvm_target_type->getScalarSizeInBits();
        
        if (source_bits < target_bits) {
            // extend
            results_ = builder.CreateFPExt(source_value, llvm_target_type, "cast.fpext");
        } else if (source_bits > target_bits) {
            // truncate
            results_ = builder.CreateFPTrunc(source_value, llvm_target_type, "cast.fptrunc");
        } else {
            results_ = source_value;
        }
    }
    else {
        // notsupportof/thetypesconvert
        results_ = nullptr;
    }
}

} // namespace pawc
