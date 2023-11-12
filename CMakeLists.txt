cmake_minimum_required(VERSION 3.16)

set(
  VULKAN_PROJECT_VERSION
  0.0.1
  CACHE INTERNAL
  "Version number of the Vulkan Project"
)

project(
  VulkanProject
  DESCRIPTION "Vulkan Project"
  VERSION ${VULKAN_PROJECT_VERSION}
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(lib)
add_subdirectory(examples)
