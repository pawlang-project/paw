//===--- pass_manager.h - Pass Manager ---------------------------*- C++ -*-===//

#ifndef PAW_PASS_MANAGER_H
#define PAW_PASS_MANAGER_H

#include "pass.h"
#include "pass_context.h"
#include <vector>
#include <memory>
#include <string>

namespace pawc {

/// PassManager - 管理和执行Pass
class PassManager {
public:
    explicit PassManager(PassContext* context) : context_(context) {}
    
    template<typename PassT>
    void addPass() {
        passes_.push_back(std::make_unique<PassHolder<PassT>>());
    }
    
    void runAll();
    
private:
    struct PassHolderBase {
        virtual ~PassHolderBase() = default;
        virtual PassResult run(PassContext* ctx) = 0;
        virtual std::string getName() const = 0;
    };
    
    template<typename PassT>
    struct PassHolder : PassHolderBase {
        PassResult run(PassContext* ctx) override {
            PassT pass;
            return pass.run(ctx);
        }
        
        std::string getName() const override {
            return PassT::name();
        }
    };
    
    PassContext* context_;
    std::vector<std::unique_ptr<PassHolderBase>> passes_;
};

} // namespace pawc

#endif // PAW_PASS_MANAGER_H
