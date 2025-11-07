# Runtime Library

add_library(pawc_runtime STATIC
    builtin_runtime.cpp
    software_f128.cpp
)

target_include_directories(pawc_runtime PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/llvm/include
    ${CMAKE_BINARY_DIR}/llvm/include
)

# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# CRITICAL: Force static CRT linkage for MSVC
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
if(MSVC)
    # Use static CRT (/MT for Release, /MTd for Debug)
    set_property(TARGET pawc_runtime PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 注意：暂不链接LLVM到Runtime（避免依赖复杂度）
# 未来实现Dragon4算法时可能需要
