function(lvs_attach_content_packaging target)
    set(content_src "${CMAKE_CURRENT_SOURCE_DIR}/src/Lvs/Engine/Content")
    if(NOT EXISTS "${content_src}")
        message(FATAL_ERROR "Engine content folder not found: ${content_src}")
    endif()

    get_property(content_pack_target GLOBAL PROPERTY LVS_CONTENT_PACKAGE_TARGET)
    if(NOT content_pack_target)
        set(content_pack_target lvs_package_content)
        add_custom_target(${content_pack_target}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/Lvs/Engine"
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_DIR=${content_src}
                -DDEST_DIR=${CMAKE_BINARY_DIR}/Lvs/Engine/Content
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyContentAssets.cmake"
            COMMENT "Packaging Engine/Content assets"
            VERBATIM
        )

        set_property(GLOBAL PROPERTY LVS_CONTENT_PACKAGE_TARGET "${content_pack_target}")
    endif()

    add_dependencies(${target} ${content_pack_target})

    if(WIN32)
        get_filename_component(qt_cmake_root "${Qt6_DIR}" ABSOLUTE)
        get_filename_component(qt_cmake_dir "${qt_cmake_root}" DIRECTORY)
        get_filename_component(qt_lib_dir "${qt_cmake_dir}" DIRECTORY)
        get_filename_component(qt_prefix_dir "${qt_lib_dir}" DIRECTORY)
        set(qt_bin_dir "${qt_prefix_dir}/bin")
        set(windeployqt "${qt_bin_dir}/windeployqt.exe")

        if(NOT EXISTS "${windeployqt}")
            message(FATAL_ERROR "windeployqt not found: ${windeployqt}")
        endif()

        # Make Windows executables self-contained to avoid PATH-order DLL issues.
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND "${windeployqt}"
                --dir "$<TARGET_FILE_DIR:${target}>"
                --no-opengl-sw
                --no-translations
                "$<TARGET_FILE:${target}>"
            COMMENT "Deploying Qt runtime for ${target}"
            VERBATIM
        )

        if(LVS_ENABLE_ASAN)
            get_filename_component(compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            set(asan_runtime "${compiler_bin_dir}/libclang_rt.asan_dynamic-x86_64.dll")
            if(EXISTS "${asan_runtime}")
                add_custom_command(
                    TARGET ${target}
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${asan_runtime}"
                        "$<TARGET_FILE_DIR:${target}>/libclang_rt.asan_dynamic-x86_64.dll"
                    COMMENT "Copying ASAN runtime for ${target}"
                    VERBATIM
                )
            endif()
        endif()
    endif()
endfunction()

function(lvs_add_distribution_target target)
    set(dist_target "${target}_dist")
    set(vendor_target "${target}_vendor_runtime")
    string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" lvs_compiler_id_lower)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(lvs_compiler_id_lower "mingw")
    endif()
    string(TOLOWER "${CMAKE_BUILD_TYPE}" lvs_build_type_lower)
    if(NOT lvs_build_type_lower)
        set(lvs_build_type_lower "unknown")
    endif()
    set(runtime_cache_dir "${CMAKE_SOURCE_DIR}/third_party/runtime/${lvs_compiler_id_lower}-${lvs_build_type_lower}/${target}")
    set(dist_dir "${CMAKE_SOURCE_DIR}/bundled/${lvs_compiler_id_lower}-${lvs_build_type_lower}/${target}")
    set(dist_args
        -DSOURCE_DIR=$<TARGET_FILE_DIR:${target}>
        -DDIST_DIR=${dist_dir}
        -DEXE_NAME=$<TARGET_FILE_NAME:${target}>
        -DRUNTIME_CACHE_DIR=${runtime_cache_dir}
    )

    if(WIN32)
        get_filename_component(qt_cmake_root "${Qt6_DIR}" ABSOLUTE)
        get_filename_component(qt_cmake_dir "${qt_cmake_root}" DIRECTORY)
        get_filename_component(qt_lib_dir "${qt_cmake_dir}" DIRECTORY)
        get_filename_component(qt_prefix_dir "${qt_lib_dir}" DIRECTORY)
        set(qt_bin_dir "${qt_prefix_dir}/bin")
        set(windeployqt "${qt_bin_dir}/windeployqt.exe")
        if(EXISTS "${windeployqt}")
            get_filename_component(compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            set(objdump "${compiler_bin_dir}/objdump.exe")
            if(NOT EXISTS "${objdump}")
                set(objdump "${compiler_bin_dir}/llvm-objdump.exe")
            endif()

            list(APPEND dist_args -DWINDEPLOYQT=${windeployqt})

            add_custom_target(${vendor_target}
                COMMAND ${CMAKE_COMMAND}
                    -DSOURCE_DIR=$<TARGET_FILE_DIR:${target}>
                    -DEXE_NAME=$<TARGET_FILE_NAME:${target}>
                    -DRUNTIME_CACHE_DIR=${runtime_cache_dir}
                    -DWINDEPLOYQT=${windeployqt}
                    -DQT_BIN_DIR=${qt_bin_dir}
                    -DCOMPILER_BIN_DIR=${compiler_bin_dir}
                    -DOBJDUMP_EXECUTABLE=${objdump}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SeedRuntimeCache.cmake"
                DEPENDS ${target}
                COMMENT "Caching runtime dependencies for ${target} into project"
                VERBATIM
            )
        endif()
    endif()

    if(LVS_ENABLE_ASAN)
        get_filename_component(compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(asan_runtime "${compiler_bin_dir}/libclang_rt.asan_dynamic-x86_64.dll")
        if(EXISTS "${asan_runtime}")
            list(APPEND dist_args -DASAN_RUNTIME_DLL=${asan_runtime})
        endif()
    endif()

    add_custom_target(${dist_target}
        COMMAND ${CMAKE_COMMAND}
            ${dist_args}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateDistributionBundle.cmake"
        DEPENDS ${target}
        COMMENT "Assembling distributable bundle for ${target}"
        VERBATIM
    )

    if(TARGET ${vendor_target})
        add_dependencies(${dist_target} ${vendor_target})
    endif()
endfunction()
