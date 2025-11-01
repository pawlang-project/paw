# Parser

add_library(pawc_parser STATIC
    parser.cpp
    ast/expr.cpp
    ast/stmt.cpp
    ast/pattern.cpp
    ast_printer.cpp
)

target_include_directories(pawc_parser PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_parser PUBLIC
    pawc_lexer
    pawc_diagnostics
    pawc_types
)
