//===--- linker.cpp - Linker Implementation ----------------------*- C++ -*-===//
/// @file linker.cpp
/// @brief Implementation file

#include "linker.h"

#include <cstdlib>
#include <sstream>
#include <iostream>

namespace pawc {

Linker::Linker() : runtime_path_(""), verbose_(false) {}

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
    
    // Use system default linker (macOS uses ld, Linux uses ld.lld)
#ifdef __APPLE__
    args.push_back("ld");
#else
    args.push_back("ld.lld");
#endif
    
    // outputfile
    args.push_back("-o");
    args.push_back(output_file);
    
    // objectfile
    for (const auto& obj : object_files) {
        args.push_back(obj);
    }
    
    // Runtimelibrary
    if (!runtime_path_.empty()) {
        args.push_back(runtime_path_ + "/libpawc_runtime.a");
    }
    
    // librarysearchpath
    for (const auto& path : library_paths) {
        args.push_back("-L" + path);
    }
    
    // library
    for (const auto& lib : libraries) {
        args.push_back("-l" + lib);
    }
    
    // system librariesandstartfile
#ifdef __APPLE__
    // macOSspecific
    args.push_back("-lSystem");
    args.push_back("-syslibroot");
    args.push_back("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk");
#else
    // Linux
    args.push_back("-lc");
    args.push_back("-lm");
    args.push_back("-lpthread");
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

