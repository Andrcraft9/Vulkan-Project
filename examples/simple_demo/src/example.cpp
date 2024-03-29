#include <graphics/engine.hpp>

#include <iostream>

int main() {
  graphics::Engine engine{};

  try {
    engine.Run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}