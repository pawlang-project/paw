# Pass System Core

add_library(pawc_pass STATIC
    pass_manager.cpp
)

target_include_directories(pawc_pass PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
