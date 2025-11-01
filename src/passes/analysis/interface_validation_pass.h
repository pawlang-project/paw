//===--- interface_validation_pass.h - Interface Validation Pass -*- C++ -*-===//
//
// 接口验证Pass：验证接口实现的正确性
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
        // TODO: 实现接口验证逻辑（已在InterfaceValidator中实现）
        // 这个Pass是对现有InterfaceValidator的封装
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_ANALYSIS_INTERFACE_VALIDATION_PASS_H

