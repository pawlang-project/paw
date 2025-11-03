//===--- literal_codegen.cpp - Literal CodeGen Implementation ---*- C++ -*-===//
//
// 字面量代码生成：IntLiteral, FloatLiteral, BoolLiteral, CharLiteral, StringLiteral
// 从expr_codegen.cpp中提取
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
// 字面量生成
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(IntLiteral* node) {
    // 使用APInt解析整数值（支持任意精度，包括i128/u128）
    const std::string& value_str = node->getValue();
    
    // 检查是否是负数
    bool is_negative = (!value_str.empty() && value_str[0] == '-');
    std::string abs_value_str = is_negative ? value_str.substr(1) : value_str;
    
    // 根据类型确定位宽
    Type* type = node->getType();
    unsigned bit_width = 64;  // 默认i32实际用64位解析
    
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
                bit_width = 128;  // 完整128位精度
                break;
            default:
                bit_width = 32;
                break;
        }
    } else {
        // 默认为i32
        bit_width = 32;
    }
    
    // 使用APInt解析（支持任意大整数）
    llvm::APInt ap_value(bit_width, abs_value_str, 10);
    
    // 如果是负数，取反
    if (is_negative) {
        ap_value = -ap_value;
    }
    
    // 创建LLVM常量
    llvm::Type* llvm_type = type ? context_->getLLVMType(type) : nullptr;
    
    if (!llvm_type || !llvm_type->isIntegerTy()) {
        llvm_type = llvm::Type::getInt32Ty(context_->getLLVMContext());
    }
    
    result_ = llvm::ConstantInt::get(llvm_type, ap_value);
}

void ExprCodeGen::visit(FloatLiteral* node) {
    // 使用APFloat支持完整精度浮点数
    const std::string& value_str = node->getValue();
    Type* type = node->getType();
    
    // f128：生成真正的fp128常量（支持运算）
    if (type && type->getKind() == Type::Kind::F128) {
        // 使用APFloat解析为IEEE 754 fp128
        llvm::APFloat ap_value(llvm::APFloat::IEEEquad(), value_str);
        
        // 生成fp128常量（而不是字符串）
        result_ = llvm::ConstantFP::get(
            llvm::Type::getFP128Ty(context_->getLLVMContext()),
            ap_value
        );
        return;
    }
    
    // 其他浮点类型：正常处理
    llvm::Type* llvm_type = type ? context_->getLLVMType(type) : nullptr;
    
    // 如果没有类型或类型不是浮点，默认f64
    if (!llvm_type || (!llvm_type->isFloatingPointTy() && !llvm_type->isStructTy())) {
        llvm_type = llvm::Type::getDoubleTy(context_->getLLVMContext());
    }
    
    // 使用APFloat解析（完整精度）
    llvm::APFloat ap_value(llvm_type->getFltSemantics(), value_str);
    
    result_ = llvm::ConstantFP::get(llvm_type, ap_value);
}

void ExprCodeGen::visit(BoolLiteral* node) {
    result_ = llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(context_->getLLVMContext()),
        node->getValue() ? 1 : 0
    );
}

void ExprCodeGen::visit(CharLiteral* node) {
    result_ = llvm::ConstantInt::get(
        llvm::Type::getInt8Ty(context_->getLLVMContext()),
        static_cast<uint8_t>(node->getValue())
    );
}

void ExprCodeGen::visit(StringLiteral* node) {
    // 创建全局字符串常量
    result_ = context_->getBuilder().CreateGlobalStringPtr(node->getValue());
}

} // namespace pawc

