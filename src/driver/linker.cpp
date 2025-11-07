//===--- linker.cpp - Linker Implementation ----------------------*- C++ -*-===//
/// @file linker.cpp
/// @brief Implementation file

#include "linker.h"

#include <cstdlib>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace pawc {

Linker::Linker() : runtime_path_(""), lld_path_(""), verbose_(false) {
    // Try to find bundled lld in build directory
    // This assumes the linker is called from project root or build directory
    std::filesystem::path current_path = std::filesystem::current_path();
    
    // Try common locations relative to current directory
    std::vector<std::filesystem::path> search_paths = {
        current_path / "build" / "llvm" / "bin",
        current_path / "llvm" / "bin",
        current_path.parent_path() / "build" / "llvm" / "bin",
    };
    
    // Also try absolute path from executable location (if available)
    #ifdef _WIN32
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) > 0) {
        std::filesystem::path exe_dir = std::filesystem::path(exe_path).parent_path();
        // If pawc.exe is in build/, lld is in build/llvm/bin/
        search_paths.insert(search_paths.begin(), exe_dir / "llvm" / "bin");
        // If pawc.exe is in build/x64/Debug/, go up to build/llvm/bin/
        search_paths.insert(search_paths.begin(), exe_dir.parent_path().parent_path() / "llvm" / "bin");
    }
    #endif
    
#ifdef _WIN32
    std::string lld_name = "lld-link.exe";
#else
    std::string lld_name = "ld.lld";
#endif
    
    for (const auto& path : search_paths) {
        auto lld_full_path = path / lld_name;
        if (std::filesystem::exists(lld_full_path)) {
            lld_path_ = path.string();
            break;
        }
    }
    
    // Debug: print found lld path if verbose (will be set later)
}

bool Linker::link(const std::vector<std::string>& object_files,
                  const std::string& output_file,
                  const std::vector<std::string>& library_paths,
                  const std::vector<std::string>& libraries) {
    
    if (object_files.empty()) {
        error_ = "No object files provided";
        return false;
    }
    
    // build/constructlinkcommand
    auto args = buildLinkCommand(object_files, output_file, library_paths, libraries);
    
    // executelink
    return invokeSystemLinker(args);
}

std::vector<std::string> Linker::buildLinkCommand(
    const std::vector<std::string>& object_files,
    const std::string& output_file,
    const std::vector<std::string>& library_paths,
    const std::vector<std::string>& libraries) {
    
    std::vector<std::string> args;
    
    // Use platform-specific linker (prefer bundled lld if available)
    std::string linker_exe;
    
#ifdef __APPLE__
    if (!lld_path_.empty()) {
        linker_exe = (std::filesystem::path(lld_path_) / "ld64.lld").string();
    } else {
        linker_exe = "ld";
    }
#elif defined(_WIN32)
    // Windows: Use bundled lld-link.exe (COFF linker for Windows)
    if (!lld_path_.empty()) {
        linker_exe = (std::filesystem::path(lld_path_) / "lld-link.exe").string();
    } else {
        // Fallback to system lld-link if bundled not found
        linker_exe = "lld-link";
    }
#else
    // Linux and other Unix-like systems
    if (!lld_path_.empty()) {
        linker_exe = (std::filesystem::path(lld_path_) / "ld.lld").string();
    } else {
        linker_exe = "ld.lld";
    }
#endif
    
    args.push_back(linker_exe);
    
    // Debug output for bundled lld usage
    if (verbose_ && !lld_path_.empty()) {
        std::cout << "   Using bundled lld from: " << lld_path_ << "\n";
    }
    
#ifdef _WIN32
    // Windows: Use lld-link syntax
    // Output file
    args.push_back("/OUT:" + output_file);
    
    // Specify machine type (X64 for x86_64)
    args.push_back("/MACHINE:X64");
    
    // Use console subsystem (for console applications)
    args.push_back("/SUBSYSTEM:CONSOLE");
    
    // Use standard mainCRTStartup entry point (calls main after C runtime init)
    // This is the standard approach when using Windows SDK and UCRT
    args.push_back("/ENTRY:mainCRTStartup");
    
    // Object files
    for (const auto& obj : object_files) {
        args.push_back(obj);
    }
    
    // Runtime library (link early, before C++ standard library)
    // This ensures our runtime symbols are available, but C++ stdlib symbols take precedence
    if (!runtime_path_.empty()) {
#ifdef _WIN32
        std::filesystem::path runtime_lib = std::filesystem::path(runtime_path_) / "pawc_runtime.lib";
#else
        std::filesystem::path runtime_lib = std::filesystem::path(runtime_path_) / "libpawc_runtime.a";
#endif
        if (verbose_) {
            std::cout << "   Runtime library: " << runtime_lib.string() << "\n";
        }
        args.push_back(runtime_lib.string());
    } else {
        if (verbose_) {
            std::cout << "   WARNING: Runtime path is empty!\n";
        }
    }
    
    // Library search paths
    for (const auto& path : library_paths) {
        args.push_back("/LIBPATH:" + path);
    }
    
    // Libraries
    for (const auto& lib : libraries) {
        args.push_back(lib + ".lib");
    }
    
    // Link Windows system libraries using Windows SDK
    // lld-link can automatically detect Windows SDK, but we'll also try to find it manually
    // Windows SDK provides kernel32.lib and other system libraries
    
    // Try to find Windows SDK in common locations
    // Use a more targeted approach to avoid slow directory scanning
    std::vector<std::string> common_sdk_versions = {
        "10.0.26100.0",  // Latest Windows 11
        "10.0.22621.0",  // Windows 11 22H2
        "10.0.22000.0",  // Windows 11 21H2
        "10.0.20348.0",  // Windows Server 2022
        "10.0.19041.0",  // Windows 10 2004
        "10.0.18362.0",  // Windows 10 1903
        "10.0.17763.0",  // Windows 10 1809
    };
    
    std::vector<std::string> windows_sdk_paths = {
        "C:/Program Files (x86)/Windows Kits/10/Lib",
        "C:/Program Files/Windows Kits/10/Lib"
    };
    
    std::string found_windows_sdk_path;
    std::string found_sdk_version;
    
    // Try common SDK versions first (faster than scanning)
    for (const auto& base_path : windows_sdk_paths) {
        try {
            if (!std::filesystem::exists(base_path)) continue;
            
            for (const auto& version : common_sdk_versions) {
                std::string um_path = base_path + "/" + version + "/um/x64";
                std::string ucrt_path = base_path + "/" + version + "/ucrt/x64";
                
                if (std::filesystem::exists(um_path)) {
                    found_windows_sdk_path = um_path;
                    found_sdk_version = version;
                    
                    // Add UM (User Mode) library path
                    if (um_path.find(' ') != std::string::npos) {
                        args.push_back("/LIBPATH:\"" + um_path + "\"");
                    } else {
                        args.push_back("/LIBPATH:" + um_path);
                    }
                    
                    // Add UCRT (Universal C Runtime) library path if exists
                    if (std::filesystem::exists(ucrt_path)) {
                        if (ucrt_path.find(' ') != std::string::npos) {
                            args.push_back("/LIBPATH:\"" + ucrt_path + "\"");
                        } else {
                            args.push_back("/LIBPATH:" + ucrt_path);
                        }
                    }
                    
                    if (verbose_) {
                        std::cout << "   Found Windows SDK: " << version << "\n";
                        std::cout << "   UM path: " << um_path << "\n";
                        if (std::filesystem::exists(ucrt_path)) {
                            std::cout << "   UCRT path: " << ucrt_path << "\n";
                        }
                    }
                    goto sdk_found;  // Break out of nested loops
                }
            }
        } catch (const std::exception& e) {
            // Ignore filesystem errors and try next path
            if (verbose_) {
                std::cout << "   Warning: Could not access Windows SDK at " << base_path << ": " << e.what() << "\n";
            }
        }
    }
    sdk_found:
    
    // Also check LIB environment variable (set by Visual Studio or Windows SDK)
    const char* lib_env = std::getenv("LIB");
    if (lib_env) {
        std::string lib_paths = lib_env;
        size_t pos = 0;
        while ((pos = lib_paths.find(';')) != std::string::npos) {
            std::string path = lib_paths.substr(0, pos);
            if (path.find("Windows Kits") != std::string::npos || 
                path.find("kernel32") != std::string::npos) {
                // Quote the path if it contains spaces
                if (path.find(' ') != std::string::npos) {
                    args.push_back("/LIBPATH:\"" + path + "\"");
                } else {
                    args.push_back("/LIBPATH:" + path);
                }
            }
            lib_paths.erase(0, pos + 1);
        }
        if (!lib_paths.empty() && 
            (lib_paths.find("Windows Kits") != std::string::npos || 
             lib_paths.find("kernel32") != std::string::npos)) {
            // Quote the path if it contains spaces
            if (lib_paths.find(' ') != std::string::npos) {
                args.push_back("/LIBPATH:\"" + lib_paths + "\"");
            } else {
                args.push_back("/LIBPATH:" + lib_paths);
            }
        }
    }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Pure MSVC Runtime Libraries Configuration
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    // Link essential Windows system libraries
    args.push_back("/DEFAULTLIB:kernel32");   // Core Windows API
    args.push_back("/DEFAULTLIB:user32");     // User interface functions
    
    // Use /NODEFAULTLIB to prevent automatic linking of conflicting CRT versions
    args.push_back("/NODEFAULTLIB:msvcrt");   // Exclude dynamic CRT
    args.push_back("/NODEFAULTLIB:libcmtd");  // Exclude debug static CRT
    args.push_back("/NODEFAULTLIB:msvcrtd");  // Exclude debug dynamic CRT
    
    // Explicitly link static CRT (Release, Multi-threaded)
    args.push_back("/DEFAULTLIB:libcmt");     // Static C Runtime (legacy CRT functions)
    args.push_back("/DEFAULTLIB:libucrt");    // Universal CRT (printf, fflush, etc.)
    args.push_back("/DEFAULTLIB:libvcruntime"); // C++ runtime support
    
    // Additional MSVC runtime libraries
    args.push_back("/DEFAULTLIB:oldnames");   // POSIX function name mapping
    
    // Suppress warnings about missing PDB files (common in Release builds)
    args.push_back("/IGNORE:4099");
    
    // Disable incremental linking for simpler output
    args.push_back("/INCREMENTAL:NO");
#else
    // Unix-like systems (macOS, Linux)
    // Output file
    args.push_back("-o");
    args.push_back(output_file);
    
    // Object files
    for (const auto& obj : object_files) {
        args.push_back(obj);
    }
    
    // Runtime library
    if (!runtime_path_.empty()) {
        args.push_back(runtime_path_ + "/libpawc_runtime.a");
    }
    
    // Library search paths
    for (const auto& path : library_paths) {
        args.push_back("-L" + path);
    }
    
    // Libraries
    for (const auto& lib : libraries) {
        args.push_back("-l" + lib);
    }
    
    // System libraries
#ifdef __APPLE__
    // macOS specific
    args.push_back("-lSystem");
    args.push_back("-syslibroot");
    args.push_back("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk");
#else
    // Linux
    args.push_back("-lc");
    args.push_back("-lm");
    args.push_back("-lpthread");
#endif
#endif
    
    return args;
}

bool Linker::invokeSystemLinker(const std::vector<std::string>& args) {
    // Build command string
    std::stringstream cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd << " ";
        // Quote arguments with spaces
        std::string arg = args[i];
        if (arg.find(' ') != std::string::npos && 
            arg.find('"') == std::string::npos &&
            !arg.empty() && arg[0] != '"') {
            cmd << "\"" << arg << "\"";
        } else {
            cmd << arg;
        }
    }
    
    std::string command = cmd.str();
    
    if (verbose_) {
        std::cout << "   Linking command length: " << command.length() << " characters\n";
        std::cout << "   Linker: " << args[0] << "\n";
        std::cout << "   Arguments count: " << (args.size() - 1) << "\n";
        
        // Print full command for debugging (truncate if too long)
        if (command.length() > 1000) {
            std::cout << "   Command (first 1000 chars): " 
                      << command.substr(0, 1000) << "...\n";
        } else {
            std::cout << "   Full command: " << command << "\n";
        }
        std::cout << "   Executing linker...\n";
        std::cout.flush();  // Ensure output is displayed immediately
    }
    
    // executelinkcommand
    int results = std::system(command.c_str());
    
    if (results != 0) {
        error_ = "Linker failed with exit code: " + std::to_string(results);
        if (verbose_) {
            std::cerr << "   ❌ Linking failed!\n";
            std::cerr << "   Exit code: " << results << "\n";
        }
        return false;
    }
    
    if (verbose_) {
        std::cout << "   [OK] Linking successful\n";
    }
    
    return true;
}

} // namespace pawc

