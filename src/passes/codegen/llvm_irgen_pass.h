//===--- llvm_irgen_pass.h - LLVM IR Generation Pass -------------*- C++ -*-===//

#ifndef PAW_LLVM_IRGEN_PASS_H
#define PAW_LLVM_IRGEN_PASS_H

#include "pass/pass.h"
#include "backend/codegen/codegen_context.h"
#include "backend/codegen/expr/expr_codegen.h"
#include "backend/codegen/stmt/stmt_codegen.h"
#include "backend/builtins/builtin_codegen.h"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>

namespace pawc {

/// LLVMIRGenPass - LLVM IR生成Pass
///
/// 遍历AST并生成LLVM IR
class LLVMIRGenPass : public PassBase<LLVMIRGenPass> {
public:
    static std::string name() { return "LLVMIRGenPass"; }
    
    PassResult runImpl(PassContext* context) {
        // 获取AST
        auto* ast = context->getAST();
        if (!ast || ast->empty()) {
            return PassResult{false, "No AST available for code generation"};
        }
        
        // 创建CodeGen上下文（使用shared_ptr保持生命周期）
        auto codegen_ctx = std::make_shared<CodeGenContext>(
            "main_module",
            context->getTypeSystem(),
            context->getSymbolTable()
        );
        
        // 声明基础runtime函数
        codegen_ctx->declareRuntimeFunctions();
        
        // 注册所有builtin函数（print, println, to_string等，共58个）
        // codegen_ctx->registerAllBuiltinFunctions(); // 暂时注释掉测试
        
        // 创建表达式和语句代码生成器
        ExprCodeGen expr_gen(codegen_ctx.get());
        StmtCodeGen stmt_gen(codegen_ctx.get(), &expr_gen);
        
        // 遍历AST生成IR
        for (const auto& stmt : *ast) {
            stmt_gen.generate(stmt.get());
        }
        
        // 验证生成的模块
        std::string error_msg;
        llvm::raw_string_ostream error_stream(error_msg);
        
        if (llvm::verifyModule(*codegen_ctx->getModule(), &error_stream)) {
            return PassResult{false, "Invalid LLVM IR: " + error_msg};
        }
        
        // 缓存整个CodeGenContext（保持生命周期）
        context->cacheAnalysisResult("codegen_context", codegen_ctx);
        
        // 同时缓存Module指针供快速访问
        context->cacheAnalysisResult("llvm_module", codegen_ctx->getModule());
        
        // 输出LLVM IR（如果verbose）
        if (context->isVerbose()) {
            codegen_ctx->dump();
        }
        
        return PassResult{true, "LLVM IR generated and verified successfully"};
    }
};

} // namespace pawc

#endif // PAW_LLVM_IRGEN_PASS_H

