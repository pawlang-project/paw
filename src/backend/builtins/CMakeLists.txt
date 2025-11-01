# Builtin Functions Code Generation

add_library(pawc_builtins STATIC
    builtin_codegen.cpp
)

target_include_directories(pawc_builtins PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(pawc_builtins PUBLIC
    pawc_codegen
    pawc_types
)
