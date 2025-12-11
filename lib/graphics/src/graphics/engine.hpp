#pragma once

#include <graphics/utils.hpp>
#include <render/context.hpp>

#include <array>
#include <map>

namespace graphics {

/// Component System
/// @{

using Vertex = render::Vertex;

using VertexShaderId = std::uint32_t;
struct VertexShader final {
  std::string shaderPath{};
};

using FragmentShaderId = std::uint32_t;
struct FragmentShader final {
  std::string shaderPath{};
};

using ProgramId = std::uint32_t;
struct Program final {
  VertexShaderId vertexShader{};
  FragmentShaderId fragmentShader{};
};

using MeshId = std::uint32_t;
struct Mesh final {
  VkPrimitiveTopology topology{};
  std::vector<Vertex> vertices{};
  std::vector<std::uint16_t> indices{};
};

using TextureId = std::uint32_t;
struct Texture final {
  const render::ImageData *image{};
};

using MaterialId = std::uint32_t;
struct Material final {
  TextureId texture{};
};

using SurfaceId = std::uint32_t;
struct Surface final {
  ProgramId program{};
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
  std::map<FragmentShaderId, FragmentShader> fragmentShaders{};
  std::map<ProgramId, Program> programs{};
  std::map<MeshId, Mesh> meshes{};
  std::map<TextureId, Texture> textures{};
  std::map<MaterialId, Material> materials{};
  std::map<SurfaceId, Surface> surfaces{};
  std::map<NodeId, Node> nodes{};
  std::map<CameraId, Camera> cameras{};
  std::map<SceneId, Scene> scenes{};
};

/// @}

/// Component Resources
/// @{

struct ProgramRes final {
  VkDescriptorSetLayout descriptorSetLayout{};
  VkPipelineLayout pipelineLayout{};
  VkPipeline pipeline{};
  VkDescriptorPool descriptorPool{};
  std::array<VkDescriptorSet, render::kMaxFramesInFlight> descriptorSets{};
  std::array<VkBuffer, render::kMaxFramesInFlight> vertexUniformBuffers{};
};

struct MeshRes final {
  VkBuffer vertexBuffer{};
  VkBuffer indexBuffer{};
};

struct TextureRes final {
  VkImage image{};
  VkImageView imageView{};
  VkSampler sampler{};
};

struct Resources final {
  std::map<VertexShaderId, VkShaderModule> vertexShaders{};
  std::map<FragmentShaderId, VkShaderModule> fragmentShaders{};
  std::map<ProgramId, ProgramRes> programs{};
  std::map<MeshId, MeshRes> meshes{};
  std::map<TextureId, TextureRes> textures{};
};

/// @}

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

  VertexShaderId AddVertexShader(VertexShader &&vertexShader);

  FragmentShaderId AddFragmentShader(FragmentShader &&fragmentShader);

  ProgramId AddProgram(Program &&program);

  MeshId AddMesh(Mesh &&mesh);

  TextureId AddTexture(Texture &&texture);

  MaterialId AddMaterial(Material &&material);

  SurfaceId AddSurface(Surface &&surface);

  NodeId AddNode(Node &&node);

  CameraId AddCamera(Camera &&camera);

  SceneId AddScene(Scene &&scene);

  /// @}

  /// Updating
  /// @{

  void UpdateNodeTransform(NodeId nodeId, const glm::mat4 &transform);

  void UpdateCameraTransform(CameraId cameraId, const glm::mat4 &transform);

  void UpdateCameraProjection(CameraId cameraId, const glm::mat4 &projection);

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
