# Project settings.
cmake_minimum_required(VERSION 3.16)

project(
  graphics
  DESCRIPTION "Graphics Engine Interfaces"
  VERSION ${VULKAN_PROJECT_VERSION}
  LANGUAGES CXX)

add_library("${PROJECT_NAME}" STATIC)
target_compile_features("${PROJECT_NAME}" PUBLIC cxx_std_17)

find_package(cpr CONFIG REQUIRED)

target_include_directories(
  "${PROJECT_NAME}"
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(
  "${PROJECT_NAME}"
  PUBLIC
    render
    cpr::cpr
)

add_subdirectory(src)
