# Types System

add_library(pawc_types STATIC
    type.cpp
    type_system.cpp
    primitive_types.cpp
    composite_types.cpp
    generic_types.cpp
    type_registry.cpp
)

target_include_directories(pawc_types PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_types PUBLIC
    pawc_utils
)
