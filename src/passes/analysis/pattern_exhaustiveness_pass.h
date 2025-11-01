//===--- pattern_exhaustiveness_pass.h - Pattern Exhaustiveness -*- C++ -*-===//
//
// 模式穷尽性检查Pass：检查match表达式是否覆盖所有情况
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
        // TODO: 实现模式穷尽性检查（已在PatternChecker中实现）
        // 这个Pass是对现有PatternChecker的封装
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_ANALYSIS_PATTERN_EXHAUSTIVENESS_PASS_H

