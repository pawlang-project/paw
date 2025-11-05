//===--- pattern_exhaustiveness_pass.h - Pattern Exhaustiveness -*- C++ -*-===//
/// @file pattern_exhaustiveness_pass.h
/// @brief Code generation utilities
///
//
// Pattern exhaustiveness check pass: check if match expression covers all caseserAllcase/situation
//
//===----------------------------------------------------------------------===//

#ifndef PAWC_PASSES_ANALYSIS_PATTERN_EXHAUSTIVENESS_PASS_H
#define PAWC_PASSES_ANALYSIS_PATTERN_EXHAUSTIVENESS_PASS_H

#include "pass/pass_base.h"

namespace pawc {

class PatternExhaustivenessPass : public PassBase<PatternExhaustivenessPass> {
public:
    static constexpr const char* getName() { return "PatternExhaustivenessPass"; }
    
    bool run(PassContext& context) override {
        // TODO: implement pattern exhaustiveness check (already in PatternChecker)eckermiddle/centerimplementation）
        // This pass should only appear when there is PatternChecker wrapping
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_ANALYSIS_PATTERN_EXHAUSTIVENESS_PASS_H

