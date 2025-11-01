# Middleend

add_subdirectory(types)
add_subdirectory(symbol)
add_subdirectory(sema)

add_library(pawc_middleend INTERFACE)

target_link_libraries(pawc_middleend INTERFACE
    pawc_types
    pawc_symbol
    pawc_sema
)
