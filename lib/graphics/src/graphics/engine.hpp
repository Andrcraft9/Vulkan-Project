#pragma once

#include <render/context.hpp>

#include <filesystem>
#include <fstream>

namespace graphics {

namespace fs = std::filesystem;

/// Helper function to load the binary data from the files.
///
/// @param path  Path to file.
///
/// @return Loaded binary data.
std::vector<char> ReadFile(fs::path path);

class VertexShader {
public:
  virtual VkShaderModule ShaderModule() const = 0;
  virtual VkVertexInputBindingDescription VertexInputBinding() const = 0;
  virtual std::vector<VkVertexInputAttributeDescription> VertexInputAttributes() const  = 0;
  virtual VkBuffer VertexBuffer() const = 0;
  virtual VkBuffer IndexBuffer() const = 0;
};

class VertexShaderImpl : public VertexShader {
public:
  struct Options final {
    const std::string vertexShaderPath{};
    VkCommandPool commandPool{};
    // TODO: Make implementation specific, don't use render::Vertex.
    std::vector<render::Vertex> vertices{};
    std::vector<std::uint16_t> indices{};
  };

  VertexShaderImpl(render::Context& context, const Options& options) {
    LOG(INFO) << "VS: Loading a shader:" << options.vertexShaderPath;
    const auto vertexShaderCode = ReadFile(options.vertexShaderPath);
    shaderModule_ = context.CreateShaderModule(vertexShaderCode);

    // TODO: Make implementation specific, don't use render::Get*().
    LOG(INFO) << "VS: Input binding/attribute descriptions...";
    vertexInputBinding_ = render::GetBindingDescription();
    vertexInputAttributes_ = render::GetAttributeDescriptions();

    LOG(INFO) << "VS: Creating a vertex buffer...";
    render::VertexBufferOptions vertexBufferOptions{};
    vertexBufferOptions.commandPool = options.commandPool;
    vertexBufferOptions.vertices = options.vertices;
    vertexBuffer_ = context.CreateVertexBuffer(vertexBufferOptions);

    LOG(INFO) << "VS: Creating a index buffer...";
    render::IndexBufferOptions indexBufferOptions{};
    indexBufferOptions.commandPool = options.commandPool;
    indexBufferOptions.indices = options.indices;
    indexBuffer_ = context.CreateIndexBuffer(indexBufferOptions);
  }

  VkShaderModule ShaderModule() const final {
    return shaderModule_;
  }

  VkVertexInputBindingDescription VertexInputBinding() const final {
    return vertexInputBinding_;
  }

  std::vector<VkVertexInputAttributeDescription> VertexInputAttributes() const final {
    return vertexInputAttributes_;
  }

  VkBuffer VertexBuffer() const final {
    return vertexBuffer_;
  }

  VkBuffer IndexBuffer() const final {
    return indexBuffer_;
  }

private:
  VkShaderModule shaderModule_;
  VkVertexInputBindingDescription vertexInputBinding_;
  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes_;
  VkBuffer vertexBuffer_;
  VkBuffer indexBuffer_;
};

struct Mesh {
  VkPrimitiveTopology topology{};
  VkPolygonMode polygonMode{};
  std::vector<render::Vertex> vertices{};
  std::vector<std::uint16_t> indices{};
};

struct Texture {
  render::ImageData image{};
};

/// Engine initialization options.
struct EngineInitializationOptions {
  /// Path to SPIR-V vertex shader code.
  std::string vertexShaderPath;
  /// Path to SPIR-V fragment shader code.
  std::string fragmentShaderPath;
  /// Mesh.
  Mesh mesh{};
  /// Texture
  Texture texture{};
};

struct EngineRenderOptions {
  render::UniformBufferObject ubo{};
  VkClearValue clearColor{};
};

/// Vulkan Engine.
class Engine final {
public:
  /// Initialize the engine.
  ///
  /// @param option  Initialization params.
  void Initialize(const EngineInitializationOptions &options);

  void Render(const EngineRenderOptions &options);

  /// Deinitialize the engine and destroy resources.
  void Deinitialize();

  /// Returns pointer to the created GLFW window.
  GLFWwindow *Window();

  /// Returns the current swapchain extent.
  VkExtent2D Extent();

private:
  render::Context context_{};
  VkRenderPass renderPass_{};
  VkPipelineLayout pipelineLayout_{};
  VkPipeline pipeline_{};
  VkBuffer vertexBuffer_{};
  VkBuffer indexBuffer_{};
  VkImage textureImage_{};
  VkImageView textureImageView_{};
  VkSampler textureSampler_{};
  std::vector<VkDescriptorSet> descriptorSets_{};
  std::vector<VkCommandBuffer> commandBuffers_{};

  std::uint32_t indexCount_{};
  std::uint32_t bufferId_{};
};

} // namespace graphics