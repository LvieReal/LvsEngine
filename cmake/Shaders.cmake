find_program(LVS_GLSLANG_VALIDATOR NAMES glslangValidator)
find_program(LVS_GLSLC NAMES glslc)
find_program(LVS_SPIRV_CROSS NAMES spirv-cross spirv-cross.exe)

if(NOT LVS_GLSLANG_VALIDATOR AND NOT LVS_GLSLC)
    message(FATAL_ERROR "No shader compiler found. Install Vulkan SDK (glslangValidator or glslc).")
endif()

set(LVS_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/content/Shaders")
set(LVS_COMPILED_SHADER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/content/CompiledShaders")
set(LVS_VULKAN_COMPILED_DIR "${LVS_COMPILED_SHADER_ROOT}/Vulkan")
file(MAKE_DIRECTORY "${LVS_VULKAN_COMPILED_DIR}")
file(GLOB_RECURSE LVS_VULKAN_GLSL_SOURCES CONFIGURE_DEPENDS
    "${LVS_SHADER_SOURCE_DIR}/*.vert"
    "${LVS_SHADER_SOURCE_DIR}/*.frag"
    "${LVS_SHADER_SOURCE_DIR}/*.geom"
)

add_executable(lvs_glsl_preprocess "${CMAKE_CURRENT_SOURCE_DIR}/src/Tools/LvsGlslPreprocess.cpp")
set_target_properties(lvs_glsl_preprocess PROPERTIES OUTPUT_NAME "lvs_glsl_preprocess")

set(LVS_VULKAN_SPV_OUTPUTS "")
foreach(shader_src IN LISTS LVS_VULKAN_GLSL_SOURCES)
    file(RELATIVE_PATH shader_rel "${LVS_SHADER_SOURCE_DIR}" "${shader_src}")
    set(shader_spv "${LVS_VULKAN_COMPILED_DIR}/${shader_rel}.spv")
    get_filename_component(shader_spv_dir "${shader_spv}" DIRECTORY)
    get_filename_component(shader_rel_dir "${shader_rel}" DIRECTORY)
    get_filename_component(shader_rel_name "${shader_rel}" NAME_WE)
    get_filename_component(shader_rel_ext "${shader_rel}" EXT)
    set(shader_pp "${LVS_VULKAN_COMPILED_DIR}/${shader_rel_dir}/${shader_rel_name}.pp${shader_rel_ext}")
    set(shader_depfile "${shader_spv}.d")
    if(LVS_GLSLANG_VALIDATOR)
        add_custom_command(
            OUTPUT "${shader_spv}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_spv_dir}"
            COMMAND "$<TARGET_FILE:lvs_glsl_preprocess>" --input "${shader_src}" --output "${shader_pp}" --root-dir "${LVS_SHADER_SOURCE_DIR}" --depfile "${shader_depfile}" --dep-target "${shader_spv}"
            COMMAND "${LVS_GLSLANG_VALIDATOR}" -V "${shader_pp}" -o "${shader_spv}"
            DEPFILE "${shader_depfile}"
            DEPENDS "${shader_src}" lvs_glsl_preprocess
            COMMENT "Preprocessing+compiling GLSL -> SPIR-V: ${shader_src}"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT "${shader_spv}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_spv_dir}"
            COMMAND "$<TARGET_FILE:lvs_glsl_preprocess>" --input "${shader_src}" --output "${shader_pp}" --root-dir "${LVS_SHADER_SOURCE_DIR}" --depfile "${shader_depfile}" --dep-target "${shader_spv}"
            COMMAND "${LVS_GLSLC}" "${shader_pp}" -o "${shader_spv}"
            DEPFILE "${shader_depfile}"
            DEPENDS "${shader_src}" lvs_glsl_preprocess
            COMMENT "Preprocessing+compiling GLSL -> SPIR-V: ${shader_src}"
            VERBATIM
        )
    endif()
    list(APPEND LVS_VULKAN_SPV_OUTPUTS "${shader_spv}")
endforeach()

add_custom_target(lvs_compile_shaders DEPENDS ${LVS_VULKAN_SPV_OUTPUTS})

# OpenGL pipeline consumes GLSL generated from SPIR-V binaries via spirv-cross.
set(LVS_OPENGL_COMPILED_DIR "${LVS_COMPILED_SHADER_ROOT}/OpenGL")
file(MAKE_DIRECTORY "${LVS_OPENGL_COMPILED_DIR}")

set(LVS_OPENGL_GLSL_OUTPUTS "")
if(NOT LVS_SPIRV_CROSS)
    message(FATAL_ERROR "spirv-cross is required to transpile OpenGL SPIR-V shaders to GLSL.")
endif()

foreach(vulkan_spv IN LISTS LVS_VULKAN_SPV_OUTPUTS)
    file(RELATIVE_PATH shader_rel_spv "${LVS_VULKAN_COMPILED_DIR}" "${vulkan_spv}")
    string(REGEX REPLACE "\\.spv$" ".glsl" shader_rel_glsl "${shader_rel_spv}")
    set(shader_glsl "${LVS_OPENGL_COMPILED_DIR}/${shader_rel_glsl}")
    get_filename_component(shader_glsl_dir "${shader_glsl}" DIRECTORY)
    add_custom_command(
        OUTPUT "${shader_glsl}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_glsl_dir}"
        COMMAND "${LVS_SPIRV_CROSS}" "${vulkan_spv}" --output "${shader_glsl}"
        DEPENDS "${vulkan_spv}"
        COMMENT "Transpiling SPIR-V -> GLSL: ${vulkan_spv}"
        VERBATIM
    )
    list(APPEND LVS_OPENGL_GLSL_OUTPUTS "${shader_glsl}")
endforeach()

add_custom_target(lvs_transpile_opengl_shaders DEPENDS ${LVS_OPENGL_GLSL_OUTPUTS})
add_dependencies(lvs_compile_shaders lvs_transpile_opengl_shaders)

function(lvs_attach_shader_compilation target)
    add_dependencies(${target} lvs_compile_shaders)
endfunction()
