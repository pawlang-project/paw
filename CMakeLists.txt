cmake_minimum_required(VERSION 3.20)
project(PawLang VERSION 1.4.0 LANGUAGES CXX C)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# PawLang 编译器 - Pass-Based架构
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "")
message(STATUS "╔════════════════════════════════════════╗")
message(STATUS "║   PawLang Compiler v${PROJECT_VERSION}        ║")
message(STATUS "║   LLVM 21.1.0                    ║")
message(STATUS "╚════════════════════════════════════════╝")
message(STATUS "")

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# LLVM Integration
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# 启用Clang和LLD项目
set(LLVM_ENABLE_PROJECTS "clang;lld" CACHE STRING "Semicolon-separated list of projects to build" FORCE)

# 设置外部源目录
set(LLVM_EXTERNAL_CLANG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/clang" CACHE PATH "" FORCE)
set(LLVM_EXTERNAL_LLD_SOURCE_DIR "${CMAKE_SOURCE_DIR}/lld" CACHE PATH "" FORCE)

# 验证目录存在
if(EXISTS "${CMAKE_SOURCE_DIR}/clang")
    message(STATUS "[OK] Clang source found")
else()
    message(FATAL_ERROR "[ERROR] Clang source not found")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/lld")
    message(STATUS "[OK] LLD source found")
else()
    message(FATAL_ERROR "[ERROR] LLD source not found")
endif()

# 根据平台自动选择目标
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
    set(LLVM_TARGETS_TO_BUILD "AArch64" CACHE STRING "")
    set(PLATFORM_TARGET "AArch64")
    message(STATUS "Detected AArch64 - building AArch64 target")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x64")
    set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "")
    set(PLATFORM_TARGET "X86")
    message(STATUS "Detected x86_64 - building X86 target")
else()
    set(LLVM_TARGETS_TO_BUILD "X86;AArch64" CACHE STRING "")
    set(PLATFORM_TARGET "X86;AArch64")
    message(STATUS "Building X86 and AArch64 for compatibility")
endif()

# LLVM最小化配置
set(LLVM_BUILD_TOOLS ON CACHE BOOL "")
set(LLVM_BUILD_EXAMPLES OFF CACHE BOOL "")
set(LLVM_BUILD_TESTS OFF CACHE BOOL "")
set(LLVM_BUILD_BENCHMARKS OFF CACHE BOOL "")
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "")
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "")
set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "")
set(LLVM_ENABLE_DOXYGEN OFF CACHE BOOL "")
set(LLVM_ENABLE_SPHINX OFF CACHE BOOL "")
set(LLVM_ENABLE_RTTI ON CACHE BOOL "")
set(LLVM_ENABLE_EH ON CACHE BOOL "")
set(LLVM_ENABLE_WARNINGS OFF CACHE BOOL "")

# Clang配置
set(CLANG_ENABLE_ARCMT OFF CACHE BOOL "")
set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "")
set(CLANG_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CLANG_INCLUDE_TESTS OFF CACHE BOOL "")
set(CLANG_INCLUDE_DOCS OFF CACHE BOOL "")

# LLD配置
set(LLD_BUILD_TOOLS ON CACHE BOOL "")
set(LLD_INCLUDE_TESTS OFF CACHE BOOL "")

# 应用最小补丁（参考您的原版本）
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

# LLVM docs补丁
set(LLVM_CMAKE "${CMAKE_SOURCE_DIR}/llvm/CMakeLists.txt")
if(EXISTS ${LLVM_CMAKE})
    file(READ ${LLVM_CMAKE} LLVM_CONTENT)
    if(NOT LLVM_CONTENT MATCHES "if.*EXISTS.*docs.*LLVM_INCLUDE_DOCS")
        string(REPLACE "add_subdirectory(docs)" "if(LLVM_INCLUDE_DOCS AND EXISTS \"\${CMAKE_CURRENT_SOURCE_DIR}/docs\")\n  add_subdirectory(docs)\nendif()" LLVM_CONTENT "${LLVM_CONTENT}")
        string(REPLACE "add_subdirectory(benchmarks)" "if(LLVM_INCLUDE_BENCHMARKS AND EXISTS \"\${CMAKE_CURRENT_SOURCE_DIR}/benchmarks\")\n  add_subdirectory(benchmarks)\nendif()" LLVM_CONTENT "${LLVM_CONTENT}")
        file(WRITE ${LLVM_CMAKE} "${LLVM_CONTENT}")
        message(STATUS "Applied patch: LLVM docs/benchmarks directory")
    endif()
endif()

# 添加LLVM子项目（使用EXCLUDE_FROM_ALL避免默认构建所有）
add_subdirectory(llvm EXCLUDE_FROM_ALL)

# LLVM include目录
include_directories(
    ${CMAKE_SOURCE_DIR}/llvm/include
    ${CMAKE_BINARY_DIR}/llvm/include
)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 第三方库
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

message(STATUS "Using bundled SipHash from third-party/siphash")
add_library(siphash INTERFACE)
target_include_directories(siphash INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/siphash/include
)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 生成配置头文件
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/pawc/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/pawc/version.h"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/pawc/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/pawc/config.h"
)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# PawLang编译器模块（按依赖顺序）
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# 1. 基础设施
add_subdirectory(src/utils)
add_subdirectory(src/diagnostics)
add_subdirectory(src/pass)
add_subdirectory(src/runtime)

# 2. 中间表示
add_subdirectory(src/middleend)

# 3. 前端
add_subdirectory(src/frontend)

# 4. 后端
add_subdirectory(src/backend)

# 5. Pass实现
add_subdirectory(src/passes)

# 6. 驱动
add_subdirectory(src/driver)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# PawLang编译器可执行文件
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# LLVM库列表（根据平台）
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
    LLVMPasses
)

# 根据平台添加目标库
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64")
    list(APPEND llvm_libs
        LLVMAArch64CodeGen
        LLVMAArch64AsmParser
        LLVMAArch64Desc
        LLVMAArch64Info
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x64|i386|i686|x86")
    list(APPEND llvm_libs
        LLVMX86CodeGen
        LLVMX86AsmParser
        LLVMX86Desc
        LLVMX86Info
    )
else()
    list(APPEND llvm_libs
        LLVMX86CodeGen LLVMX86AsmParser LLVMX86Desc LLVMX86Info
        LLVMAArch64CodeGen LLVMAArch64AsmParser LLVMAArch64Desc LLVMAArch64Info
    )
endif()

add_executable(pawc src/driver/main.cpp)

set(pawc_all_libs
    pawc_driver
    pawc_pass
    pawc_runtime
    ${llvm_libs}
)
list(REMOVE_DUPLICATES pawc_all_libs)

target_link_libraries(pawc PRIVATE
    ${pawc_all_libs}
)

target_include_directories(pawc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 运行时库路径配置 - 优雅的自动检测方案
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# 根据不同的生成器和平台构建运行时库路径
if(MSVC OR CMAKE_GENERATOR MATCHES "Visual Studio")
    # Visual Studio multi-config generator: 运行时库在 ${CMAKE_BUILD_TYPE} 子目录中
    set(PAWC_RUNTIME_RELATIVE_PATH "src/runtime/$<CONFIG>")
else()
    # Unix Makefiles / Ninja single-config generator: 运行时库直接在 src/runtime 目录中
    set(PAWC_RUNTIME_RELATIVE_PATH "src/runtime")
endif()

# 将运行时库路径传递给编译器（相对于构建目录）
target_compile_definitions(pawc_driver PRIVATE
    PAWC_RUNTIME_PATH_HINT="${CMAKE_BINARY_DIR}/${PAWC_RUNTIME_RELATIVE_PATH}"
)

message(STATUS "Runtime library path hint: ${CMAKE_BINARY_DIR}/${PAWC_RUNTIME_RELATIVE_PATH}")

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 安装配置
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

install(TARGETS pawc
    RUNTIME DESTINATION bin
)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 构建信息
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

message(STATUS "")
message(STATUS "╔════════════════════════════════════════╗")
message(STATUS "║   PawLang Build Configuration          ║")
message(STATUS "╠════════════════════════════════════════╣")
message(STATUS "║ Version:           ${PROJECT_VERSION}")
message(STATUS "║ Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "║ C++ Compiler:      ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "║ LLVM Version:      21.1.0")
message(STATUS "║ Targets:           X86;AArch64")
message(STATUS "╚════════════════════════════════════════╝")
message(STATUS "")
