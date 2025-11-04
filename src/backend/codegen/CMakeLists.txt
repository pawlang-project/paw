# Code Generation - 模块化架构

add_library(pawc_codegen STATIC
    codegen_context.cpp
    type/type_codegen.cpp
    expr/expr_codegen.cpp
    expr/literal_codegen.cpp
    expr/binary_codegen.cpp
    expr/result_codegen.cpp
    expr/closure_codegen.cpp
    expr/call_codegen.cpp
    expr/match_codegen.cpp
    stmt/stmt_codegen.cpp
    generic/mangling.cpp
    generic/monomorphization.cpp
    generic/generic_instantiator.cpp
    interface/interface_codegen.cpp
    memory/memory_manager.cpp
)

target_include_directories(pawc_codegen PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(pawc_codegen PUBLIC
    pawc_types
    pawc_symbol
    pawc_parser
    ${llvm_libs}
)

add_dependencies(pawc_codegen LLVMCore LLVMSupport)
