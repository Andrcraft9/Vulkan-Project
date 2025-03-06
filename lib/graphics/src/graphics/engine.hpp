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