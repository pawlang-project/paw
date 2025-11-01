//===--- diagnostic_engine.h - Diagnostic Engine ------------------*- C++ -*-===//

#ifndef PAW_DIAGNOSTIC_ENGINE_H
#define PAW_DIAGNOSTIC_ENGINE_H

#include "source_location.h"
#include <vector>
#include <string>
#include <iostream>

namespace pawc {

enum class DiagnosticLevel {
    Note, Warning, Error, Fatal
};

class Diagnostic {
public:
    DiagnosticLevel level;
    std::string message;
    SourceLocation location;
    
    Diagnostic(DiagnosticLevel lv, const std::string& msg, SourceLocation loc)
        : level(lv), message(msg), location(loc) {}
};

class DiagnosticEngine {
public:
    void reportError(const std::string& msg, SourceLocation loc) {
        diagnostics_.emplace_back(DiagnosticLevel::Error, msg, loc);
        has_errors_ = true;
    }
    
    void reportWarning(const std::string& msg, SourceLocation loc) {
        diagnostics_.emplace_back(DiagnosticLevel::Warning, msg, loc);
    }
    
    bool hasErrors() const { return has_errors_; }
    
    void printAll() const {
        for (const auto& diag : diagnostics_) {
            std::cerr << (diag.level == DiagnosticLevel::Error ? "error: " : "warning: ")
                     << diag.message;
            if (!diag.location.file.empty()) {
                std::cerr << " at " << diag.location.file << ":" << diag.location.line;
            }
            std::cerr << "\n";
        }
    }
    
    void clear() {
        diagnostics_.clear();
        has_errors_ = false;
    }
    
private:
    std::vector<Diagnostic> diagnostics_;
    bool has_errors_ = false;
};

} // namespace pawc

#endif // PAW_DIAGNOSTIC_ENGINE_H
