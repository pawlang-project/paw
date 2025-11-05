//===--- type_inference_pass.h - Type Inference Pass ------------*- C++ -*-===//
/// @file type_inference_pass.h
/// @brief Code generation utilities
///
//
// typeinferPass：infervariableandexpressionof/thetypes
//
//===----------------------------------------------------------------------===//

#ifndef PAWC_PASSES_ANALYSIS_TYPE_INFERENCE_PASS_H
#define PAWC_PASSES_ANALYSIS_TYPE_INFERENCE_PASS_H

#include "pass/pass_base.h"

namespace pawc {

class TypeInferencePass : public PassBase<TypeInferencePass> {
public:
    static constexpr const char* getName() { return "TypeInferencePass"; }
    
    bool run(PassContext& context) override {
        // TODO: implementationtypesinferlogic
        // 1. infervariabletypes
        // 2. inferexpressiontypes
        // 3. inferreturntypes
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_ANALYSIS_TYPE_INFERENCE_PASS_H

