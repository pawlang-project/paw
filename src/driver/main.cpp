//===--- main.cpp - Compiler Entry Point -------------------------*- C++ -*-===//
/// @file main.cpp
/// @brief Implementation file
//
// PawLang Compiler - Main Entry Point
//
//===----------------------------------------------------------------------===//

#include "driver.h"

using namespace pawc;

int main(int argc, char** argv) {
    Driver driver;
    return driver.run(argc, argv);
}
