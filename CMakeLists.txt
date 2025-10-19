cmake_minimum_required(VERSION 3.20)
project(pawc C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# LLVM Integration - Direct Source Build (zig-bootstrap style)
# ============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Building LLVM from source (minimal)")
message(STATUS "========================================")
message(STATUS "")

# Configure LLVM build options BEFORE adding subdirectory
set(LLVM_ENABLE_PROJECTS "llvm" CACHE STRING "")
set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "")
set(LLVM_BUILD_TOOLS OFF CACHE BOOL "")
set(LLVM_BUILD_EXAMPLES OFF CACHE BOOL "")
set(LLVM_BUILD_TESTS OFF CACHE BOOL "")
set(LLVM_BUILD_BENCHMARKS OFF CACHE BOOL "")
set(LLVM_ENABLE_RTTI ON CACHE BOOL "")
set(LLVM_ENABLE_EH ON CACHE BOOL "")
set(LLVM_ENABLE_ZLIB OFF CACHE BOOL "")
set(LLVM_ENABLE_ZSTD OFF CACHE BOOL "")
set(LLVM_ENABLE_LIBXML2 OFF CACHE BOOL "")
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "")
set(LLVM_ENABLE_THREADS ON CACHE BOOL "")
set(LLVM_ENABLE_UNWIND_TABLES ON CACHE BOOL "")
set(LLVM_ENABLE_WARNINGS OFF CACHE BOOL "")
set(LLVM_ENABLE_WERROR OFF CACHE BOOL "")
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "")
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "")
set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "")

# Add LLVM as subdirectory (exclude from default build)
add_subdirectory(llvm EXCLUDE_FROM_ALL)

message(STATUS "LLVM source integration complete")
message(STATUS "")

# LLVM include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/llvm/include
    ${CMAKE_BINARY_DIR}/llvm/include
)

# Get LLVM components (minimal for code generation)
set(llvm_libs
    LLVMCore
    LLVMSupport
    LLVMIRReader
    LLVMX86CodeGen
    LLVMX86AsmParser
    LLVMX86Desc
    LLVMX86Info
    LLVMCodeGen
    LLVMTarget
    LLVMAnalysis
    LLVMTransformUtils
    LLVMScalarOpts
    LLVMipo
    LLVMAsmParser
    LLVMAsmPrinter
    LLVMMC
    LLVMMCParser
    LLVMObject
    LLVMBitReader
    LLVMBitWriter
)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files for main compiler
set(SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/codegen/codegen.cpp
    src/codegen/codegen_match.cpp
    src/codegen/codegen_stmt.cpp
    src/builtins/builtins.cpp
    src/module/module_loader.cpp
    src/module/symbol_table.cpp
    src/module/module_compiler.cpp
    src/colors.cpp
    src/toml_parser.cpp
    src/error_reporter.cpp
)

# Create main compiler executable
add_executable(pawc ${SOURCES})

# Ensure pawc depends on LLVM libraries being built
add_dependencies(pawc ${llvm_libs})

# Link LLVM libraries
target_link_libraries(pawc ${llvm_libs})

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(pawc PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(pawc PRIVATE /utf-8 /wd4819 /wd4624)
    target_compile_definitions(pawc PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Windows-specific LLVM definitions
if(WIN32)
    add_compile_definitions(LLVM_ON_WIN32=1)
endif()

# Installation
install(TARGETS pawc DESTINATION bin)

# Output information
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  PawLang Compiler - Integrated LLVM")
message(STATUS "========================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "LLVM: Building from source (X86 only)")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
message(STATUS "")
