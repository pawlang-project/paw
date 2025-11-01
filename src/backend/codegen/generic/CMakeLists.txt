# Generic CodeGen模块

add_library(pawc_codegen_generic STATIC
    mangling.cpp
    monomorphization.cpp
    generic_instantiator.cpp
)

target_include_directories(pawc_codegen_generic PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(pawc_codegen_generic PUBLIC
    pawc_types
    ${LLVM_LIBS}
)

