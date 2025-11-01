# Transform Passes

add_library(pawc_transform_passes STATIC
    monomorphization_pass.cpp
)

target_include_directories(pawc_transform_passes PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_transform_passes
    pawc_pass
    pawc_parser
    pawc_types
)

