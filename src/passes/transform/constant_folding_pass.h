//===--- constant_folding_pass.h - Constant Folding Pass --------*- C++ -*-===//
/// @file constant_folding_pass.h
/// @brief Code generation utilities
///
//
// constantfoldPass：in/atcompile/compilationearlycomputeconstantexpression
//
//===----------------------------------------------------------------------===//

#ifndef PAWC_PASSES_TRANSFORM_CONSTANT_FOLDING_PASS_H
#define PAWC_PASSES_TRANSFORM_CONSTANT_FOLDING_PASS_H

#include "pass/pass_base.h"

namespace pawc {

class ConstantFoldingPass : public PassBase<ConstantFoldingPass> {
public:
    static constexpr const char* getName() { return "ConstantFoldingPass"; }
    
    bool run(PassContext& context) override {
        // TODO: implementationconstantfold
        // 1. identifyconstantexpression
        // 2. compile/compilationearlycompute
        // 3. substitutionis/asconstant
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_TRANSFORM_CONSTANT_FOLDING_PASS_H

