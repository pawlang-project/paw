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
    
    // Use /ENTRY:main to directly use our main function
    // This avoids needing mainCRTStartup and C runtime initialization
    args.push_back("/ENTRY:main");
    
    // Object files
    for (const auto& obj : object_files) {
        args.push_back(obj);
    }
    
    // Runtime library (link early, before C++ standard library)
    // This ensures our runtime symbols are available, but C++ stdlib symbols take precedence
    if (!runtime_path_.empty()) {
        std::filesystem::path runtime_lib = std::filesystem::path(runtime_path_) / "libpawc_runtime.a";
        args.push_back(runtime_lib.string());
    }
    
    // Library search paths
    for (const auto& path : library_paths) {
        args.push_back("/LIBPATH:" + path);
    }
    
    // Libraries
    for (const auto& lib : libraries) {
        args.push_back(lib + ".lib");
    }
    
    // Windows: Try to link MinGW libraries if available (for atexit, etc.)
    // Try to find MinGW libraries in common locations
    std::vector<std::string> mingw_lib_paths = {
        "C:/msys64/mingw64/lib",
        "C:/mingw64/lib",
        "C:/msys64/ucrt64/lib"
    };
    
    std::string found_mingw_path;
    for (const auto& path : mingw_lib_paths) {
        if (std::filesystem::exists(path)) {
            found_mingw_path = path;
            args.push_back("/LIBPATH:" + path);
            break;
        }
    }
    
    // Link MinGW libraries for runtime support
    // Note: Since we use /ENTRY:main and provide our own __main,
    // we don't need libmingw32 (which requires C runtime initialization)
    // But we need libgcc, libmingwex, and libmsvcrt for runtime functions
    // IMPORTANT: Link order matters! libgcc must come before libstdc++
    if (!found_mingw_path.empty()) {
        // 1. Link libmingw32 FIRST - provides low-level functions like ___chkstk_ms
        // This must come before other libraries that depend on it
        std::filesystem::path libmingw32 = std::filesystem::path(found_mingw_path) / "libmingw32.a";
        if (std::filesystem::exists(libmingw32)) {
            args.push_back(libmingw32.string());
        }
        
        // 2. Link libgcc - provides exception handling (_Unwind_* symbols)
        // libstdc++ depends on libgcc, so it must be linked before libstdc++
        // Try different possible names for libgcc
        std::vector<std::string> libgcc_names = {
            "libgcc.a",
            "libgcc_s.a",
            "libgcc_s_seh-1.dll.a",  // SEH version (for 64-bit)
            "libgcc_s_dw2-1.dll.a"   // DWARF version (for 32-bit)
        };
        bool libgcc_found = false;
        for (const auto& name : libgcc_names) {
            std::filesystem::path libgcc = std::filesystem::path(found_mingw_path) / name;
            if (std::filesystem::exists(libgcc)) {
                args.push_back(libgcc.string());
                libgcc_found = true;
                break;
            }
        }
        // If not found as .a, try to use /DEFAULTLIB to link libgcc
        if (!libgcc_found) {
            args.push_back("/DEFAULTLIB:libgcc");
        }
        
        // 3. Link pthread library (for mutex support in C++ standard library)
        std::filesystem::path libpthread = std::filesystem::path(found_mingw_path) / "libpthread.a";
        if (std::filesystem::exists(libpthread)) {
            args.push_back(libpthread.string());
        }
        
        // 4. Link C++ standard library (libstdc++) - depends on libgcc
        std::filesystem::path libstdcxx = std::filesystem::path(found_mingw_path) / "libstdc++.a";
        if (std::filesystem::exists(libstdcxx)) {
            args.push_back(libstdcxx.string());
        }
        
        // 4.5. Link libgcc again after libstdc++ to resolve any remaining symbols
        // Some symbols may only be needed after libstdc++ is linked
        if (libgcc_found) {
            // Link libgcc again - use the same name we found earlier
            for (const auto& name : libgcc_names) {
                std::filesystem::path libgcc = std::filesystem::path(found_mingw_path) / name;
                if (std::filesystem::exists(libgcc)) {
                    args.push_back(libgcc.string());
                    break;
                }
            }
        }
        
        // 5. Link libmingwex for MinGW-specific functions (__mingw_printf, etc.)
        std::filesystem::path libmingwex = std::filesystem::path(found_mingw_path) / "libmingwex.a";
        if (std::filesystem::exists(libmingwex)) {
            args.push_back(libmingwex.string());
        }
        
        // 6. Link libmsvcrt.a (MinGW's C runtime wrapper)
        std::filesystem::path libmsvcrt = std::filesystem::path(found_mingw_path) / "libmsvcrt.a";
        if (std::filesystem::exists(libmsvcrt)) {
            args.push_back(libmsvcrt.string());
        }
        
    }
    
    // Link Windows system libraries using Windows SDK
    // lld-link can automatically detect Windows SDK, but we'll also try to find it manually
    // Windows SDK provides kernel32.lib and other system libraries
    
    // Try to find Windows SDK in common locations
    std::vector<std::string> windows_sdk_paths = {
        "C:/Program Files (x86)/Windows Kits/10/Lib",
        "C:/Program Files/Windows Kits/10/Lib"
    };
    
    std::string found_windows_sdk_path;
    // Look for the latest version in the Windows SDK directory
    for (const auto& base_path : windows_sdk_paths) {
        if (std::filesystem::exists(base_path)) {
            // Try to find the latest version (e.g., 10.0.22621.0)
            std::vector<std::string> versions;
            try {
                for (const auto& entry : std::filesystem::directory_iterator(base_path)) {
                    if (entry.is_directory()) {
                        std::string version = entry.path().filename().string();
                        // Check if it looks like a version number (e.g., 10.0.xxxxx.x)
                        if (version.find("10.0") == 0) {
                            versions.push_back(version);
                        }
                    }
                }
                // Sort versions in descending order to get the latest
                std::sort(versions.rbegin(), versions.rend());
                if (!versions.empty()) {
                    std::string latest_version = versions[0];
                    std::string um_path = base_path + "/" + latest_version + "/um/x64";
                    if (std::filesystem::exists(um_path)) {
                        found_windows_sdk_path = um_path;
                        // Quote the path if it contains spaces
                        if (found_windows_sdk_path.find(' ') != std::string::npos) {
                            args.push_back("/LIBPATH:\"" + found_windows_sdk_path + "\"");
                        } else {
                            args.push_back("/LIBPATH:" + found_windows_sdk_path);
                        }
                        break;
                    }
                }
            } catch (...) {
                // Ignore errors when scanning directories
            }
        }
    }
    
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
    
    // Now we can safely link kernel32 and other Windows system libraries
    // lld-link will find them in the Windows SDK paths we added
    args.push_back("/DEFAULTLIB:kernel32");
    
    // Allow multiple definitions for C++ runtime symbols
    // This is needed because libstdc++ may define symbols that are also referenced
    // in other libraries. The linker will use the first definition found.
    // Note: This is safe for C++ runtime symbols like __cxa_pure_virtual
    args.push_back("/FORCE:MULTIPLE");
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
        cmd << args[i];
    }
    
    std::string command = cmd.str();
    
    if (verbose_) {
        std::cout << "   Linking command: " << command << "\n";
    }
    
    // executelinkcommand
    int results = std::system(command.c_str());
    
    if (results != 0) {
        error_ = "Linker failed with exit code: " + std::to_string(results);
        if (verbose_) {
            std::cerr << "   Link command: " << command << "\n";
        }
        return false;
    }
    
    if (verbose_) {
        std::cout << "   Linking successful\n";
    }
    
    return true;
}

} // namespace pawc

