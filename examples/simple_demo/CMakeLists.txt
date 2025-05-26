add_executable(simple_demo)
target_compile_features(simple_demo PUBLIC cxx_std_17)

target_include_directories(
  simple_demo
  PRIVATE
    src
)
target_link_libraries(
  simple_demo
  PUBLIC
    graphics
)

add_subdirectory(src)
