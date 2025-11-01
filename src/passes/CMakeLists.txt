# Concrete Pass Implementations

add_subdirectory(transform)

add_library(pawc_passes INTERFACE)

target_include_directories(pawc_passes INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_passes INTERFACE
    pawc_pass
    pawc_backend
    pawc_transform_passes
)

