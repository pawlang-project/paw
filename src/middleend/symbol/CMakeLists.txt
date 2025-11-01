# Symbol Table

add_library(pawc_symbol STATIC
    symbol_table.cpp
    builtin_symbols.cpp
)

target_include_directories(pawc_symbol PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_symbol PUBLIC
    pawc_types
)
