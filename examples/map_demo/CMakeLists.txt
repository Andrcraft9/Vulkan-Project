add_executable(map_demo)
target_compile_features(map_demo PUBLIC cxx_std_17)

target_include_directories(
  map_demo
  PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
target_link_libraries(
  map_demo
  PUBLIC
    graphics
)

add_subdirectory(src)
