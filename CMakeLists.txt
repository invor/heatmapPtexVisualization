cmake_minimum_required(VERSION 3.16)

project(heatmapPtexVisualization)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD TRUE)

option(ExampleProjectUWP "Build DirectX UWP example project" OFF)
option(ExampleProject "Build OpenGL GLFW example project" OFF)

set(USE_DX11 OFF CACHE BOOL "Use DirectX11 graphics backend" FORCE)
set(UWP OFF CACHE BOOL "Compile for UWP" FORCE)
set(USE_GL ON CACHE BOOL "Use OpenGL graphics backend" FORCE)

add_subdirectory(space-lion)

add_subdirectory(HeatmapVisualization)

