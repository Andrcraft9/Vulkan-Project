# Project settings.
cmake_minimum_required(VERSION 3.16)

project(
  render
  DESCRIPTION "Vulkan Render Interfaces"
  VERSION ${VULKAN_PROJECT_VERSION}
  LANGUAGES CXX)

add_library("${PROJECT_NAME}" STATIC)
target_compile_features("${PROJECT_NAME}" PUBLIC cxx_std_17)

# Dependencies:
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

target_include_directories(
  "${PROJECT_NAME}"
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(
  "${PROJECT_NAME}"
  PUBLIC
    glfw
    Vulkan::Vulkan
)

add_subdirectory(src)
