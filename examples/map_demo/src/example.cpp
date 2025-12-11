#include <graphics/engine.hpp>
#include <graphics/map.hpp>

void update(graphics::Engine &engine, const graphics::CameraId camera) {
  const auto swapChainExtent = engine.Extent();

  glm::mat4 view =
      glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f));
  glm::mat4 projection = glm::perspective(
      glm::radians(60.0f),
      swapChainExtent.width / static_cast<float>(swapChainExtent.height), 0.0f,
      10.0f);
  projection[1][1] *= -1;
  engine.UpdateCameraTransform(camera, view);
  engine.UpdateCameraProjection(camera, projection);
}

int main(int, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();

  try {
    // Create shaders, mesh and texture to render.
    std::string vertexShaderPath{"../../../shaders/vert.spv"};
    std::string fragmentShaderPath{"../../../shaders/frag.spv"};
    graphics::TileParser tp{};
    render::ImageData tile{tp.Parse(0, 0, 0)};

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
    const auto texture = engine.AddTexture(graphics::Texture{&tile});
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
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      update(engine, camera);
      engine.Render();
    }

    engine.Deinitialize();
  } catch (const std::exception &e) {
    LOG(ERROR) << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}