# Frontend

add_subdirectory(lexer)
add_subdirectory(parser)

add_library(pawc_frontend INTERFACE)

target_link_libraries(pawc_frontend INTERFACE
    pawc_lexer
    pawc_parser
)
