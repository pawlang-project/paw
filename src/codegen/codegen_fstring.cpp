/**
 * @file codegen_fstring.cpp
 * @brief F-String (字符串插值) 代码生成实现
 */

#include "codegen.h"
#include "../parser/ast.h"
#include <sstream>
#include <iostream>

namespace pawc {

llvm::Value* CodeGenerator::generateFStringExpr(const FStringExpr* expr) {
    // f"Hello, {name}! Age: {age}"
    // 转换为：
    // "Hello, " + to_string(name) + "! Age: " + to_string(age)
    
    // Debug: 打印parts和expressions信息
    std::cerr << "[DEBUG] FStringExpr: parts=" << expr->parts.size() 
              << ", expressions=" << expr->expressions.size() << std::endl;
    for (size_t i = 0; i < expr->parts.size(); ++i) {
        std::cerr << "  part[" << i << "]: [" << expr->parts[i] << "]" << std::endl;
    }
    
    llvm::Value* result = nullptr;
    
    for (size_t i = 0; i < expr->parts.size(); ++i) {
        // 添加字符串片段
        if (!expr->parts[i].empty()) {
            std::cerr << "[DEBUG] Creating string constant for part " << i << std::endl;
            llvm::Value* part = createStringConstant(expr->parts[i]);
            std::cerr << "[DEBUG] Got part, result=" << (void*)result << ", part=" << (void*)part << std::endl;
            result = result ? concatenateStrings(result, part) : part;
            std::cerr << "[DEBUG] After assign, result=" << (void*)result << std::endl;
        }
        
        // 添加表达式（如果有）
        if (i < expr->expressions.size()) {
            llvm::Value* expr_val = generateExpr(expr->expressions[i].get());
            if (!expr_val) {
                std::cerr << "Error: Failed to generate expression in f-string\n";
                return createStringConstant("");
            }
            
            // 转换为字符串（类型推断会在generateExpr中完成）
            // 暂时简单处理：根据LLVM类型判断
            llvm::Type* llvm_type = expr_val->getType();
            llvm::Value* str_val = nullptr;
            
            if (llvm_type->isIntegerTy(32)) {
                str_val = intToString(expr_val);
            } else if (llvm_type->isDoubleTy()) {
                str_val = floatToString(expr_val);
            } else if (llvm_type->isIntegerTy(1)) {
                // bool
                str_val = builder_->CreateSelect(
                    expr_val,
                    createStringConstant("true"),
                    createStringConstant("false")
                );
            } else if (llvm_type->isIntegerTy(8)) {
                // char
                str_val = charToString(expr_val);
            } else if (llvm_type->isPointerTy()) {
                // 假设是字符串
                str_val = expr_val;
            } else {
                str_val = createStringConstant("<value>");
            }
            
            result = result ? concatenateStrings(result, str_val) : str_val;
        }
    }
    
    return result ? result : createStringConstant("");
}

// 辅助函数：创建字符串常量
llvm::Value* CodeGenerator::createStringConstant(const std::string& str) {
    // 创建全局字符串
    llvm::Constant* strConstant = llvm::ConstantDataArray::getString(*context_, str);
    llvm::GlobalVariable* globalStr = new llvm::GlobalVariable(
        *module_,
        strConstant->getType(),
        true,  // isConstant
        llvm::GlobalValue::PrivateLinkage,
        strConstant,
        ".str"
    );
    
    // 返回指针
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0);
    std::vector<llvm::Value*> indices = {zero, zero};
    
    return builder_->CreateInBoundsGEP(
        globalStr->getValueType(),
        globalStr,
        indices,
        "str"
    );
}

// 辅助函数：拼接两个字符串
llvm::Value* CodeGenerator::concatenateStrings(llvm::Value* left, llvm::Value* right) {
    // 调用内置的字符串拼接函数
    // 先简单实现：调用 C 的 strcat 或自定义函数
    
    llvm::Type* i8PtrTy = llvm::PointerType::get(*context_, 0);
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*context_);
    
    // 1. 计算长度
    llvm::Function* strlen_func = module_->getFunction("strlen");
    if (!strlen_func) {
        llvm::FunctionType* strlen_type = llvm::FunctionType::get(
            i64Ty,
            {i8PtrTy},
            false
        );
        strlen_func = llvm::Function::Create(
            strlen_type,
            llvm::Function::ExternalLinkage,
            "strlen",
            module_.get()
        );
    }
    
    llvm::Value* len1 = builder_->CreateCall(strlen_func, {left});
    llvm::Value* len2 = builder_->CreateCall(strlen_func, {right});
    llvm::Value* total_len = builder_->CreateAdd(len1, len2);
    llvm::Value* alloc_len = builder_->CreateAdd(
        total_len,
        llvm::ConstantInt::get(i64Ty, 1)  // +1 for null terminator
    );
    
    // 2. 分配内存
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        llvm::FunctionType* malloc_type = llvm::FunctionType::get(
            i8PtrTy,
            {i64Ty},
            false
        );
        malloc_func = llvm::Function::Create(
            malloc_type,
            llvm::Function::ExternalLinkage,
            "malloc",
            module_.get()
        );
    }
    
    llvm::Value* result_buf = builder_->CreateCall(malloc_func, {alloc_len});
    
    // 3. 拷贝字符串
    llvm::Function* strcpy_func = module_->getFunction("strcpy");
    if (!strcpy_func) {
        llvm::FunctionType* strcpy_type = llvm::FunctionType::get(
            i8PtrTy,
            {i8PtrTy, i8PtrTy},
            false
        );
        strcpy_func = llvm::Function::Create(
            strcpy_type,
            llvm::Function::ExternalLinkage,
            "strcpy",
            module_.get()
        );
    }
    
    llvm::Function* strcat_func = module_->getFunction("strcat");
    if (!strcat_func) {
        llvm::FunctionType* strcat_type = llvm::FunctionType::get(
            i8PtrTy,
            {i8PtrTy, i8PtrTy},
            false
        );
        strcat_func = llvm::Function::Create(
            strcat_type,
            llvm::Function::ExternalLinkage,
            "strcat",
            module_.get()
        );
    }
    
    builder_->CreateCall(strcpy_func, {result_buf, left});
    builder_->CreateCall(strcat_func, {result_buf, right});
    
    return result_buf;
}

// 类型转字符串
llvm::Value* CodeGenerator::convertToString(llvm::Value* value, Type* type) {
    if (!type) {
        return createStringConstant("<unknown>");
    }
    
    switch (type->kind) {
        case Type::Kind::Primitive: {
            auto* prim = static_cast<PrimitiveTypeNode*>(type);
            
            // i32 -> string
            if (prim->prim_type == PrimitiveType::I32) {
                return intToString(value);
            }
            
            // f64 -> string
            if (prim->prim_type == PrimitiveType::F64) {
                return floatToString(value);
            }
            
            // bool -> string
            if (prim->prim_type == PrimitiveType::BOOL) {
                return builder_->CreateSelect(
                    value,
                    createStringConstant("true"),
                    createStringConstant("false")
                );
            }
            
            // char -> string
            if (prim->prim_type == PrimitiveType::CHAR) {
                return charToString(value);
            }
            
            break;
        }
        
        case Type::Kind::Named: {
            auto* named = static_cast<NamedTypeNode*>(type);
            if (named->name == "string") {
                // 已经是字符串
                return value;
            }
            break;
        }
        
        default:
            break;
    }
    
    // 其他类型：默认表示
    return createStringConstant("<value>");
}

// i32 -> string
llvm::Value* CodeGenerator::intToString(llvm::Value* value) {
    llvm::Type* i8PtrTy = llvm::PointerType::get(*context_, 0);
    llvm::Type* i32Ty = llvm::Type::getInt32Ty(*context_);
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*context_);
    
    // 分配足够的缓冲区（最多 32 字符）
    llvm::Value* buf_size = llvm::ConstantInt::get(i64Ty, 32);
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        llvm::FunctionType* malloc_type = llvm::FunctionType::get(
            i8PtrTy,
            {i64Ty},
            false
        );
        malloc_func = llvm::Function::Create(
            malloc_type,
            llvm::Function::ExternalLinkage,
            "malloc",
            module_.get()
        );
    }
    
    llvm::Value* buf = builder_->CreateCall(malloc_func, {buf_size});
    
    // 调用 snprintf
    llvm::Function* snprintf_func = module_->getFunction("snprintf");
    if (!snprintf_func) {
        llvm::FunctionType* snprintf_type = llvm::FunctionType::get(
            i32Ty,
            {i8PtrTy, i64Ty, i8PtrTy},
            true  // varargs
        );
        snprintf_func = llvm::Function::Create(
            snprintf_type,
            llvm::Function::ExternalLinkage,
            "snprintf",
            module_.get()
        );
    }
    
    llvm::Value* format = createStringConstant("%d");
    builder_->CreateCall(snprintf_func, {buf, buf_size, format, value});
    
    return buf;
}

// f64 -> string
llvm::Value* CodeGenerator::floatToString(llvm::Value* value) {
    llvm::Type* i8PtrTy = llvm::PointerType::get(*context_, 0);
    llvm::Type* i32Ty = llvm::Type::getInt32Ty(*context_);
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*context_);
    
    // 分配足够的缓冲区（最多 64 字符）
    llvm::Value* buf_size = llvm::ConstantInt::get(i64Ty, 64);
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        llvm::FunctionType* malloc_type = llvm::FunctionType::get(
            i8PtrTy,
            {i64Ty},
            false
        );
        malloc_func = llvm::Function::Create(
            malloc_type,
            llvm::Function::ExternalLinkage,
            "malloc",
            module_.get()
        );
    }
    
    llvm::Value* buf = builder_->CreateCall(malloc_func, {buf_size});
    
    // 调用 snprintf
    llvm::Function* snprintf_func = module_->getFunction("snprintf");
    if (!snprintf_func) {
        llvm::FunctionType* snprintf_type = llvm::FunctionType::get(
            i32Ty,
            {i8PtrTy, i64Ty, i8PtrTy},
            true  // varargs
        );
        snprintf_func = llvm::Function::Create(
            snprintf_type,
            llvm::Function::ExternalLinkage,
            "snprintf",
            module_.get()
        );
    }
    
    llvm::Value* format = createStringConstant("%g");  // %g for compact float repr
    builder_->CreateCall(snprintf_func, {buf, buf_size, format, value});
    
    return buf;
}

// char -> string
llvm::Value* CodeGenerator::charToString(llvm::Value* value) {
    llvm::Type* i8PtrTy = llvm::PointerType::get(*context_, 0);
    llvm::Type* i8Ty = llvm::Type::getInt8Ty(*context_);
    llvm::Type* i64Ty = llvm::Type::getInt64Ty(*context_);
    
    // 分配 2 字节（字符 + null terminator）
    llvm::Value* buf_size = llvm::ConstantInt::get(i64Ty, 2);
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        llvm::FunctionType* malloc_type = llvm::FunctionType::get(
            i8PtrTy,
            {i64Ty},
            false
        );
        malloc_func = llvm::Function::Create(
            malloc_type,
            llvm::Function::ExternalLinkage,
            "malloc",
            module_.get()
        );
    }
    
    llvm::Value* buf = builder_->CreateCall(malloc_func, {buf_size});
    
    // buf[0] = char
    llvm::Value* zero = llvm::ConstantInt::get(i64Ty, 0);
    llvm::Value* one = llvm::ConstantInt::get(i64Ty, 1);
    
    llvm::Value* ptr0 = builder_->CreateGEP(i8Ty, buf, zero);
    builder_->CreateStore(value, ptr0);
    
    // buf[1] = '\0'
    llvm::Value* ptr1 = builder_->CreateGEP(i8Ty, buf, one);
    llvm::Value* null_term = llvm::ConstantInt::get(i8Ty, 0);
    builder_->CreateStore(null_term, ptr1);
    
    return buf;
}

} // namespace pawc

