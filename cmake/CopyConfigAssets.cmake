if(NOT DEFINED SOURCE_DIR OR NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "SOURCE_DIR and DEST_DIR are required")
endif()

if(NOT EXISTS "${SOURCE_DIR}")
    message(FATAL_ERROR "Config source directory not found: ${SOURCE_DIR}")
endif()

file(REMOVE_RECURSE "${DEST_DIR}")
file(MAKE_DIRECTORY "${DEST_DIR}")

file(GLOB_RECURSE config_files LIST_DIRECTORIES false "${SOURCE_DIR}/*")
foreach(src_file IN LISTS config_files)
    file(RELATIVE_PATH rel_path "${SOURCE_DIR}" "${src_file}")
    set(dst_file "${DEST_DIR}/${rel_path}")
    get_filename_component(dst_dir "${dst_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${dst_dir}")
    file(COPY "${src_file}" DESTINATION "${dst_dir}")
endforeach()

