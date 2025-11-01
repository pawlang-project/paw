# Diagnostics System

add_library(pawc_diagnostics INTERFACE)

target_include_directories(pawc_diagnostics INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
