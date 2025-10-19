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
# Enable Clang and LLD projects
set(LLVM_ENABLE_PROJECTS "clang;lld" CACHE STRING "Semicolon-separated list of projects to build" FORCE)

# Point to external Clang and LLD source directories
set(LLVM_EXTERNAL_CLANG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/clang" CACHE PATH "" FORCE)
set(LLVM_EXTERNAL_LLD_SOURCE_DIR "${CMAKE_SOURCE_DIR}/lld" CACHE PATH "" FORCE)

# Verify directories exist
if(EXISTS "${CMAKE_SOURCE_DIR}/clang")
    message(STATUS "✓ Clang source found: ${CMAKE_SOURCE_DIR}/clang")
else()
    message(FATAL_ERROR "✗ Clang source not found at: ${CMAKE_SOURCE_DIR}/clang")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/lld")
    message(STATUS "✓ LLD source found: ${CMAKE_SOURCE_DIR}/lld")
else()
    message(FATAL_ERROR "✗ LLD source not found at: ${CMAKE_SOURCE_DIR}/lld")
endif()

# Enable building tools (required for clang and lld executables)
set(LLVM_BUILD_TOOLS ON CACHE BOOL "" FORCE)

message(STATUS "LLVM_ENABLE_PROJECTS: ${LLVM_ENABLE_PROJECTS}")
message(STATUS "LLVM_BUILD_TOOLS: ${LLVM_BUILD_TOOLS}")

# Clang build options (minimal)
set(CLANG_ENABLE_ARCMT OFF CACHE BOOL "")
set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "")
set(CLANG_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CLANG_INCLUDE_DOCS OFF CACHE BOOL "")
set(CLANG_INCLUDE_TESTS OFF CACHE BOOL "")

# LLD build options (minimal)
set(LLD_BUILD_TOOLS ON CACHE BOOL "")
set(LLD_INCLUDE_TESTS OFF CACHE BOOL "")

# 根据当前平台和架构自动选择LLVM目标（减少编译时间和二进制大小）
# 参考：
# - Windows: x86_64, aarch64, x86
# - macOS: x86_64, aarch64  
# - Linux: x86_64, aarch64, arm, riscv64, powerpc64le, x86, loongarch64, s390x

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
    # ARM64/AArch64 架构（Apple Silicon、服务器ARM、移动设备）
    set(LLVM_TARGETS_TO_BUILD "AArch64" CACHE STRING "")
    set(PLATFORM_TARGET "AArch64")
    message(STATUS "Detected AArch64 (ARM64) - building AArch64 target only")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x64")
    # x86_64 架构（主流桌面和服务器）
    set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "")
    set(PLATFORM_TARGET "X86")
    message(STATUS "Detected x86_64 - building X86 target only")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
    # 32位 x86 架构（旧PC、嵌入式）
    set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "")
    set(PLATFORM_TARGET "X86")
    message(STATUS "Detected x86 (32-bit) - building X86 target only")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
    # ARM 32位架构（嵌入式、旧移动设备）
    set(LLVM_TARGETS_TO_BUILD "ARM" CACHE STRING "")
    set(PLATFORM_TARGET "ARM")
    message(STATUS "Detected ARM (32-bit) - building ARM target only")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    # RISC-V 64位架构（新兴开源架构）
    set(LLVM_TARGETS_TO_BUILD "RISCV" CACHE STRING "")
    set(PLATFORM_TARGET "RISCV")
    message(STATUS "Detected RISC-V 64-bit - building RISCV target only")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le|powerpc64le")
    # PowerPC 64位小端架构（IBM服务器）
    set(LLVM_TARGETS_TO_BUILD "PowerPC" CACHE STRING "")
    set(PLATFORM_TARGET "PowerPC")
    message(STATUS "Detected PowerPC 64LE - building PowerPC target only")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "loongarch64")
    # LoongArch 64位架构（中国龙芯）
    set(LLVM_TARGETS_TO_BUILD "LoongArch" CACHE STRING "")
    set(PLATFORM_TARGET "LoongArch")
    message(STATUS "Detected LoongArch 64-bit - building LoongArch target only")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "s390x")
    # IBM z/Architecture（大型机）
    set(LLVM_TARGETS_TO_BUILD "SystemZ" CACHE STRING "")
    set(PLATFORM_TARGET "SystemZ")
    message(STATUS "Detected s390x - building SystemZ target only")
    
else()
    # 未知平台，编译常用的两个目标（兼容性）
    set(LLVM_TARGETS_TO_BUILD "X86;AArch64" CACHE STRING "")
    set(PLATFORM_TARGET "X86;AArch64")
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "Building X86 and AArch64 targets for compatibility")
endif()

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

# Apply minimal patches for clang and lld (zig-bootstrap style)
set(CLANG_CMAKE "${CMAKE_SOURCE_DIR}/clang/CMakeLists.txt")
if(EXISTS ${CLANG_CMAKE})
    file(READ ${CLANG_CMAKE} CLANG_CONTENT)
    if(NOT CLANG_CONTENT MATCHES "if.*CLANG_BUILD_EXAMPLES")
        string(REPLACE "add_subdirectory(examples)" "if(CLANG_BUILD_EXAMPLES AND EXISTS \"\${CMAKE_CURRENT_SOURCE_DIR}/examples\")\n  add_subdirectory(examples)\nendif()" CLANG_CONTENT "${CLANG_CONTENT}")
        file(WRITE ${CLANG_CMAKE} "${CLANG_CONTENT}")
        message(STATUS "Applied patch: Clang examples directory")
    endif()
endif()

set(LLD_CMAKE "${CMAKE_SOURCE_DIR}/lld/CMakeLists.txt")
if(EXISTS ${LLD_CMAKE})
    file(READ ${LLD_CMAKE} LLD_CONTENT)
    if(NOT LLD_CONTENT MATCHES "if.*EXISTS.*docs")
        string(REPLACE "add_subdirectory(docs)" "if(EXISTS \"\${CMAKE_CURRENT_SOURCE_DIR}/docs\")\n  add_subdirectory(docs)\nendif()" LLD_CONTENT "${LLD_CONTENT}")
        file(WRITE ${LLD_CMAKE} "${LLD_CONTENT}")
        message(STATUS "Applied patch: LLD docs directory")
    endif()
endif()

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
# 根据平台动态添加目标库
set(llvm_libs
        LLVMCore
        LLVMSupport
        LLVMIRReader
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

# 根据检测到的平台添加相应的目标库
if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
    # AArch64 (ARM 64-bit)
    list(APPEND llvm_libs
        LLVMAArch64CodeGen
        LLVMAArch64AsmParser
        LLVMAArch64Desc
        LLVMAArch64Info
    )
    message(STATUS "Linking AArch64 libraries")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x64")
    # x86-64 (64-bit Intel/AMD)
    list(APPEND llvm_libs
        LLVMX86CodeGen
        LLVMX86AsmParser
        LLVMX86Desc
        LLVMX86Info
    )
    message(STATUS "Linking X86 libraries")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
    # x86 (32-bit)
    list(APPEND llvm_libs
        LLVMX86CodeGen
        LLVMX86AsmParser
        LLVMX86Desc
        LLVMX86Info
    )
    message(STATUS "Linking X86 libraries (32-bit)")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
    # ARM (32-bit)
    list(APPEND llvm_libs
        LLVMARMCodeGen
        LLVMARMAsmParser
        LLVMARMDesc
        LLVMARMInfo
    )
    message(STATUS "Linking ARM libraries (32-bit)")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    # RISC-V (64-bit)
    list(APPEND llvm_libs
        LLVMRISCVCodeGen
        LLVMRISCVAsmParser
        LLVMRISCVDesc
        LLVMRISCVInfo
    )
    message(STATUS "Linking RISC-V libraries")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le|powerpc64le")
    # PowerPC (64-bit little-endian)
    list(APPEND llvm_libs
        LLVMPowerPCCodeGen
        LLVMPowerPCAsmParser
        LLVMPowerPCDesc
        LLVMPowerPCInfo
    )
    message(STATUS "Linking PowerPC libraries")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "loongarch64")
    # LoongArch (64-bit)
    list(APPEND llvm_libs
        LLVMLoongArchCodeGen
        LLVMLoongArchAsmParser
        LLVMLoongArchDesc
        LLVMLoongArchInfo
    )
    message(STATUS "Linking LoongArch libraries")
    
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "s390x")
    # IBM System z (s390x)
    list(APPEND llvm_libs
        LLVMSystemZCodeGen
        LLVMSystemZAsmParser
        LLVMSystemZDesc
        LLVMSystemZInfo
    )
    message(STATUS "Linking SystemZ libraries")
    
else()
    # 未知平台：编译最常用的两个目标（兼容性）
    list(APPEND llvm_libs
        LLVMX86CodeGen
        LLVMX86AsmParser
        LLVMX86Desc
        LLVMX86Info
        LLVMAArch64CodeGen
        LLVMAArch64AsmParser
        LLVMAArch64Desc
        LLVMAArch64Info
    )
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "Linking both X86 and AArch64 libraries for compatibility")
endif()

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
        src/codegen/codegen_expr.cpp
        src/codegen/codegen_struct.cpp
        src/codegen/codegen_type.cpp
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

# Also depend on clang and lld if they are being built
if(TARGET clang)
    add_dependencies(pawc clang)
    message(STATUS "pawc will depend on clang")
endif()
if(TARGET lld)
    add_dependencies(pawc lld)
    message(STATUS "pawc will depend on lld")
endif()

# Expose clang and lld paths for runtime use
if(TARGET clang)
    set(CLANG_EXECUTABLE "$<TARGET_FILE:clang>" CACHE INTERNAL "Path to clang executable")
    message(STATUS "Clang executable will be available at: ${CMAKE_BINARY_DIR}/bin/clang")
endif()
if(TARGET lld)
    set(LLD_EXECUTABLE "$<TARGET_FILE:lld>" CACHE INTERNAL "Path to lld executable")
    message(STATUS "LLD executable will be available at: ${CMAKE_BINARY_DIR}/bin/lld")
endif()

# Link LLVM libraries
target_link_libraries(pawc ${llvm_libs})

# Note: Using dynamic linking with MinGW
# Required DLLs will be copied to the output directory

# Compiler options
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(pawc PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif (MSVC)
    target_compile_options(pawc PRIVATE /utf-8 /wd4819 /wd4624)
    target_compile_definitions(pawc PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()

# Windows-specific LLVM definitions
if (WIN32)
    add_compile_definitions(LLVM_ON_WIN32=1)
endif ()

# Copy stdlib to build directory
file(COPY stdlib DESTINATION ${CMAKE_BINARY_DIR})

# Installation
install(TARGETS pawc DESTINATION bin)
install(DIRECTORY stdlib DESTINATION .)

# Install clang and lld if they are built
if(TARGET clang)
    install(TARGETS clang DESTINATION bin)
    message(STATUS "clang will be installed to bin/")
endif()
if(TARGET lld)
    install(TARGETS lld DESTINATION bin)
    message(STATUS "lld will be installed to bin/")
endif()

# Output information
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  PawLang Compiler - Integrated LLVM")
message(STATUS "========================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "LLVM: Building from source")
message(STATUS "Target Architecture: ${PLATFORM_TARGET}")
message(STATUS "Clang: Enabled (as build tool)")
message(STATUS "LLD: Enabled (as build tool)")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
message(STATUS "")
