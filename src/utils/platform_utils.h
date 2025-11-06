//===--- platform_utils.h - Platform Utilities -----------------*- C++ -*-===//
//
// PawLang Compiler - Platform Utilities
//
//===----------------------------------------------------------------------===//

#ifndef PAW_PLATFORM_UTILS_H
#define PAW_PLATFORM_UTILS_H

#include <string>

namespace pawc {

/// Get platform-specific executable file extension
/// Returns ".exe" on Windows, empty string on Unix-like systems
inline std::string getExecutableExtension() {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

/// Get default output filename based on input filename
/// Removes .paw extension and adds platform-specific executable extension
inline std::string getDefaultOutputFile(const std::string& input_file) {
    std::string output = input_file;
    
    // Remove .paw extension if present
    size_t last_dot = output.find_last_of('.');
    size_t last_slash = output.find_last_of("/\\");
    
    if (last_dot != std::string::npos && 
        (last_slash == std::string::npos || last_dot > last_slash) &&
        output.substr(last_dot) == ".paw") {
        output = output.substr(0, last_dot);
    }
    
    // Add platform-specific extension if not already present
    std::string ext = getExecutableExtension();
    if (!ext.empty()) {
        size_t ext_pos = output.find_last_of('.');
        size_t slash_pos = output.find_last_of("/\\");
        
        // Only add extension if there's no extension or the extension is not .exe
        if (ext_pos == std::string::npos || 
            (slash_pos != std::string::npos && ext_pos < slash_pos) ||
            output.substr(ext_pos) != ext) {
            output += ext;
        }
    }
    
    return output;
}

/// Ensure output filename has correct extension for the platform
/// If user specified a name without extension, add platform-specific extension
/// If user already specified correct extension, keep it as is
inline std::string ensureExecutableExtension(const std::string& output_file) {
    std::string ext = getExecutableExtension();
    
    if (ext.empty()) {
        // Unix-like: no extension needed, return as is
        return output_file;
    }
    
    // Windows: ensure .exe extension
    size_t last_dot = output_file.find_last_of('.');
    size_t last_slash = output_file.find_last_of("/\\");
    
    // Check if there's an extension after the last slash
    bool has_extension = (last_dot != std::string::npos) && 
                         (last_slash == std::string::npos || last_dot > last_slash);
    
    if (!has_extension) {
        // No extension: add .exe
        return output_file + ext;
    }
    
    // Has extension: check if it's already .exe
    if (output_file.substr(last_dot) == ext) {
        // Already has .exe, return as is
        return output_file;
    }
    
    // Has different extension: replace with .exe
    return output_file.substr(0, last_dot) + ext;
}

} // namespace pawc

#endif // PAW_PLATFORM_UTILS_H

