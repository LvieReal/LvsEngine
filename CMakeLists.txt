cmake_minimum_required(VERSION 3.27)

project(LvsEngineDevCpp VERSION 1.0.0 LANGUAGES C CXX)

if(WIN32)
    enable_language(RC)
endif()

include(cmake/ProjectOptions.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Sources.cmake)
include(cmake/Shaders.cmake)
include(cmake/WindowsResources.cmake)
include(cmake/Packaging.cmake)
include(cmake/Targets.cmake)
