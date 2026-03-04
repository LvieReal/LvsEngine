if(NOT DEFINED SOURCE_DIR OR NOT DEFINED DIST_DIR OR NOT DEFINED EXE_NAME)
    message(FATAL_ERROR "SOURCE_DIR, DIST_DIR, and EXE_NAME are required")
endif()

if(NOT EXISTS "${SOURCE_DIR}/${EXE_NAME}")
    message(FATAL_ERROR "Executable not found: ${SOURCE_DIR}/${EXE_NAME}")
endif()

file(REMOVE_RECURSE "${DIST_DIR}")
file(MAKE_DIRECTORY "${DIST_DIR}")

file(COPY "${SOURCE_DIR}/${EXE_NAME}" DESTINATION "${DIST_DIR}")

set(plugin_dirs
    generic
    iconengines
    imageformats
    networkinformation
    platforms
    styles
    tls
    translations
)

if(DEFINED RUNTIME_CACHE_DIR AND EXISTS "${RUNTIME_CACHE_DIR}/.ready")
    file(GLOB cached_dlls LIST_DIRECTORIES false "${RUNTIME_CACHE_DIR}/*.dll")
    if(cached_dlls)
        file(COPY ${cached_dlls} DESTINATION "${DIST_DIR}")
    endif()

    if(EXISTS "${RUNTIME_CACHE_DIR}/qt.conf")
        file(COPY "${RUNTIME_CACHE_DIR}/qt.conf" DESTINATION "${DIST_DIR}")
    endif()

    foreach(plugin_dir IN LISTS plugin_dirs)
        if(EXISTS "${RUNTIME_CACHE_DIR}/${plugin_dir}")
            file(COPY "${RUNTIME_CACHE_DIR}/${plugin_dir}" DESTINATION "${DIST_DIR}")
        endif()
    endforeach()
elseif(DEFINED WINDEPLOYQT AND EXISTS "${WINDEPLOYQT}")
    execute_process(
        COMMAND "${WINDEPLOYQT}"
            --dir "${DIST_DIR}"
            --no-opengl-sw
            --no-translations
            "${DIST_DIR}/${EXE_NAME}"
        RESULT_VARIABLE deploy_result
    )
    if(NOT deploy_result EQUAL 0)
        message(FATAL_ERROR "windeployqt failed with exit code ${deploy_result}")
    endif()
endif()

if(DEFINED ASAN_RUNTIME_DLL AND EXISTS "${ASAN_RUNTIME_DLL}")
    file(COPY "${ASAN_RUNTIME_DLL}" DESTINATION "${DIST_DIR}")
endif()

if(EXISTS "${SOURCE_DIR}/Lvs")
    file(COPY "${SOURCE_DIR}/Lvs" DESTINATION "${DIST_DIR}")
endif()

if(EXISTS "${SOURCE_DIR}/config")
    file(COPY "${SOURCE_DIR}/config" DESTINATION "${DIST_DIR}")
endif()
