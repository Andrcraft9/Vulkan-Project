#include <graphics/engine.hpp>

#include <chrono>

void update(graphics::Engine &engine, const float time) {
  const auto swapChainExtent = engine.Extent();
  render::UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                          glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f),
                              swapChainExtent.width /
                                  static_cast<float>(swapChainExtent.height),
                              0.1f, 10.0f);
  ubo.proj[1][1] *= -1;

  engine.Render(graphics::EngineRenderOptions{
    std::move(ubo),
    VkClearValue{127, 127, 127, 127}
  });
}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();

  // Create shaders and mesh to render.
  std::string vertexShaderPath{"../../../shaders/vert.spv"};
  std::string fragmentShaderPath{"../../../shaders/frag.spv"};
  graphics::Mesh rectangle{
      VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      VkPolygonMode::VK_POLYGON_MODE_FILL,
      std::vector<render::Vertex>{{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                  {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                  {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                  {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}},
      std::vector<std::uint16_t>{0, 1, 2, 2, 3, 0}};

  const graphics::EngineInitializationOptions options{
      std::move(vertexShaderPath), std::move(fragmentShaderPath),
      std::move(rectangle)};

  graphics::Engine engine{};
  try {
    engine.Initialize(options);
  } catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return EXIT_FAILURE;
  }

  const auto window = engine.Window();
  /// Main engine loop.
  const auto startTime = std::chrono::high_resolution_clock::now();
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    const auto currentTime = std::chrono::high_resolution_clock::now();
    const float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime -
                                                                   startTime)
            .count();

    try {
      update(engine, time);
    } catch (const std::exception &e) {
      LOG(ERROR) << e.what();
      return EXIT_FAILURE;
    }
  }

  try {
    engine.Deinitialize();
  } catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}