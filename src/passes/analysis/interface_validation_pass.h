//===--- interface_validation_pass.h - Interface Validation Pass -*- C++ -*-===//
/// @file interface_validation_pass.h
/// @brief Code generation utilities
///
//
// Interface validation pass: validate interface implementation correctnesshecorrectperformance
//
//===----------------------------------------------------------------------===//

#ifndef PAWC_PASSES_ANALYSIS_INTERFACE_VALIDATION_PASS_H
#define PAWC_PASSES_ANALYSIS_INTERFACE_VALIDATION_PASS_H

#include "pass/pass_base.h"

namespace pawc {

class InterfaceValidationPass : public PassBase<InterfaceValidationPass> {
public:
    static constexpr const char* getName() { return "InterfaceValidationPass"; }
    
    bool run(PassContext& context) override {
        // TODO: implementationinterfacevalidatelogic（alreadyin/atInterfaceValidatormiddle/centerimplementation）
        // This pass should only appear when there are interfaceserfaceValidatorof/thewrapper/encapsulation
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_ANALYSIS_INTERFACE_VALIDATION_PASS_H

