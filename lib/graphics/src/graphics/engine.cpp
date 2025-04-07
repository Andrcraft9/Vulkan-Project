#include "graphics/engine.hpp"

namespace graphics {

VertexShaderId Engine::AddVertexShader(VertexShader vertexShader) {
  LOG(INFO) << "Loading a vertex shader:" << vertexShader.shaderPath;

  const VertexShaderId id = components_.vertexShaders.size();
  components_.vertexShaders[id] = vertexShader;

  const auto vertexShaderCode = ReadFile(vertexShader.shaderPath);
  render::ShaderModuleOptions shaderOptions{};
  shaderOptions.data = vertexShaderCode.data();
  shaderOptions.size = vertexShaderCode.size();
  const auto vertexShaderModule = context_.CreateShaderModule(shaderOptions);

  resources_.vertexShaders[id] = vertexShaderModule;
  return id;
}

MeshId Engine::AddMesh(Mesh mesh) {
  LOG(INFO) << "Loading a mesh";

  const auto vertexShaderIt = components_.vertexShaders.find(mesh.shader);
  if (vertexShaderIt == components_.vertexShaders.end()) {
    throw std::runtime_error("failed to find the vertex shader!");
  }

  const MeshId id = components_.meshes.size();
  components_.meshes[id] = mesh;

  LOG(INFO) << "Creating a vertex buffer...";
  render::VertexBufferOptions vertexBufferOptions{};
  vertexBufferOptions.commandPool = commandPool_;
  vertexBufferOptions.bufferSize =
      sizeof(mesh.vertices[0]) * mesh.vertices.size();
  vertexBufferOptions.bufferData = mesh.vertices.data();
  const auto vertexBuffer = context_.CreateVertexBuffer(vertexBufferOptions);

  LOG(INFO) << "Creating a index buffer...";
  render::IndexBufferOptions indexBufferOptions{};
  indexBufferOptions.commandPool = commandPool_;
  indexBufferOptions.bufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();
  indexBufferOptions.bufferData = mesh.indices.data();
  const auto indexBuffer = context_.CreateIndexBuffer(indexBufferOptions);

  std::array<VkBuffer, render::kMaxFramesInFlight> uniformBuffers{};
  for (std::size_t i{0}; i < render::kMaxFramesInFlight; ++i) {
    LOG(INFO) << "Creating an uniform buffer...";
    const auto uniformBuffer = context_.CreateUniformBuffer();
    uniformBuffers[i] = uniformBuffer;
  }

  resources_.meshes[id] =
      MeshRes{vertexBuffer, indexBuffer, std::move(uniformBuffers)};
  return id;
}

FragmentShaderId Engine::AddFragmentShader(FragmentShader fragmentShader) {
  LOG(INFO) << "Loading a fragment shader:" << fragmentShader.shaderPath;

  const FragmentShaderId id = components_.fragmentShaders.size();
  components_.fragmentShaders[id] = fragmentShader;

  const auto fragmentShaderCode = ReadFile(fragmentShader.shaderPath);
  render::ShaderModuleOptions shaderOptions{};
  shaderOptions.data = fragmentShaderCode.data();
  shaderOptions.size = fragmentShaderCode.size();
  const auto fragmentShaderModule = context_.CreateShaderModule(shaderOptions);

  resources_.fragmentShaders[id] = fragmentShaderModule;
  return id;
}

TextureId Engine::AddTexture(Texture texture) {
  LOG(INFO) << "Loading a texture";

  const TextureId id = components_.textures.size();
  components_.textures[id] = texture;

  LOG(INFO) << "Creating a texture image...";
  render::TextureImageOptions textureImageOptions{};
  textureImageOptions.commandPool = commandPool_;
  textureImageOptions.imageData = texture.image;
  const auto image = context_.CreateTextureImage(textureImageOptions);

  LOG(INFO) << "Creating a texture image view...";
  render::ImageViewOptions textureImageViewOptions{};
  textureImageViewOptions.format = VK_FORMAT_R8G8B8A8_SRGB;
  textureImageViewOptions.image = image;
  const auto imageView = context_.CreateImageView(textureImageViewOptions);

  LOG(INFO) << "Creating a texture sampler...";
  render::TextureSamplerOptions textureSamplerOptions{};
  textureSamplerOptions.magFilter = VK_FILTER_LINEAR;
  textureSamplerOptions.minFilter = VK_FILTER_LINEAR;
  textureSamplerOptions.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  textureSamplerOptions.anisotropyEnable = false;
  textureSamplerOptions.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  const auto sampler = context_.CreateTextureSampler(textureSamplerOptions);

  resources_.textures[id] = TextureRes{image, imageView, sampler};
  return id;
}

MaterialId Engine::AddMaterial(Material material) {
  LOG(INFO) << "Loading a material";

  const auto fragmentShaderIt =
      components_.fragmentShaders.find(material.shader);
  if (fragmentShaderIt == components_.fragmentShaders.end()) {
    throw std::runtime_error("failed to find the fragment shader!");
  }

  const auto textureIt = components_.textures.find(material.texture);
  if (textureIt == components_.textures.end()) {
    throw std::runtime_error("failed to find the texture!");
  }

  const MaterialId id = components_.materials.size();
  components_.materials[id] = material;

  return id;
}

SurfaceId Engine::AddSurface(Surface surface) {
  LOG(INFO) << "Loading a surface";

  const auto meshIt = components_.meshes.find(surface.mesh);
  if (meshIt == components_.meshes.end()) {
    throw std::runtime_error("failed to find the mesh!");
  }

  const auto materialIt = components_.materials.find(surface.material);
  if (materialIt == components_.materials.end()) {
    throw std::runtime_error("failed to find the material!");
  }

  const SurfaceId id = components_.surfaces.size();
  components_.surfaces[id] = surface;

  LOG(INFO) << "Creating a descriptor set layout...";
  std::vector<render::DescriptorSetLayoutBindingOptions> bindingOptions{};
  bindingOptions.emplace_back(render::DescriptorSetLayoutBindingOptions{
      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT});
  bindingOptions.emplace_back(render::DescriptorSetLayoutBindingOptions{
      1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT});
  render::DescriptorSetLayoutOptions descriptorSetLayoutOptions{
      std::move(bindingOptions)};
  const auto descriptorSetLayout =
      context_.CreateDescriptorSetLayout(descriptorSetLayoutOptions);

  LOG(INFO) << "Creating a pipeline layout...";
  render::PipelineLayoutOptions pipelineLayoutOptions{};
  pipelineLayoutOptions.descriptorSetLayout = descriptorSetLayout;
  const auto pipelineLayout =
      context_.CreatePipelineLayout(pipelineLayoutOptions);

  LOG(INFO) << "Creating a graphics pipeline...";
  const auto &mesh = components_.meshes[surface.mesh];
  const auto &vertexShaderModule = resources_.vertexShaders[mesh.shader];
  const auto &material = components_.materials[surface.material];
  const auto &fragmentShaderModule =
      resources_.fragmentShaders[material.shader];
  render::GraphicsPipelineOptions pipelineOptions{};
  pipelineOptions.pipelineLayout = pipelineLayout;
  pipelineOptions.renderPass = renderPass_;
  pipelineOptions.vertexShader = vertexShaderModule;
  pipelineOptions.vertexInputBinding = render::GetBindingDescription();
  pipelineOptions.vertexInputAttribues = render::GetAttributeDescriptions();
  pipelineOptions.fragmentShader = fragmentShaderModule;
  pipelineOptions.viewportExtent = context_.GetSwapchainExtent();
  pipelineOptions.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipelineOptions.polygonMode = VK_POLYGON_MODE_FILL;
  const auto pipeline = context_.CreateGraphicsPipeline(pipelineOptions);

  LOG(INFO) << "Creating a descriptor pool...";
  std::vector<render::DescriptorPoolSizeOptions> poolSizes{};
  poolSizes.emplace_back(render::DescriptorPoolSizeOptions{
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, render::kMaxFramesInFlight});
  poolSizes.emplace_back(render::DescriptorPoolSizeOptions{
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, render::kMaxFramesInFlight});
  render::DescriptorPoolOptions descriptorPoolOptions{
      std::move(poolSizes), render::kMaxFramesInFlight};
  const auto descriptorPool =
      context_.CreateDescriptorPool(descriptorPoolOptions);

  std::array<VkDescriptorSet, render::kMaxFramesInFlight> descriptorSets{};
  for (std::size_t i{0}; i < render::kMaxFramesInFlight; ++i) {
    LOG(INFO) << "Creating a descriptor set...";
    render::DescriptorSetOptions descriptorSetOptions{};
    descriptorSetOptions.descriptorPool = descriptorPool;
    descriptorSetOptions.descriptorSetLayout = descriptorSetLayout;
    const auto descriptorSet =
        context_.CreateDescriptorSet(descriptorSetOptions);
    descriptorSets[i] = (descriptorSet);
  }

  const auto &meshRes = resources_.meshes[surface.mesh];
  const auto &textureRes = resources_.textures[material.texture];
  for (std::size_t i{0}; i < render::kMaxFramesInFlight; ++i) {
    LOG(INFO) << "Updating a descriptor set...";

    // Uniform Buffer.
    std::vector<render::DescriptorUniformBufferInfo> uniformBufferInfos{};
    uniformBufferInfos.emplace_back(
        render::DescriptorUniformBufferInfo{meshRes.uniformBuffers[i], 0});

    // Texture.
    std::vector<render::DescriptorImageInfo> imageInfos{};
    imageInfos.emplace_back(render::DescriptorImageInfo{textureRes.imageView,
                                                        textureRes.sampler, 1});
    render::UpdateDescriptorSetOptions updateDescriptorSetOptions{};
    updateDescriptorSetOptions.descriptorSet = descriptorSets[i];
    updateDescriptorSetOptions.descriptorUniformBuffers =
        std::move(uniformBufferInfos);
    updateDescriptorSetOptions.descriptorImages = std::move(imageInfos);

    context_.UpdateDescriptorSet(updateDescriptorSetOptions);
  }

  resources_.surfaces[id] =
      SurfaceRes{descriptorSetLayout, pipelineLayout, pipeline, descriptorPool,
                 std::move(descriptorSets)};
  return id;
}

NodeId Engine::AddNode(Node node) {
  LOG(INFO) << "Loading a node";

  const auto surfaceIt = components_.surfaces.find(node.surface);
  if (surfaceIt == components_.surfaces.end()) {
    throw std::runtime_error("failed to find the surface!");
  }

  const NodeId id = components_.nodes.size();
  components_.nodes[id] = node;
  return id;
}

CameraId Engine::AddCamera(Camera camera) {
  LOG(INFO) << "Loading a camera";

  const CameraId id = components_.cameras.size();
  components_.cameras[id] = camera;
  return id;
}

SceneId Engine::AddScene(Scene scene) {
  LOG(INFO) << "Loading a scene";

  const auto cameraIt = components_.cameras.find(scene.camera);
  if (cameraIt == components_.cameras.end()) {
    throw std::runtime_error("failed to find the camera!");
  }

  for (const auto nodeId : scene.nodes) {
    const auto nodeIt = components_.nodes.find(nodeId);
    if (nodeIt == components_.nodes.end()) {
      throw std::runtime_error("failed to find the node!");
    }
  }

  const SceneId id = components_.scenes.size();
  components_.scenes[id] = scene;
  return id;
}

void Engine::UpdateNodeTransform(NodeId nodeId, glm::mat4 transform) {
  auto nodeIt = components_.nodes.find(nodeId);
  if (nodeIt == components_.nodes.end()) {
    throw std::runtime_error("failed to find the node!");
  }

  nodeIt->second.transform = transform;
}

void Engine::UpdateCameraTransform(CameraId cameraId, glm::mat4 transform) {
  auto cameraIt = components_.cameras.find(cameraId);
  if (cameraIt == components_.cameras.end()) {
    throw std::runtime_error("failed to find the camera!");
  }

  cameraIt->second.transform = transform;
}

void Engine::UpdateCameraProjection(CameraId cameraId, glm::mat4 projection) {
  auto cameraIt = components_.cameras.find(cameraId);
  if (cameraIt == components_.cameras.end()) {
    throw std::runtime_error("failed to find the camera!");
  }

  cameraIt->second.projection = projection;
}

void Engine::Initialize() {
  render::ContextOptions contextOptions{};
  contextOptions.enableValidationLayers = true;
  contextOptions.title = "Graphics Engine";
  LOG(INFO) << "Initializing the engine...";
  context_.Initialize(contextOptions);

  LOG(INFO) << "Creating a render pass...";
  render::RenderPassOptions renderPassOptions{};
  renderPassOptions.format = context_.GetSwapchainImageFormat();
  renderPass_ = context_.CreateRenderPass(renderPassOptions);

  LOG(INFO) << "Creating framebuffers...";
  context_.CreateSwapchainFramebuffers(renderPass_);

  LOG(INFO) << "Creating a command pool...";
  render::CommandPoolOptions commandPoolOptions{};
  commandPool_ = context_.CreateCommandPool(commandPoolOptions);

  LOG(INFO) << "Creating command buffers...";
  for (std::size_t i{0}; i < render::kMaxFramesInFlight; ++i) {
    render::CommandBufferOptions commandBufferOptions{};
    commandBufferOptions.commandPool = commandPool_;
    const auto commandBuffer =
        context_.CreateCommandBuffer(commandBufferOptions);
    commandBuffers_[i] = commandBuffer;
  }

  bufferId_ = 0;
}

void Engine::Render() {
  render::BeginFrameOptions beginFrameOptions{};
  beginFrameOptions.renderPass = renderPass_;
  context_.BeginFrame(beginFrameOptions);

  for (const auto &[sceneId, scene] : components_.scenes) {
    for (const auto nodeId : scene.nodes) {
      const auto &node = components_.nodes[nodeId];
      const auto &surface = components_.surfaces[node.surface];
      const auto &surfaceRes = resources_.surfaces[node.surface];
      const auto &mesh = components_.meshes[surface.mesh];
      const auto &meshRes = resources_.meshes[surface.mesh];
      const auto &camera = components_.cameras[scene.camera];

      render::RecordCommandBufferOptions recordOptions{};
      recordOptions.commandBuffer = commandBuffers_[bufferId_];
      recordOptions.renderPass = renderPass_;
      recordOptions.vertexBuffer = meshRes.vertexBuffer;
      recordOptions.indexBuffer = meshRes.indexBuffer;
      recordOptions.indexCount = mesh.indices.size();
      recordOptions.descriptorSet = surfaceRes.descriptorSets[bufferId_];
      recordOptions.pipelineLayout = surfaceRes.pipelineLayout;
      recordOptions.pipeline = surfaceRes.pipeline;
      recordOptions.clearColor = scene.clearColor;
      context_.RecordCommandBuffer(recordOptions);

      render::UniformBufferObject ubo{};
      ubo.proj = camera.projection;
      ubo.view = camera.transform;
      ubo.model = node.transform;
      context_.UpdateUniformBuffer(
          render::UpdateUniformBufferOptions{bufferId_, ubo});
    }
  }

  render::EndFrameOptions endFrameOptions{};
  endFrameOptions.renderPass = renderPass_;
  endFrameOptions.commandBuffer = commandBuffers_[bufferId_];
  context_.EndFrame(endFrameOptions);

  bufferId_ = (bufferId_ + 1) % render::kMaxFramesInFlight;
}

void Engine::Deinitialize() {
  context_.WaitIdle();
  context_.Cleanup();
}

GLFWwindow *Engine::Window() { return context_.GetWindow(); }

VkExtent2D Engine::Extent() { return context_.GetSwapchainExtent(); }

} // namespace graphics
