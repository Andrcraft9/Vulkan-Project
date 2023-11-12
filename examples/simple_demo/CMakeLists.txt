add_executable(simple_demo)
target_compile_features(simple_demo PUBLIC cxx_std_17)

target_include_directories(
  simple_demo
  PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
target_link_libraries(
  simple_demo
  PUBLIC
    vulkan_engine
)

add_subdirectory(src)
