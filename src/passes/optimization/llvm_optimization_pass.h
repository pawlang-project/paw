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

/// LLVMOptimizationPass - LLVMoptimizationPass
///
/// according tooptimizationlevelapplydifferentof/theLLVMoptimization
/// -O0: nooptimization
/// -O1: basic optimization (Mem2Reg, InstCombine, etc.)
/// -O2: standard optimization (+ GVN, DSE, DCE, etc.)
/// -O3: aggressive optimization (+ Inlining, Unroll, Vectorization, etc.)orizeetc）
class LLVMOptimizationPass : public PassBase<LLVMOptimizationPass> {
public:
    static std::string name() { return "LLVMOptimizationPass"; }
    
    PassResult runImpl(PassContext* context) {
        // Get LLVM module from context
        llvm::Module* module = nullptr;
        if (!context->getCachedResult("llvm_module", module) || !module) {
            return PassResult{false, "No LLVM module available for optimization"};
        }
        
        int opt_level = context->getOptLevel();
        
        if (opt_level == 0) {
            // -O0: nooptimization，directlyreturn
            return PassResult{true, "No optimization (O0)"};
        }
        
        // createoptimizationPassmanager
        llvm::legacy::PassManager pm;
        
        if (opt_level >= 1) {
            // -O1: baseoptimization
            pm.add(llvm::createPromoteMemoryToRegisterPass());  // Mem2Reg
            pm.add(llvm::createInstructionCombiningPass());     // InstCombine
            pm.add(llvm::createReassociatePass());              // Reassociate
            pm.add(llvm::createCFGSimplificationPass());        // CFGsimplify
        }
        
        if (opt_level >= 2) {
            // -O2: standardoptimization
            pm.add(llvm::createEarlyCSEPass());                 // Early CSE
            pm.add(llvm::createDeadCodeEliminationPass());      // DCE
            pm.add(llvm::createCFGSimplificationPass());        // againCFGsimplify
        }
        
        if (opt_level >= 3) {
            // -O3: aggressive optimization
            pm.add(llvm::createLoopUnrollPass());               // Loop Unroll
        }
        
        // runAlloptimizationPass
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

