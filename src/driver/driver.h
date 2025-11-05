//===--- driver.h - Command Line Driver -------------------------*- C++ -*-===//
//
// PawLang Compiler - Command Line Driver
//
//===----------------------------------------------------------------------===//

#ifndef PAW_DRIVER_H
#define PAW_DRIVER_H

#include "options.h"
#include <string>

namespace pawc {

/// Driver - command line driver
///
/// Responsible for parsing command line parameters and calling Compiler
class Driver {
public:
    Driver();
    
    /// runcompiler
    /// \param argc parameter count
    /// \param argv parameterarray
    /// \return exit code (0 indicates success)
    int run(int argc, char** argv);
    
private:
    CompilerOptions options_;
    
    /// Parse command line parameters
    /// \return true indicates success
    bool parseArguments(int argc, char** argv);
    
    /// Print help information
    void printHelp();
    
    /// Print version information
    void printVersion();
};

} // namespace pawc

#endif // PAW_DRIVER_H
