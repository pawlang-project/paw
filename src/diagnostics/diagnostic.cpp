//===--- diagnostic.cpp - Diagnostic Implementation -------------*- C++ -*-===//
/// @file diagnostic.cpp
/// @brief Implementation file
///

#include "diagnostic.h"
#include <sstream>

namespace pawc {

std::string Diagnostic::format() const {
    std::ostringstream oss;
    
    // Level prefix
    switch (level_) {
        case DiagnosticLevel::Note:    oss << "note: "; break;
        case DiagnosticLevel::Warning: oss << "warning: "; break;
        case DiagnosticLevel::Error:   oss << "error: "; break;
        case DiagnosticLevel::Fatal:   oss << "fatal error: "; break;
    }
    
    // Location and message
    oss << location_.file << ":" << location_.line << ":" << location_.column 
        << ": " << message_;
    
    // Notes
    for (const auto& note : notes_) {
        oss << "\n  note: " << note;
    }
    
    return oss.str();
}

} // namespace pawc
