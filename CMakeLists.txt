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
    
    # 创建一个临时的CMake项目来编译设置工具
    set(SETUP_BUILD_DIR "${CMAKE_BINARY_DIR}/setup_build")
    file(MAKE_DIRECTORY ${SETUP_BUILD_DIR})
    
    # 创建临时的CMakeLists.txt
    file(WRITE "${SETUP_BUILD_DIR}/CMakeLists.txt" "
cmake_minimum_required(VERSION 3.20)
project(pawc_setup CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(pawc-setup 
    \"${CMAKE_SOURCE_DIR}/src/setup.cpp\"
    \"${CMAKE_SOURCE_DIR}/src/llvm_downloader.cpp\")
if(MSVC)
    target_compile_options(pawc-setup PRIVATE /utf-8)
endif()
")
    
    # 配置临时项目
    execute_process(
        COMMAND ${CMAKE_COMMAND} .
        WORKING_DIRECTORY ${SETUP_BUILD_DIR}
        RESULT_VARIABLE CONFIG_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "无法配置LLVM下载工具")
    endif()
    
    # 编译临时项目
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --config Release
        WORKING_DIRECTORY ${SETUP_BUILD_DIR}
        RESULT_VARIABLE BUILD_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "无法编译LLVM下载工具")
    endif()
    
    # 查找生成的可执行文件
    if(WIN32)
        if(EXISTS "${SETUP_BUILD_DIR}/Release/pawc-setup.exe")
            set(SETUP_EXECUTABLE "${SETUP_BUILD_DIR}/Release/pawc-setup.exe")
        else()
            set(SETUP_EXECUTABLE "${SETUP_BUILD_DIR}/pawc-setup.exe")
        endif()
    else()
        set(SETUP_EXECUTABLE "${SETUP_BUILD_DIR}/pawc-setup")
    endif()
    
    if(NOT EXISTS ${SETUP_EXECUTABLE})
        message(FATAL_ERROR "找不到编译好的设置工具: ${SETUP_EXECUTABLE}")
    endif()
    
    # 运行下载
    execute_process(
        COMMAND ${SETUP_EXECUTABLE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE DOWNLOAD_RESULT
        OUTPUT_VARIABLE DOWNLOAD_OUTPUT
        ERROR_VARIABLE DOWNLOAD_ERROR
    )
    
    if(NOT DOWNLOAD_RESULT EQUAL 0)
        message(STATUS "下载输出: ${DOWNLOAD_OUTPUT}")
        message(STATUS "下载错误: ${DOWNLOAD_ERROR}")
        message(FATAL_ERROR "LLVM下载失败")
    endif()
    
    # 清理临时文件
    file(REMOVE_RECURSE ${SETUP_BUILD_DIR})
    
    message(STATUS "")
    message(STATUS "✓ LLVM下载完成")
    message(STATUS "")
endif()

# 只使用预编译LLVM
set(LLVM_DIR "${LLVM_LOCAL_PATH}/lib/cmake/llvm")

# 为预编译LLVM创建缺失的依赖target（如果不存在）
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB INTERFACE IMPORTED)
endif()
if(NOT TARGET zstd::libzstd_shared)
    add_library(zstd::libzstd_shared INTERFACE IMPORTED)
endif()
if(NOT TARGET zstd::libzstd_static)
    add_library(zstd::libzstd_static INTERFACE IMPORTED)
endif()

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
elseif(MSVC)
    target_compile_options(pawc PRIVATE /utf-8 /wd4819)
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
