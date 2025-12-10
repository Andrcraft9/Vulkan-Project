#pragma once

#include <graphics/utils.hpp>
#include <render/context.hpp>

#include <array>
#include <map>

namespace graphics {

/// Component System

using Vertex = render::Vertex;

using VertexShaderId = std::uint32_t;
struct VertexShader final {
  std::string shaderPath{};
};

// TODO: Template can be reused.
// using MeshTemplateId = std::uint32_t;
// struct MeshTemplate final {
//  VertexShaderId shader{};
//  VkPrimitiveTopology topology{};
//  VkPolygonMode polygonMode{};
//};

using MeshId = std::uint32_t;
struct Mesh final {
  VertexShaderId shader{};
  std::vector<Vertex> vertices{};
  std::vector<std::uint16_t> indices{};
};

using TextureId = std::uint32_t;
struct Texture final {
  const render::ImageData *image{};
};

using FragmentShaderId = std::uint32_t;
struct FragmentShader final {
  std::string shaderPath{};
};

using MaterialId = std::uint32_t;
struct Material final {
  FragmentShaderId shader{};
  TextureId texture{};
};

using SurfaceId = std::uint32_t;
struct Surface final {
  MeshId mesh{};
  MaterialId material{};
};

using NodeId = std::uint32_t;
struct Node final {
  glm::mat4 transform{};
  SurfaceId surface{};
};

using CameraId = std::uint32_t;
struct Camera final {
  glm::mat4 transform{};
  glm::mat4 projection{};
};

using SceneId = std::uint32_t;
struct Scene final {
  std::vector<NodeId> nodes{};
  CameraId camera{};
  VkClearValue clearColor{};
};

struct Components final {
  std::map<VertexShaderId, VertexShader> vertexShaders{};
  std::map<MeshId, Mesh> meshes{};
  std::map<FragmentShaderId, FragmentShader> fragmentShaders{};
  std::map<TextureId, Texture> textures{};
  std::map<MaterialId, Material> materials{};
  std::map<SurfaceId, Surface> surfaces{};
  std::map<NodeId, Node> nodes{};
  std::map<CameraId, Camera> cameras{};
  std::map<SceneId, Scene> scenes{};
};

struct MeshRes final {
  VkBuffer vertexBuffer{};
  VkBuffer indexBuffer{};
  std::array<VkBuffer, render::kMaxFramesInFlight> uniformBuffers{};
};

struct TextureRes final {
  VkImage image{};
  VkImageView imageView{};
  VkSampler sampler{};
};

struct SurfaceRes final {
  VkDescriptorSetLayout descriptorSetLayout{};
  VkPipelineLayout pipelineLayout{};
  VkPipeline pipeline{};
  VkDescriptorPool descriptorPool{};
  std::array<VkDescriptorSet, render::kMaxFramesInFlight> descriptorSets{};
};

struct Resources final {
  std::map<VertexShaderId, VkShaderModule> vertexShaders{};
  std::map<MeshId, MeshRes> meshes{};
  std::map<FragmentShaderId, VkShaderModule> fragmentShaders{};
  std::map<TextureId, TextureRes> textures{};
  std::map<SurfaceId, SurfaceRes> surfaces{};
};

/// Engine.
class Engine final {
public:
  /// Initializing
  /// @{

  void Initialize();

  void Deinitialize();

  /// @}

  /// Building
  /// @{

  VertexShaderId AddVertexShader(VertexShader vertexShader);

  MeshId AddMesh(Mesh mesh);

  FragmentShaderId AddFragmentShader(FragmentShader fragmentShader);

  TextureId AddTexture(Texture texture);

  MaterialId AddMaterial(Material material);

  SurfaceId AddSurface(Surface surface);

  NodeId AddNode(Node node);

  CameraId AddCamera(Camera camera);

  SceneId AddScene(Scene scene);

  /// @}

  /// Updating
  /// @{

  void UpdateNodeTransform(NodeId nodeId, glm::mat4 transform);

  void UpdateCameraTransform(CameraId cameraId, glm::mat4 transform);

  void UpdateCameraProjection(CameraId cameraId, glm::mat4 projection);

  /// @}

  /// Rendering
  /// @{

  void Render();

  GLFWwindow *Window();

  VkExtent2D Extent();

  /// @}

private:
  Components components_{};
  Resources resources_{};
  render::Context context_{};
  VkCommandPool commandPool_{};
  std::array<VkCommandBuffer, render::kMaxFramesInFlight> commandBuffers_{};
  std::uint32_t bufferId_{};
};

} // namespace graphics
