# Project settings.
cmake_minimum_required(VERSION 3.16)

project(
  render
  DESCRIPTION "Vulkan Render Interfaces"
  VERSION ${VULKAN_PROJECT_VERSION}
  LANGUAGES CXX)

add_library("${PROJECT_NAME}" STATIC)
target_compile_features("${PROJECT_NAME}" PUBLIC cxx_std_17)

# System dependencies:
find_package(Vulkan REQUIRED)

# vcpkg dependencies:
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(Stb REQUIRED)

target_include_directories(
  "${PROJECT_NAME}"
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${Stb_INCLUDE_DIRS}"
)

target_link_libraries(
  "${PROJECT_NAME}"
  PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    vk-bootstrap::vk-bootstrap
    vk-bootstrap::vk-bootstrap-compiler-warnings
    glm::glm
    glfw
    glog::glog
)

add_subdirectory(src)
