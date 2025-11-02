//===--- binary_codegen.cpp - Binary/Unary Expr CodeGen ---------*- C++ -*-===//
//
// 二元和一元运算代码生成
// 从expr_codegen.cpp中提取
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "frontend/lexer/token.h"

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 二元运算
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(BinaryExpr* node) {
    // 生成左右操作数
    node->getLeft()->accept(this);
    llvm::Value* left = result_;
    
    node->getRight()->accept(this);
    llvm::Value* right = result_;
    
    if (!left || !right) {
        result_ = nullptr;
        return;
    }
    
    result_ = generateBinaryOp(node->getOperator(), left, right,
                               node->getLeft()->getType());
}

llvm::Value* ExprCodeGen::generateBinaryOp(TokenType op, llvm::Value* left,
                                           llvm::Value* right, Type* type) {
    auto& builder = context_->getBuilder();
    
    if (type->isInteger()) {
        switch (op) {
            case TokenType::PLUS:   return builder.CreateAdd(left, right, "addtmp");
            case TokenType::MINUS:  return builder.CreateSub(left, right, "subtmp");
            case TokenType::STAR:   return builder.CreateMul(left, right, "multmp");
            case TokenType::SLASH:
                return type->isSignedInteger() ?
                    builder.CreateSDiv(left, right, "divtmp") :
                    builder.CreateUDiv(left, right, "divtmp");
            case TokenType::PERCENT:
                return type->isSignedInteger() ?
                    builder.CreateSRem(left, right, "modtmp") :
                    builder.CreateURem(left, right, "modtmp");
            
            // 比较运算
            case TokenType::EQ_EQ:       return builder.CreateICmpEQ(left, right, "eqtmp");
            case TokenType::NOT_EQ:      return builder.CreateICmpNE(left, right, "netmp");
            case TokenType::LESS:
                return type->isSignedInteger() ?
                    builder.CreateICmpSLT(left, right, "lttmp") :
                    builder.CreateICmpULT(left, right, "lttmp");
            case TokenType::LESS_EQ:
                return type->isSignedInteger() ?
                    builder.CreateICmpSLE(left, right, "letmp") :
                    builder.CreateICmpULE(left, right, "letmp");
            case TokenType::GREATER:
                return type->isSignedInteger() ?
                    builder.CreateICmpSGT(left, right, "gttmp") :
                    builder.CreateICmpUGT(left, right, "gttmp");
            case TokenType::GREATER_EQ:
                return type->isSignedInteger() ?
                    builder.CreateICmpSGE(left, right, "getmp") :
                    builder.CreateICmpUGE(left, right, "getmp");
            
            // 逻辑运算
            case TokenType::AND_AND: return builder.CreateAnd(left, right, "andtmp");
            case TokenType::OR_OR:   return builder.CreateOr(left, right, "ortmp");
            
            default: return nullptr;
        }
    } else if (type->isFloat()) {
        // 特殊处理f128：直接调用PawLang的运算函数（绕过LLVM ABI问题）
        if (type->getKind() == Type::Kind::F128) {
            std::string func_name;
            switch (op) {
                case TokenType::PLUS:       func_name = "paw_f128_add"; break;
                case TokenType::MINUS:      func_name = "paw_f128_sub"; break;
                case TokenType::STAR:       func_name = "paw_f128_mul"; break;
                case TokenType::SLASH:      func_name = "paw_f128_div"; break;
                
                // 比较运算仍使用LLVM指令（简单比较无ABI问题）
                case TokenType::EQ_EQ:      return builder.CreateFCmpOEQ(left, right, "feqtmp");
                case TokenType::NOT_EQ:     return builder.CreateFCmpONE(left, right, "fnetmp");
                case TokenType::LESS:       return builder.CreateFCmpOLT(left, right, "flttmp");
                case TokenType::LESS_EQ:    return builder.CreateFCmpOLE(left, right, "fletmp");
                case TokenType::GREATER:    return builder.CreateFCmpOGT(left, right, "fgttmp");
                case TokenType::GREATER_EQ: return builder.CreateFCmpOGE(left, right, "fgetmp");
                
                default: return nullptr;
            }
            
            // 声明并调用paw_f128_*函数（分解传递）
            llvm::Function* runtime_fn = context_->getRuntimeFunction(func_name);
            if (!runtime_fn) {
                llvm::Type* i64_ty = builder.getInt64Ty();
                llvm::Type* ptr_i64_ty = llvm::PointerType::getUnqual(builder.getContext());
                auto* fn_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {ptr_i64_ty, ptr_i64_ty,  // ret_low, ret_high
                     i64_ty, i64_ty,          // a_low, a_high
                     i64_ty, i64_ty},         // b_low, b_high
                    false
                );
                runtime_fn = llvm::Function::Create(
                    fn_type,
                    llvm::Function::ExternalLinkage,
                    func_name,
                    context_->getModule()
                );
                context_->registerRuntimeFunction(func_name, runtime_fn);
            }
            
            // 分解fp128为两个i64
            llvm::Value* left_i128 = builder.CreateBitCast(left, builder.getInt128Ty());
            llvm::Value* right_i128 = builder.CreateBitCast(right, builder.getInt128Ty());
            
            llvm::Value* a_low = builder.CreateTrunc(left_i128, builder.getInt64Ty());
            llvm::Value* a_high = builder.CreateTrunc(
                builder.CreateLShr(left_i128, llvm::ConstantInt::get(builder.getInt128Ty(), 64)),
                builder.getInt64Ty()
            );
            
            llvm::Value* b_low = builder.CreateTrunc(right_i128, builder.getInt64Ty());
            llvm::Value* b_high = builder.CreateTrunc(
                builder.CreateLShr(right_i128, llvm::ConstantInt::get(builder.getInt128Ty(), 64)),
                builder.getInt64Ty()
            );
            
            // 分配返回值空间
            llvm::Value* ret_low = builder.CreateAlloca(builder.getInt64Ty());
            llvm::Value* ret_high = builder.CreateAlloca(builder.getInt64Ty());
            
            // 调用
            builder.CreateCall(runtime_fn, {ret_low, ret_high, a_low, a_high, b_low, b_high});
            
            // 重新组装为fp128
            llvm::Value* res_low = builder.CreateLoad(builder.getInt64Ty(), ret_low);
            llvm::Value* res_high = builder.CreateLoad(builder.getInt64Ty(), ret_high);
            
            llvm::Value* res_i128 = builder.CreateOr(
                builder.CreateZExt(res_low, builder.getInt128Ty()),
                builder.CreateShl(
                    builder.CreateZExt(res_high, builder.getInt128Ty()),
                    llvm::ConstantInt::get(builder.getInt128Ty(), 64)
                )
            );
            
            return builder.CreateBitCast(res_i128, llvm::Type::getFP128Ty(builder.getContext()));
        }
        
        // 其他浮点类型：使用LLVM指令
        switch (op) {
            case TokenType::PLUS:       return builder.CreateFAdd(left, right, "faddtmp");
            case TokenType::MINUS:      return builder.CreateFSub(left, right, "fsubtmp");
            case TokenType::STAR:       return builder.CreateFMul(left, right, "fmultmp");
            case TokenType::SLASH:      return builder.CreateFDiv(left, right, "fdivtmp");
            
            // 比较运算
            case TokenType::EQ_EQ:      return builder.CreateFCmpOEQ(left, right, "feqtmp");
            case TokenType::NOT_EQ:     return builder.CreateFCmpONE(left, right, "fnetmp");
            case TokenType::LESS:       return builder.CreateFCmpOLT(left, right, "flttmp");
            case TokenType::LESS_EQ:    return builder.CreateFCmpOLE(left, right, "fletmp");
            case TokenType::GREATER:    return builder.CreateFCmpOGT(left, right, "fgttmp");
            case TokenType::GREATER_EQ: return builder.CreateFCmpOGE(left, right, "fgetmp");
            
            default: return nullptr;
        }
    }
    
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 一元运算
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(UnaryExpr* node) {
    node->getOperand()->accept(this);
    llvm::Value* operand = result_;
    
    if (!operand) {
        result_ = nullptr;
        return;
    }
    
    result_ = generateUnaryOp(node->getOperator(), operand,
                              node->getOperand()->getType());
}

llvm::Value* ExprCodeGen::generateUnaryOp(TokenType op, llvm::Value* operand,
                                          Type* type) {
    auto& builder = context_->getBuilder();
    
    switch (op) {
        case TokenType::MINUS:
            if (type->isInteger()) {
                return builder.CreateNeg(operand, "negtmp");
            } else if (type->isFloat()) {
                return builder.CreateFNeg(operand, "fnegtmp");
            }
            break;
        
        case TokenType::BANG:
            return builder.CreateNot(operand, "nottmp");
        
        default:
            break;
    }
    
    return nullptr;
}

} // namespace pawc

