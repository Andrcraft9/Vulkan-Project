#include <vulkan_engine/vulkan_engine.hpp>

#include <iostream>

int main() {
  vulkan_engine::VulkanApp app;

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}