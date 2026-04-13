if(NOT DEFINED RUNTIME_CACHE_DIR OR NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "RUNTIME_CACHE_DIR and DEST_DIR are required")
endif()

if(NOT EXISTS "${DEST_DIR}")
    file(MAKE_DIRECTORY "${DEST_DIR}")
endif()

if(NOT EXISTS "${RUNTIME_CACHE_DIR}/.ready")
    message(STATUS "Runtime cache not ready, skipping: ${RUNTIME_CACHE_DIR}")
    return()
endif()

file(GLOB cached_dlls LIST_DIRECTORIES false "${RUNTIME_CACHE_DIR}/*.dll")
foreach(dll_path IN LISTS cached_dlls)
    if(EXISTS "${dll_path}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${dll_path}"
                "${DEST_DIR}"
        )
    endif()
endforeach()

if(EXISTS "${RUNTIME_CACHE_DIR}/qt.conf")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${RUNTIME_CACHE_DIR}/qt.conf"
            "${DEST_DIR}/qt.conf"
    )
endif()

