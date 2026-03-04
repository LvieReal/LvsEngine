find_program(LVS_GLSLANG_VALIDATOR NAMES glslangValidator)
find_program(LVS_GLSLC NAMES glslc)

if(NOT LVS_GLSLANG_VALIDATOR AND NOT LVS_GLSLC)
    message(FATAL_ERROR "No shader compiler found. Install Vulkan SDK (glslangValidator or glslc).")
endif()

set(LVS_VULKAN_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/Lvs/Engine/Content/Shaders/Vulkan")
file(GLOB_RECURSE LVS_VULKAN_GLSL_SOURCES CONFIGURE_DEPENDS
    "${LVS_VULKAN_SHADER_DIR}/*.vert"
    "${LVS_VULKAN_SHADER_DIR}/*.frag"
)

set(LVS_VULKAN_SPV_OUTPUTS "")
foreach(shader_src IN LISTS LVS_VULKAN_GLSL_SOURCES)
    set(shader_spv "${shader_src}.spv")
    if(LVS_GLSLANG_VALIDATOR)
        add_custom_command(
            OUTPUT "${shader_spv}"
            COMMAND "${LVS_GLSLANG_VALIDATOR}" -V "${shader_src}" -o "${shader_spv}"
            DEPENDS "${shader_src}"
            COMMENT "Compiling GLSL -> SPIR-V: ${shader_src}"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT "${shader_spv}"
            COMMAND "${LVS_GLSLC}" "${shader_src}" -o "${shader_spv}"
            DEPENDS "${shader_src}"
            COMMENT "Compiling GLSL -> SPIR-V: ${shader_src}"
            VERBATIM
        )
    endif()
    list(APPEND LVS_VULKAN_SPV_OUTPUTS "${shader_spv}")
endforeach()

add_custom_target(lvs_compile_shaders DEPENDS ${LVS_VULKAN_SPV_OUTPUTS})

function(lvs_attach_shader_compilation target)
    add_dependencies(${target} lvs_compile_shaders)
endfunction()
