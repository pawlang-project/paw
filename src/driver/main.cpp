//===--- main.cpp - Compiler Entry Point -------------------------*- C++ -*-===//
/// @file main.cpp
/// @brief Implementation file
//
// PawLang Compiler - Main Entry Point
//
//===----------------------------------------------------------------------===//

#include "driver.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace pawc;

int main(int argc, char** argv) {
#ifdef _WIN32
    // Set console to UTF-8 on Windows
    // This allows proper display of Unicode characters (emoji, box-drawing, etc.)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    Driver driver;
    return driver.run(argc, argv);
}
