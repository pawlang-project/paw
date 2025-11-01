# Backend

add_subdirectory(codegen)
add_subdirectory(builtins)

add_library(pawc_backend INTERFACE)

target_link_libraries(pawc_backend INTERFACE
    pawc_codegen
    pawc_builtins
    pawc_runtime
)
