cmake_minimum_required(VERSION 3.15)

# Project configuration
project(OcctImgui LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

# Include directories
find_package(glfw3 REQUIRED)
find_package(OpenCASCADE REQUIRED)
find_package(spdlog REQUIRED)
find_package(imgui REQUIRED)

file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/*.cpp"
    "${CMAKE_SOURCE_DIR}/*.h"
)

# Exclude CMake-generated files from the SOURCES list
list(FILTER SOURCES EXCLUDE REGEX ".*CMakeFiles.*")

# Add executable target
add_executable(OcctImgui ${SOURCES})

# Link libraries
target_link_libraries(OcctImgui
  PRIVATE   
  ${OpenCASCADE_LIBRARIES}
  glfw
  spdlog::spdlog
  imgui::imgui
)

target_compile_options(OcctImgui PRIVATE /utf-8)

target_link_directories(OcctImgui PRIVATE
    $<$<CONFIG:Debug>:${DEBUG_LIBS}>
    $<$<CONFIG:Release>:${RELEASE_LIBS}>
)