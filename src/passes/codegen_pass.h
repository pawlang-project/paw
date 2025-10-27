/**
 * @file codegen_pass.h
 * @brief 代码生成 Pass
 * 
 * @version 0.3.0-dev (Phase 4)
 * @date 2025-10-27
 */

#ifndef PAWC_CODEGEN_PASS_H
#define PAWC_CODEGEN_PASS_H

#include "compiler_pass.h"
#include "../codegen/codegen.h"

namespace pawc {

/**
 * 代码生成 Pass
 * 
 * 将 AST 转换为 LLVM IR。
 */
class CodeGenPass : public CompilerPass {
public:
    bool run(CompilationUnit* unit) override {
        // 创建 CodeGenerator
        CodeGenerator codegen(unit->filename, unit->symbol_table);
        
        // 生成 IR
        if (!codegen.generate(*unit->program)) {
            return false;
        }
        
        // 保存 IR module 和 CodeGenerator
        unit->ir_module = codegen.getModule();
        unit->code_generator = &codegen;
        
        return true;
    }
    
    const char* getName() const override {
        return "CodeGen";
    }
};

} // namespace pawc

#endif // PAWC_CODEGEN_PASS_H

