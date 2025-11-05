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

/// LLVMIRGenPass - LLVM IRgeneratePass
///
/// Traverse AST and generate LLVM IR
class LLVMIRGenPass : public PassBase<LLVMIRGenPass> {
public:
    static std::string name() { return "LLVMIRGenPass"; }
    
    PassResult runImpl(PassContext* context) {
        // getAST
        auto* ast = context->getAST();
        if (!ast || ast->empty()) {
            return PassResult{false, "No AST available for code generation"};
        }
        
        // createCodeGenContext
        auto codegen_ctx = std::make_shared<CodeGenContext>(
            "main_module",
            context->getTypeSystem(),
            context->getSymbolTable()
        );
        
        // declarebaseruntimefunction
        codegen_ctx->declareRuntimeFunctions();
        
        // Register all builtin functions (print, println, to_string, etc., total 58)individual/piece）
        // codegen_ctx->registerAllBuiltinFunctions(); // Temporarily commented outcommentouttest
        
        // createexpressionandstatementcodegenerator
        ExprCodeGen expr_gen(codegen_ctx.get());
        StmtCodeGen stmt_gen(codegen_ctx.get(), &expr_gen);
        
        // 🔧 Two-pass scan: support function forward references
        // First pass: generate all function declarations (only signature, no body)notgeneratefunctionbody/struct）
        for (const auto& stmt : *ast) {
            if (auto* func = dynamic_cast<FunctionDecl*>(stmt.get())) {
                stmt_gen.generateFunctionDeclaration(func);
            }
        }
        
        // Second pass: generate all statements (including function bodies)
        for (const auto& stmt : *ast) {
            stmt_gen.generate(stmt.get());
        }
        
        // Validate generated module
        std::string error_msg;
        llvm::raw_string_ostream error_stream(error_msg);
        
        if (llvm::verifyModule(*codegen_ctx->getModule(), &error_stream)) {
            return PassResult{false, "Invalid LLVM IR: " + error_msg};
        }
        
        // outputLLVM IR（ifverbose）
        if (context->isVerbose()) {
            codegen_ctx->dump();
        }
        
        // 🔧 Bug Fix: must cache shared_ptr to maintain CodeGenContext lifetimenContextof/thelifetimeperiod
        // Otherwise CodeGenContext will be destroyed after runImpl ends, Module pointer will become invalidnt/effective
        context->cacheAnalysisResult("codegen_context", codegen_ctx);
        
        // Also cache Module raw pointer for fast access
        context->cacheAnalysisResult("llvm_module", codegen_ctx->getModule());
        
        return PassResult{true, "LLVM IR generated and verified successfully"};
    }
};

} // namespace pawc

#endif // PAW_LLVM_IRGEN_PASS_H

