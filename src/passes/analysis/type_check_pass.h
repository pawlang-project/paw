//===--- type_check_pass.h - Type Check Pass --------------------*- C++ -*-===//
/// @file type_check_pass.h
/// @brief Code generation utilities
///
//
// Type checking pass: validate type correctness and consistencycy
//
//===----------------------------------------------------------------------===//

#ifndef PAWC_PASSES_ANALYSIS_TYPE_CHECK_PASS_H
#define PAWC_PASSES_ANALYSIS_TYPE_CHECK_PASS_H

#include "pass/pass_base.h"

namespace pawc {

class TypeCheckPass : public PassBase<TypeCheckPass> {
public:
    static constexpr const char* getName() { return "TypeCheckPass"; }
    
    bool run(PassContext& context) override {
        // TODO: implementationtypeschecklogic（alreadyin/atTypeCheckermiddle/centerimplementation）
        // This pass should only appear when there is TypeChecker wrapping
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_ANALYSIS_TYPE_CHECK_PASS_H

