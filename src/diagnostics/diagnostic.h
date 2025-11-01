//===--- diagnostic.h - Diagnostic Messages ----------------------*- C++ -*-===//

#ifndef PAW_DIAGNOSTIC_H
#define PAW_DIAGNOSTIC_H

#include "source_location.h"
#include <string>
#include <vector>

namespace pawc {

enum class DiagnosticLevel {
    Note,
    Warning,
    Error,
    Fatal
};

class Diagnostic {
public:
    Diagnostic(DiagnosticLevel level, const SourceLocation& loc, const std::string& msg)
        : level_(level), location_(loc), message_(msg) {}
    
    DiagnosticLevel getLevel() const { return level_; }
    const SourceLocation& getLocation() const { return location_; }
    const std::string& getMessage() const { return message_; }
    
    void addNote(const std::string& note) {
        notes_.push_back(note);
    }
    
    std::string format() const;
    
private:
    DiagnosticLevel level_;
    SourceLocation location_;
    std::string message_;
    std::vector<std::string> notes_;
};

} // namespace pawc

#endif // PAW_DIAGNOSTIC_H
