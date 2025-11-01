# Interface CodeGen模块

add_library(pawc_codegen_interface STATIC
    interface_codegen.cpp
)

target_include_directories(pawc_codegen_interface PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(pawc_codegen_interface PUBLIC
    pawc_types
    ${LLVM_LIBS}
)

