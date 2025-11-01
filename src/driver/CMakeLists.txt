# Driver模块

add_library(pawc_driver STATIC
    compiler.cpp
    driver.cpp
    linker.cpp
)

target_include_directories(pawc_driver PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pawc_driver PUBLIC
    pawc_frontend
    pawc_middleend
    pawc_backend
    pawc_pass
    pawc_transform_passes
    pawc_diagnostics
    pawc_utils
)
