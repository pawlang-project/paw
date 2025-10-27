/**
 * @file pass_manager.cpp
 * @brief PassManager 实现
 */

#include "pass_manager.h"
#include "pawc/colors.h"
#include <iostream>

namespace pawc {

bool PassManager::runAll(CompilationUnit* unit) {
    for (const auto& pass : passes_) {
        if (verbose_) {
            std::cout << Colors::info("  → Running Pass: ") << pass->getName() << std::endl;
        }
        
        if (!pass->run(unit)) {
            if (verbose_) {
                std::cerr << Colors::error("  ✗ Pass failed: ") << pass->getName() << std::endl;
            }
            return false;
        }
        
        if (verbose_) {
            std::cout << Colors::success("  ✓ Pass completed: ") << pass->getName() << std::endl;
        }
    }
    
    return true;
}

} // namespace pawc

