#include <graphics/engine.hpp>

#include <chrono>

void update(graphics::Engine &engine, const graphics::NodeId node,
            const graphics::CameraId camera, const float time) {
  const auto swapChainExtent = engine.Extent();

  engine.UpdateNodeTransform(node, glm::rotate(glm::mat4(1.0f),
                                               time * glm::radians(90.0f),
                                               glm::vec3(0.0f, 0.0f, 1.0f)));
  engine.UpdateCameraTransform(camera,
                               glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f),
                                           glm::vec3(0.0f, 0.0f, 0.0f),
                                           glm::vec3(0.0f, 0.0f, 1.0f)));
  glm::mat4 projection =
      glm::perspective(glm::radians(45.0f),
                       static_cast<float>(swapChainExtent.width) /
                           static_cast<float>(swapChainExtent.height),
                       0.1f, 10.0f);
  projection[1][1] *= -1;
  engine.UpdateCameraProjection(camera, projection);
}

int main(int, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();

  try {
    // Create shaders, mesh and texture to render.
    std::string vertexShaderPath{"../../../shaders/vert.spv"};
    std::string fragmentShaderPath{"../../../shaders/frag.spv"};
    render::ImageData image{std::string{"../../../data/photo.jpg"}};

    graphics::Engine engine{};
    engine.Initialize();
    const auto vertexShader =
        engine.AddVertexShader(graphics::VertexShader{vertexShaderPath});
    const auto fragmentShader =
        engine.AddFragmentShader(graphics::FragmentShader{fragmentShaderPath});
    const auto program =
        engine.AddProgram(graphics::Program{vertexShader, fragmentShader});
    std::vector<graphics::Vertex> vertices{{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
                                           {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
                                           {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
                                           {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}}};
    std::vector<std::uint16_t> indices{0, 1, 2, 2, 3, 0};
    const auto mesh =
        engine.AddMesh(graphics::Mesh{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                      std::move(vertices), std::move(indices)});
    const auto texture = engine.AddTexture(graphics::Texture{&image});
    const auto material = engine.AddMaterial(graphics::Material{texture});
    const auto surface =
        engine.AddSurface(graphics::Surface{program, mesh, material});
    const auto node = engine.AddNode(graphics::Node{glm::mat4{1.0f}, surface});
    const auto camera =
        engine.AddCamera(graphics::Camera{glm::mat4{1.0f}, glm::mat4{1.0f}});
    engine.AddScene(
        graphics::Scene{{node}, camera, VkClearValue{127, 127, 127, 127}});

    /// Main engine loop.
    const auto window = engine.Window();
    const auto startTime = std::chrono::high_resolution_clock::now();
    std::size_t frameCount{};
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      const auto currentTime = std::chrono::high_resolution_clock::now();
      const float time =
          std::chrono::duration<float, std::chrono::seconds::period>(
              currentTime - startTime)
              .count();

      update(engine, node, camera, time);
      engine.Render();
      ++frameCount;
    }
    const auto finalTime = std::chrono::high_resolution_clock::now();
    LOG(INFO) << "FPS: "
              << static_cast<float>(frameCount) /
                     std::chrono::duration<float, std::chrono::seconds::period>(
                         finalTime - startTime)
                         .count();

    engine.Deinitialize();
  } catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}