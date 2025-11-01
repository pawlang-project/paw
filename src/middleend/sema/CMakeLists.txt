# Semantic Analysis

add_library(pawc_sema STATIC
    type_checker.cpp
    semantic_context.cpp
    type_inference.cpp
    interface_validator.cpp
    pattern_checker.cpp
    capture_analyzer.cpp
)

target_include_directories(pawc_sema PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_sema PUBLIC
    pawc_types
    pawc_symbol
    pawc_parser
    pawc_diagnostics
)
