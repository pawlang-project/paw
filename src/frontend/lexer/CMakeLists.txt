# Lexer

add_library(pawc_lexer STATIC
    token.cpp
    lexer.cpp
)

target_include_directories(pawc_lexer PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_lexer PUBLIC
    pawc_diagnostics
)
