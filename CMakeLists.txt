cmake_minimum_required(VERSION 3.16)

set(
  VULKAN_PROJECT_VERSION
  0.0.1
  CACHE INTERNAL
  "Version number of the Vulkan Project")

project(
  VulkanProject
  DESCRIPTION "Vulkan Project"
  VERSION ${VULKAN_PROJECT_VERSION}
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

# Documentation
find_package(Doxygen)
set(DOXYGEN_STRIP_FROM_PATH "${CMAKE_SOURCE_DIR}")
set(DOXYGEN_INPUT "${CMAKE_SOURCE_DIR}/lib")
set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/doc")
message(STATUS "Generating Doxygen configuration")
configure_file(Doxyfile.in Doxyfile)
add_custom_command(
  OUTPUT "${DOXYGEN_OUTPUT_DIRECTORY}"
  COMMENT "Generating Doxygen documentation"
  VERBATIM
  COMMAND doxygen)
add_custom_target(
  doc_doxygen
  DEPENDS "${DOXYGEN_OUTPUT_DIRECTORY}")

add_subdirectory(lib)
add_subdirectory(examples)
