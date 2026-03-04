add_library(lvs_core STATIC
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

target_include_directories(lvs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(lvs_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Vulkan::Vulkan
)

qt_add_resources(lvs_core lvs_app_info_resource
    PREFIX "/config"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/config"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/AppInfo.json"
)

if(WIN32)
    target_compile_definitions(lvs_core PUBLIC VK_USE_PLATFORM_WIN32_KHR WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

function(lvs_apply_runtime_linking target)
    if(NOT WIN32 OR NOT LVS_STATIC_RUNTIME)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # clang64 is typically libc++; static-link what is consistently available.
        target_link_options(${target} PRIVATE -static-libgcc)
    endif()
endfunction()

function(lvs_apply_release_windows_subsystem target)
    if(NOT WIN32)
        return()
    endif()
    set_target_properties(${target} PROPERTIES
        WIN32_EXECUTABLE "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>"
    )
endfunction()

add_executable(lvs_app src/main_App.cpp)
target_link_libraries(lvs_app PRIVATE lvs_core)
lvs_apply_runtime_linking(lvs_app)
lvs_apply_release_windows_subsystem(lvs_app)
lvs_attach_shader_compilation(lvs_app)
lvs_apply_windows_resources(lvs_app)
lvs_attach_content_packaging(lvs_app)
lvs_add_distribution_target(lvs_app)

add_executable(lvs_studio src/main_Studio.cpp)
target_link_libraries(lvs_studio PRIVATE lvs_core)
lvs_apply_runtime_linking(lvs_studio)
lvs_apply_release_windows_subsystem(lvs_studio)
lvs_attach_shader_compilation(lvs_studio)
lvs_apply_windows_resources(lvs_studio)
lvs_attach_content_packaging(lvs_studio)
lvs_add_distribution_target(lvs_studio)

add_custom_target(lvs_dist
    DEPENDS lvs_app_dist lvs_studio_dist
    COMMENT "Assembling distributable bundles for all executables"
)
