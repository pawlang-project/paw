//===--- dead_code_elimination_pass.h - Dead Code Elimination ---*- C++ -*-===//
/// @file dead_code_elimination_pass.h
/// @brief Code generation utilities
///
//
// Dead code elimination pass: remove unreachable and unused code
//
//===----------------------------------------------------------------------===//

#ifndef PAWC_PASSES_TRANSFORM_DEAD_CODE_ELIMINATION_PASS_H
#define PAWC_PASSES_TRANSFORM_DEAD_CODE_ELIMINATION_PASS_H

#include "pass/pass_base.h"

namespace pawc {

class DeadCodeEliminationPass : public PassBase<DeadCodeEliminationPass> {
public:
    static constexpr const char* getName() { return "DeadCodeEliminationPass"; }
    
    bool run(PassContext& context) override {
        // TODO: implement dead code elimination
        // 1. Identify unreachable code
        // 2. identifynousevariable
        // 3. Remove dead code
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_TRANSFORM_DEAD_CODE_ELIMINATION_PASS_H

