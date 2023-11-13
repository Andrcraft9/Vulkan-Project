add_executable(app_demo)
target_compile_features(app_demo PUBLIC cxx_std_17)

target_include_directories(
  app_demo
  PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
target_link_libraries(
  app_demo
  PUBLIC
    graphics
)

add_subdirectory(src)
