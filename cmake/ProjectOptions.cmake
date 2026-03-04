set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "This project currently supports MinGW GCC/Clang only.")
endif()

option(LVS_ENABLE_ASAN "Enable AddressSanitizer instrumentation" OFF)
option(LVS_STATIC_RUNTIME "Link compiler runtime libraries statically on Windows where possible" ON)

add_compile_options(-Wall -Wextra -Wpedantic)

if(LVS_ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
