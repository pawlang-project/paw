cmake_minimum_required(VERSION 3.20)
project(pawc C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 检查预编译LLVM是否存在，不存在则自动下载
set(LLVM_LOCAL_PATH "${CMAKE_SOURCE_DIR}/llvm")

if(NOT EXISTS "${LLVM_LOCAL_PATH}/lib/cmake/llvm")
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "  未找到预编译LLVM - 开始自动下载")
    message(STATUS "========================================")
    message(STATUS "")
    
    # 编译下载工具 (使用src/目录中的代码)
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 
                ${CMAKE_SOURCE_DIR}/src/setup.cpp 
                ${CMAKE_SOURCE_DIR}/src/llvm_downloader.cpp 
                -o ${CMAKE_BINARY_DIR}/pawc-setup
        RESULT_VARIABLE COMPILE_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT COMPILE_RESULT EQUAL 0)
        message(FATAL_ERROR "无法编译LLVM下载工具")
    endif()
    
    # 运行下载
    execute_process(
        COMMAND ${CMAKE_BINARY_DIR}/pawc-setup
        RESULT_VARIABLE DOWNLOAD_RESULT
    )
    
    if(NOT DOWNLOAD_RESULT EQUAL 0)
        message(FATAL_ERROR "LLVM下载失败")
    endif()
    
    # 清理临时文件
    file(REMOVE ${CMAKE_BINARY_DIR}/pawc-setup)
    
    message(STATUS "")
    message(STATUS "✓ LLVM下载完成")
    message(STATUS "")
endif()

# 只使用预编译LLVM
set(LLVM_DIR "${LLVM_LOCAL_PATH}/lib/cmake/llvm")

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# LLVM configuration
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Get LLVM components
# 使用AllTargetsAsmParsers和AllTargetsCodeGens来自动包含所有可用的target
llvm_map_components_to_libnames(llvm_libs
    Core
    Support
    irreader
    native
    Object
    MC
    MCParser
    CodeGen
    AsmParser
    AsmPrinter
    Target
    Linker
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

# Link LLVM libraries
target_link_libraries(pawc ${llvm_libs})

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(pawc PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Installation
install(TARGETS pawc DESTINATION bin)

# Output information
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  PawLang Compiler (pawc)")
message(STATUS "========================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "LLVM Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
message(STATUS "")
