# Utils Library

add_library(pawc_utils INTERFACE)

target_include_directories(pawc_utils INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_utils INTERFACE
    siphash
)
