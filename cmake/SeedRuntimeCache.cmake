if(NOT DEFINED SOURCE_DIR OR NOT DEFINED EXE_NAME OR NOT DEFINED RUNTIME_CACHE_DIR OR NOT DEFINED WINDEPLOYQT OR NOT DEFINED QT_BIN_DIR OR NOT DEFINED COMPILER_BIN_DIR)
    message(FATAL_ERROR "SOURCE_DIR, EXE_NAME, RUNTIME_CACHE_DIR, WINDEPLOYQT, QT_BIN_DIR, and COMPILER_BIN_DIR are required")
endif()

if(NOT EXISTS "${SOURCE_DIR}/${EXE_NAME}")
    message(FATAL_ERROR "Executable not found: ${SOURCE_DIR}/${EXE_NAME}")
endif()

if(NOT EXISTS "${WINDEPLOYQT}")
    message(FATAL_ERROR "windeployqt not found: ${WINDEPLOYQT}")
endif()

if(NOT DEFINED OBJDUMP_EXECUTABLE OR NOT EXISTS "${OBJDUMP_EXECUTABLE}")
    message(FATAL_ERROR "objdump executable not found: ${OBJDUMP_EXECUTABLE}")
endif()

function(is_system_dll dll_name out_var)
    string(TOLOWER "${dll_name}" dll_lower)
    if(dll_lower MATCHES "^api-ms-win-" OR dll_lower MATCHES "^ext-ms-win-")
        set(${out_var} TRUE PARENT_SCOPE)
    elseif(dll_lower MATCHES "^(kernel32|user32|gdi32|advapi32|shell32|ole32|oleaut32|comdlg32|winspool|ws2_32|bcrypt|crypt32|dwmapi|imm32|setupapi|version|winmm|shlwapi|shcore|uxtheme|mpr|netapi32|wtsapi32|dwrite|dxgi|d3d11|d3d12|vulkan-1|msvcrt|ntdll)\\.dll$")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(resolve_and_copy_runtime root_binary)
    set(processed "")
    set(queue "${root_binary}")

    while(queue)
        list(POP_FRONT queue current_bin)
        if(NOT EXISTS "${current_bin}")
            continue()
        endif()
        list(FIND processed "${current_bin}" processed_index)
        if(NOT processed_index EQUAL -1)
            continue()
        endif()
        list(APPEND processed "${current_bin}")

        execute_process(
            COMMAND "${OBJDUMP_EXECUTABLE}" -p "${current_bin}"
            OUTPUT_VARIABLE objdump_output
            ERROR_QUIET
        )
        string(REGEX MATCHALL "DLL Name: [^\r\n]+" dll_lines "${objdump_output}")

        foreach(dll_line IN LISTS dll_lines)
            string(REGEX REPLACE "DLL Name: " "" dll_name "${dll_line}")
            string(STRIP "${dll_name}" dll_name)
            if(NOT dll_name)
                continue()
            endif()

            is_system_dll("${dll_name}" dll_is_system)
            if(dll_is_system)
                continue()
            endif()

            set(found_path "")
            foreach(search_dir IN LISTS search_dirs)
                if(EXISTS "${search_dir}/${dll_name}")
                    set(found_path "${search_dir}/${dll_name}")
                    break()
                endif()
            endforeach()

            if(NOT found_path)
                continue()
            endif()

            set(dst_path "${seed_dir}/${dll_name}")
            if(NOT EXISTS "${dst_path}")
                file(COPY "${found_path}" DESTINATION "${seed_dir}")
            endif()
            list(APPEND queue "${dst_path}")
        endforeach()
    endwhile()
endfunction()

set(seed_dir "${SOURCE_DIR}/.lvs_runtime_seed")
file(REMOVE_RECURSE "${seed_dir}")
file(MAKE_DIRECTORY "${seed_dir}")
file(COPY "${SOURCE_DIR}/${EXE_NAME}" DESTINATION "${seed_dir}")

set(search_dirs
    "${seed_dir}"
    "${SOURCE_DIR}"
    "${QT_BIN_DIR}"
    "${COMPILER_BIN_DIR}"
)

execute_process(
    COMMAND "${WINDEPLOYQT}"
        --dir "${seed_dir}"
        --no-opengl-sw
        --no-translations
        "${seed_dir}/${EXE_NAME}"
    RESULT_VARIABLE deploy_result
)
if(NOT deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed with exit code ${deploy_result}")
endif()

resolve_and_copy_runtime("${seed_dir}/${EXE_NAME}")

file(REMOVE_RECURSE "${RUNTIME_CACHE_DIR}")
file(MAKE_DIRECTORY "${RUNTIME_CACHE_DIR}")

file(GLOB seed_dlls LIST_DIRECTORIES false "${seed_dir}/*.dll")
if(seed_dlls)
    file(COPY ${seed_dlls} DESTINATION "${RUNTIME_CACHE_DIR}")
endif()

if(EXISTS "${seed_dir}/qt.conf")
    file(COPY "${seed_dir}/qt.conf" DESTINATION "${RUNTIME_CACHE_DIR}")
endif()

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

foreach(plugin_dir IN LISTS plugin_dirs)
    if(EXISTS "${seed_dir}/${plugin_dir}")
        file(COPY "${seed_dir}/${plugin_dir}" DESTINATION "${RUNTIME_CACHE_DIR}")
    endif()
endforeach()

file(WRITE "${RUNTIME_CACHE_DIR}/.ready" "ok")
file(REMOVE_RECURSE "${seed_dir}")
