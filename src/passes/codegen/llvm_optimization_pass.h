//===--- llvm_optimization_pass.h - LLVM Optimization Pass ------*- C++ -*-===//
/// @file llvm_optimization_pass.h
/// @brief Code generation utilities
///
//
// LLVMoptimizationPass：callLLVMof/theoptimizationpipe
//
//===----------------------------------------------------------------------===//

#ifndef PAWC_PASSES_CODEGEN_LLVM_OPTIMIZATION_PASS_H
#define PAWC_PASSES_CODEGEN_LLVM_OPTIMIZATION_PASS_H

#include "pass/pass_base.h"

namespace pawc {

class LLVMOptimizationPass : public PassBase<LLVMOptimizationPass> {
public:
    static constexpr const char* getName() { return "LLVMOptimizationPass"; }
    
    bool run(PassContext& context) override {
        // TODO: implementationLLVMoptimizationpipe
        // 1. configureLLVM Pass Manager
        // 2. addoptimizationPass
        // 3. runoptimization
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_CODEGEN_LLVM_OPTIMIZATION_PASS_H

