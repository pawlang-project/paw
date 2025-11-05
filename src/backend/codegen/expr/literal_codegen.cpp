//===--- literal_codegen.cpp - Literal CodeGen Implementation ---*- C++ -*-===//
/// @file literal_codegen.cpp
/// @brief Code generation implementation
//
// literalcode generation：IntLiteral, FloatLiteral, BoolLiteral, CharLiteral, StringLiteral
// fromexpr_codegen.cppmiddle/centerextract
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "../type/type_codegen.h"
#include <llvm/IR/Constants.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <iostream>

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// literalgenerate
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(IntLiteral* node) {
    // useAPIntparseintegervalue（supportarbitrary precision，packageincludingi128/u128）
    const std::string& value_str = node->getValue();
    
    // Checkyesnoyesnegativenumber
    bool is_negative = (!value_str.empty() && value_str[0] == '-');
    std::string abs_value_str = is_negative ? value_str.substr(1) : value_str;
    
    // according totypescertain/surebit/digitwide
    Type* type = node->getType();
    unsigned bit_width = 64;  // defaulti32actualuse64bit/digitparse
    
    if (type) {
        switch (type->getKind()) {
            case Type::Kind::I8:
            case Type::Kind::U8:
                bit_width = 8;
                break;
            case Type::Kind::I16:
            case Type::Kind::U16:
                bit_width = 16;
                break;
            case Type::Kind::I32:
            case Type::Kind::U32:
                bit_width = 32;
                break;
            case Type::Kind::I64:
            case Type::Kind::U64:
                bit_width = 64;
                break;
            case Type::Kind::I128:
            case Type::Kind::U128:
                bit_width = 128;  // complete128bit/digitprecision
                break;
            default:
                bit_width = 32;
                break;
        }
    } else {
        // defaultis/asi32
        bit_width = 32;
    }
    
    // useAPIntparse（supportarbitrary/anylarge/biginteger）
    llvm::APInt ap_value(bit_width, abs_value_str, 10);
    
    // ifyesnegativenumber，negate
    if (is_negative) {
        ap_value = -ap_value;
    }
    
    // createLLVMconstant
    llvm::Type* llvm_type = type ? context_->getLLVMType(type) : nullptr;
    
    if (!llvm_type || !llvm_type->isIntegerTy()) {
        llvm_type = llvm::Type::getInt32Ty(context_->getLLVMContext());
    }
    
    results_ = llvm::ConstantInt::get(llvm_type, ap_value);
}

void ExprCodeGen::visit(FloatLiteral* node) {
    // useAPFloatsupportcompleteprecisionfloating-pointnumber
    const std::string& value_str = node->getValue();
    Type* type = node->getType();
    
    // f128：generatetruepositiveof/thefp128constant（supportoperation）
    if (type && type->getKind() == Type::Kind::F128) {
        // useAPFloatparseis/asIEEE 754 fp128
        llvm::APFloat ap_value(llvm::APFloat::IEEEquad(), value_str);
        
        // generationfp128constant（whilenotyescharacterstring）
        results_ = llvm::ConstantFP::get(
            llvm::Type::getFP128Ty(context_->getLLVMContext()),
            ap_value
        );
        return;
    }
    
    // Other typesfloating-pointtypes：positivenormallyprocess
    llvm::Type* llvm_type = type ? context_->getLLVMType(type) : nullptr;
    
    // ifnohastypesortypesnotyesfloating-point，defaultf64
    if (!llvm_type || (!llvm_type->isFloatingPointTy() && !llvm_type->isStructTy())) {
        llvm_type = llvm::Type::getDoubleTy(context_->getLLVMContext());
    }
    
    // useAPFloatparse（completeprecision）
    llvm::APFloat ap_value(llvm_type->getFltSemantics(), value_str);
    
    results_ = llvm::ConstantFP::get(llvm_type, ap_value);
}

void ExprCodeGen::visit(BoolLiteral* node) {
    results_ = llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(context_->getLLVMContext()),
        node->getValue() ? 1 : 0
    );
}

void ExprCodeGen::visit(CharLiteral* node) {
    results_ = llvm::ConstantInt::get(
        llvm::Type::getInt8Ty(context_->getLLVMContext()),
        static_cast<uint8_t>(node->getValue())
    );
}

void ExprCodeGen::visit(StringLiteral* node) {
    // Create global character string constant
    results_ = context_->getBuilder().CreateGlobalString(node->getValue());
}

} // namespace pawc

