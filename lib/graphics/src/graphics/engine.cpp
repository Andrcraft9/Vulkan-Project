#include "graphics/engine.hpp"

namespace graphics {

VertexShaderId Engine::AddVertexShader(VertexShader &&vertexShader) {
  LOG(INFO) << "Loading a vertex shader:" << vertexShader.shaderPath;

  const auto vertexShaderCode = ReadFile(vertexShader.shaderPath);
  render::ShaderModuleOptions shaderOptions{};
  shaderOptions.data = vertexShaderCode.data();
  shaderOptions.size = vertexShaderCode.size();
  const auto vertexShaderModule = context_.CreateShaderModule(shaderOptions);

  const VertexShaderId id = components_.vertexShaders.size();
  components_.vertexShaders[id] = std::move(vertexShader);
  resources_.vertexShaders[id] = vertexShaderModule;
  return id;
}

FragmentShaderId Engine::AddFragmentShader(FragmentShader &&fragmentShader) {
  LOG(INFO) << "Loading a fragment shader:" << fragmentShader.shaderPath;

  const auto fragmentShaderCode = ReadFile(fragmentShader.shaderPath);
  render::ShaderModuleOptions shaderOptions{};
  shaderOptions.data = fragmentShaderCode.data();
  shaderOptions.size = fragmentShaderCode.size();
  const auto fragmentShaderModule = context_.CreateShaderModule(shaderOptions);

  const FragmentShaderId id = components_.fragmentShaders.size();
  components_.fragmentShaders[id] = fragmentShader;
  resources_.fragmentShaders[id] = fragmentShaderModule;
  return id;
}

ProgramId Engine::AddProgram(Program &&program) {
  LOG(INFO) << "Loading a program";

  const auto vertexShaderIt =
      components_.vertexShaders.find(program.vertexShader);
  if (vertexShaderIt == components_.vertexShaders.end()) {
    throw std::runtime_error("failed to find the vertex shader!");
  }

  const auto fragmentShaderIt =
      components_.fragmentShaders.find(program.fragmentShader);
  if (fragmentShaderIt == components_.fragmentShaders.end()) {
    throw std::runtime_error("failed to find the fragment shader!");
  }

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
  const auto vertexShaderModule =
      resources_.vertexShaders[program.vertexShader];
  const auto fragmentShaderModule =
      resources_.fragmentShaders[program.fragmentShader];
  render::GraphicsPipelineOptions pipelineOptions{};
  pipelineOptions.pipelineLayout = pipelineLayout;
  pipelineOptions.vertexShader = vertexShaderModule;
  pipelineOptions.vertexInputBinding = render::GetBindingDescription();
  pipelineOptions.vertexInputAttribues = render::GetAttributeDescriptions();
  pipelineOptions.fragmentShader = fragmentShaderModule;
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

  std::array<VkBuffer, render::kMaxFramesInFlight> uniformBuffers{};
  for (std::size_t i{0}; i < render::kMaxFramesInFlight; ++i) {
    LOG(INFO) << "Creating an uniform buffer...";
    const auto uniformBuffer = context_.CreateUniformBuffer();
    uniformBuffers[i] = uniformBuffer;
  }

  const ProgramId id = components_.programs.size();
  components_.programs[id] = std::move(program);
  resources_.programs[id] = ProgramRes{descriptorSetLayout,
                                       pipelineLayout,
                                       pipeline,
                                       descriptorPool,
                                       std::move(descriptorSets),
                                       std::move(uniformBuffers)};
  return id;
}

MeshId Engine::AddMesh(Mesh &&mesh) {
  LOG(INFO) << "Loading a mesh";

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

  const MeshId id = components_.meshes.size();
  components_.meshes[id] = std::move(mesh);
  resources_.meshes[id] = MeshRes{vertexBuffer, indexBuffer};
  return id;
}

TextureId Engine::AddTexture(Texture &&texture) {
  LOG(INFO) << "Loading a texture";

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

  const TextureId id = components_.textures.size();
  components_.textures[id] = std::move(texture);
  resources_.textures[id] = TextureRes{image, imageView, sampler};
  return id;
}

MaterialId Engine::AddMaterial(Material &&material) {
  LOG(INFO) << "Loading a material";

  const auto textureIt = components_.textures.find(material.texture);
  if (textureIt == components_.textures.end()) {
    throw std::runtime_error("failed to find the texture!");
  }

  const MaterialId id = components_.materials.size();
  components_.materials[id] = std::move(material);
  return id;
}

SurfaceId Engine::AddSurface(Surface &&surface) {
  LOG(INFO) << "Loading a surface";

  const auto programIt = components_.programs.find(surface.program);
  if (programIt == components_.programs.end()) {
    throw std::runtime_error("failed to find the program!");
  }

  const auto meshIt = components_.meshes.find(surface.mesh);
  if (meshIt == components_.meshes.end()) {
    throw std::runtime_error("failed to find the mesh!");
  }

  const auto materialIt = components_.materials.find(surface.material);
  if (materialIt == components_.materials.end()) {
    throw std::runtime_error("failed to find the material!");
  }

  const auto &programRes = resources_.programs[surface.program];
  const auto &textureRes = resources_.textures[materialIt->second.texture];
  for (std::size_t i{0}; i < render::kMaxFramesInFlight; ++i) {
    LOG(INFO) << "Updating a descriptor set...";

    // Uniform Buffer.
    std::vector<render::DescriptorUniformBufferInfo> uniformBufferInfos{};
    uniformBufferInfos.emplace_back(render::DescriptorUniformBufferInfo{
        programRes.vertexUniformBuffers[i], 0});

    // Texture.
    std::vector<render::DescriptorImageInfo> imageInfos{};
    imageInfos.emplace_back(render::DescriptorImageInfo{textureRes.imageView,
                                                        textureRes.sampler, 1});
    render::UpdateDescriptorSetOptions updateDescriptorSetOptions{};
    updateDescriptorSetOptions.descriptorSet = programRes.descriptorSets[i];
    updateDescriptorSetOptions.descriptorUniformBuffers =
        std::move(uniformBufferInfos);
    updateDescriptorSetOptions.descriptorImages = std::move(imageInfos);

    context_.UpdateDescriptorSet(updateDescriptorSetOptions);
  }

  const SurfaceId id = components_.surfaces.size();
  components_.surfaces[id] = std::move(surface);
  return id;
}

NodeId Engine::AddNode(Node &&node) {
  LOG(INFO) << "Loading a node";

  const auto surfaceIt = components_.surfaces.find(node.surface);
  if (surfaceIt == components_.surfaces.end()) {
    throw std::runtime_error("failed to find the surface!");
  }

  const NodeId id = components_.nodes.size();
  components_.nodes[id] = std::move(node);
  return id;
}

CameraId Engine::AddCamera(Camera &&camera) {
  LOG(INFO) << "Loading a camera";

  const CameraId id = components_.cameras.size();
  components_.cameras[id] = std::move(camera);
  return id;
}

SceneId Engine::AddScene(Scene &&scene) {
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
  components_.scenes[id] = std::move(scene);
  return id;
}

void Engine::UpdateNodeTransform(const NodeId nodeId,
                                 const glm::mat4 &transform) {
  auto nodeIt = components_.nodes.find(nodeId);
  if (nodeIt == components_.nodes.end()) {
    throw std::runtime_error("failed to find the node!");
  }

  nodeIt->second.transform = transform;
}

void Engine::UpdateCameraTransform(const CameraId cameraId,
                                   const glm::mat4 &transform) {
  auto cameraIt = components_.cameras.find(cameraId);
  if (cameraIt == components_.cameras.end()) {
    throw std::runtime_error("failed to find the camera!");
  }

  cameraIt->second.transform = transform;
}

void Engine::UpdateCameraProjection(const CameraId cameraId,
                                    const glm::mat4 &projection) {
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
  const auto beginRes = context_.BeginFrame(beginFrameOptions);

  if (!beginRes.isImageAcquired) {
    // Swapchain was recreated, skip this frame.
    return;
  }

  for (const auto &[sceneId, scene] : components_.scenes) {
    for (const auto nodeId : scene.nodes) {
      const auto &node = components_.nodes[nodeId];
      const auto &surface = components_.surfaces[node.surface];
      const auto &programRes = resources_.programs[surface.program];
      const auto &mesh = components_.meshes[surface.mesh];
      const auto &meshRes = resources_.meshes[surface.mesh];
      const auto &camera = components_.cameras[scene.camera];

      render::RecordCommandBufferOptions recordOptions{};
      recordOptions.commandBuffer = commandBuffers_[bufferId_];
      recordOptions.vertexBuffer = meshRes.vertexBuffer;
      recordOptions.indexBuffer = meshRes.indexBuffer;
      recordOptions.indexCount = mesh.indices.size();
      recordOptions.topology = mesh.topology;
      recordOptions.descriptorSet = programRes.descriptorSets[bufferId_];
      recordOptions.pipelineLayout = programRes.pipelineLayout;
      recordOptions.pipeline = programRes.pipeline;
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
