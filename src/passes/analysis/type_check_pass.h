//===--- type_check_pass.h - Type Check Pass --------------------*- C++ -*-===//
//
// 类型检查Pass：验证类型的正确性和一致性
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
        // TODO: 实现类型检查逻辑（已在TypeChecker中实现）
        // 这个Pass是对现有TypeChecker的封装
        return true;
    }
};

} // namespace pawc

#endif // PAWC_PASSES_ANALYSIS_TYPE_CHECK_PASS_H

