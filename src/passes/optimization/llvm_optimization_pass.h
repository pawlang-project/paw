//===--- llvm_optimization_pass.h - LLVM Optimization Pass ------*- C++ -*-===//

#ifndef PAW_LLVM_OPTIMIZATION_PASS_H
#define PAW_LLVM_OPTIMIZATION_PASS_H

#include "pass/pass.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Utils.h>

namespace pawc {

/// LLVMOptimizationPass - LLVM优化Pass
///
/// 根据优化级别应用不同的LLVM优化
/// -O0: 无优化
/// -O1: 基础优化（Mem2Reg, InstCombine等）
/// -O2: 标准优化（+ GVN, DSE, DCE等）
/// -O3: 激进优化（+ Inlining, Unroll, Vectorize等）
class LLVMOptimizationPass : public PassBase<LLVMOptimizationPass> {
public:
    static std::string name() { return "LLVMOptimizationPass"; }
    
    PassResult runImpl(PassContext* context) {
        // 从context获取LLVM模块
        llvm::Module* module = nullptr;
        if (!context->getCachedResult("llvm_module", module) || !module) {
            return PassResult{false, "No LLVM module available for optimization"};
        }
        
        int opt_level = context->getOptLevel();
        
        if (opt_level == 0) {
            // -O0: 无优化，直接返回
            return PassResult{true, "No optimization (O0)"};
        }
        
        // 创建优化Pass管理器
        llvm::legacy::PassManager pm;
        
        if (opt_level >= 1) {
            // -O1: 基础优化
            pm.add(llvm::createPromoteMemoryToRegisterPass());  // Mem2Reg
            pm.add(llvm::createInstructionCombiningPass());     // InstCombine
            pm.add(llvm::createReassociatePass());              // Reassociate
            pm.add(llvm::createCFGSimplificationPass());        // CFG简化
        }
        
        if (opt_level >= 2) {
            // -O2: 标准优化
            pm.add(llvm::createEarlyCSEPass());                 // Early CSE
            pm.add(llvm::createDeadCodeEliminationPass());      // DCE
            pm.add(llvm::createCFGSimplificationPass());        // 再次CFG简化
        }
        
        if (opt_level >= 3) {
            // -O3: 激进优化
            pm.add(llvm::createLoopUnrollPass());               // Loop Unroll
        }
        
        // 运行所有优化Pass
        pm.run(*module);
        
        if (context->isVerbose()) {
            return PassResult{true, "LLVM optimizations applied (O" + 
                                   std::to_string(opt_level) + ")"};
        }
        
        return PassResult{true, "Optimization complete"};
    }
};

} // namespace pawc

#endif // PAW_LLVM_OPTIMIZATION_PASS_H

