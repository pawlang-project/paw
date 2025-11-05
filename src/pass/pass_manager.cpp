//===--- pass_manager.cpp - Pass Manager Implementation ---------*- C++ -*-===//
/// @file pass_manager.cpp
/// @brief Implementation file

#include "pass_manager.h"
#include <iostream>

namespace pawc {

void PassManager::runAll() {
    for (auto& pass : passes_) {
        std::cout << "Running pass: " << pass->getName() << "..." << std::endl;
        auto result = pass->run(context_);
        
        if (!result.success) {
            std::cerr << "Pass failed: " << pass->getName() << std::endl;
            if (!result.message.empty()) {
                std::cerr << "Reason: " << result.message << std::endl;
            }
            return;
        }
        
        if (context_->isVerbose() && !result.message.empty()) {
            std::cout << "   " << result.message << std::endl;
        }
    }
}

} // namespace pawc
