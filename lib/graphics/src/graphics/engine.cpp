#include "graphics/engine.hpp"

namespace graphics {

std::vector<char> ReadFile(fs::path path) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.is_open()) {
    throw std::runtime_error("failed to open file!");
  }
  const auto sz = fs::file_size(path);
  std::vector<char> result(sz);
  f.read(result.data(), sz);
  return result;
}

void Engine::Initialize(const EngineInitializationOptions &options) {
  render::ContextOptions contextOptions{};
  contextOptions.enableValidationLayers = true;
  contextOptions.title = "Graphics Engine";
  LOG(INFO) << "Initializing the engine...";
  context_.Initialize(contextOptions);

  LOG(INFO) << "Creating a render pass...";
  render::RenderPassOptions renderPassOptions{};
  renderPassOptions.format = context_.GetSwapChainImageFormat();
  renderPass_ = context_.CreateRenderPass(renderPassOptions);

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
  pipelineLayout_ = context_.CreatePipelineLayout(pipelineLayoutOptions);

  LOG(INFO) << "Loading vertex shader:" << options.vertexShaderPath;
  auto vertexShaderCode = ReadFile(options.vertexShaderPath);
  const auto vertexShaderModule = context_.CreateShaderModule(vertexShaderCode);
  LOG(INFO) << "Loading fragment shader: " << options.fragmentShaderPath;
  auto fragmentShaderCode = ReadFile(options.fragmentShaderPath);
  const auto fragmentShaderModule =
      context_.CreateShaderModule(fragmentShaderCode);

  LOG(INFO) << "Creating a graphics pipeline...";
  render::GraphicsPipelineOptions pipelineOptions{};
  pipelineOptions.pipelineLayout = pipelineLayout_;
  pipelineOptions.renderPass = renderPass_;
  pipelineOptions.vertexShader = vertexShaderModule;
  pipelineOptions.fragmentShader = fragmentShaderModule;
  pipelineOptions.viewportExtent = context_.GetSwapChainExtent();
  pipelineOptions.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipelineOptions.polygonMode = VK_POLYGON_MODE_FILL;
  pipeline_ = context_.CreateGraphicsPipeline(pipelineOptions);

  LOG(INFO) << "Creating framebuffers...";
  context_.CreateSwapChainFramebuffers(renderPass_);

  LOG(INFO) << "Creating a command pool...";
  render::CommandPoolOptions commandPoolOptions{};
  const auto commandPool = context_.CreateCommandPool(commandPoolOptions);

  LOG(INFO) << "Creating a vertex buffer...";
  render::VertexBufferOptions vertexBufferOptions{};
  vertexBufferOptions.commandPool = commandPool;
  vertexBufferOptions.vertices = options.mesh.vertices;
  vertexBuffer_ = context_.CreateVertexBuffer(vertexBufferOptions);

  LOG(INFO) << "Creating a index buffer...";
  render::IndexBufferOptions indexBufferOptions{};
  indexBufferOptions.commandPool = commandPool;
  indexBufferOptions.indices = options.mesh.indices;
  indexBuffer_ = context_.CreateIndexBuffer(indexBufferOptions);

  LOG(INFO) << "Creating a texture image...";
  render::TextureImageOptions textureImageOptions{};
  textureImageOptions.commandPool = commandPool;
  textureImageOptions.imageData = &options.texture.image;
  textureImage_ = context_.CreateTextureImage(textureImageOptions);

  LOG(INFO) << "Creating a texture image view...";
  render::ImageViewOptions textureImageViewOptions{};
  textureImageViewOptions.format = VK_FORMAT_R8G8B8A8_SRGB;
  textureImageViewOptions.image = textureImage_;
  textureImageView_ = context_.CreateImageView(textureImageViewOptions);

  LOG(INFO) << "Creating a texture sampler...";
  render::TextureSamplerOptions textureSamplerOptions{};
  textureSamplerOptions.magFilter = VK_FILTER_LINEAR;
  textureSamplerOptions.minFilter = VK_FILTER_LINEAR;
  textureSamplerOptions.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  textureSamplerOptions.anisotropyEnable = false;
  textureSamplerOptions.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  textureSampler_ = context_.CreateTextureSampler(textureSamplerOptions);

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

  descriptorSets_.clear();
  for (std::size_t id{0}; id < render::kMaxFramesInFlight; ++id) {
    LOG(INFO) << "Creating a descriptor set...";
    render::DescriptorSetOptions descriptorSetOptions{};
    descriptorSetOptions.descriptorPool = descriptorPool;
    descriptorSetOptions.descriptorSetLayout = descriptorSetLayout;
    const auto descriptorSet =
        context_.CreateDescriptorSet(descriptorSetOptions);
    descriptorSets_.push_back(descriptorSet);
  }

  std::vector<VkBuffer> uniformBuffers{};
  for (const auto &descriptorSet : descriptorSets_) {
    LOG(INFO) << "Creating an uniform buffer...";
    const auto uniformBuffer = context_.CreateUniformBuffer();
    uniformBuffers.push_back(uniformBuffer);

    LOG(INFO) << "Updating a descriptor set...";

    // Uniform Buffer.
    std::vector<render::DescriptorUniformBufferInfo> uniformBufferInfos{};
    uniformBufferInfos.emplace_back(
        render::DescriptorUniformBufferInfo{uniformBuffer, 0});
    // Texture.
    std::vector<render::DescriptorImageInfo> imageInfos{};
    imageInfos.emplace_back(
        render::DescriptorImageInfo{textureImageView_, textureSampler_, 1});
    render::UpdateDescriptorSetOptions updateDescriptorSetOptions{};
    updateDescriptorSetOptions.descriptorSet = descriptorSet;
    updateDescriptorSetOptions.descriptorUniformBuffers =
        std::move(uniformBufferInfos);
    updateDescriptorSetOptions.descriptorImages = std::move(imageInfos);

    context_.UpdateDescriptorSet(updateDescriptorSetOptions);
  }

  LOG(INFO) << "Creating command buffers...";
  commandBuffers_.clear();
  for (std::size_t id{0}; id < render::kMaxFramesInFlight; ++id) {
    render::CommandBufferOptions commandBufferOptions{};
    commandBufferOptions.commandPool = commandPool;
    const auto commandBuffer =
        context_.CreateCommandBuffer(commandBufferOptions);
    commandBuffers_.push_back(commandBuffer);
  }

  indexCount_ = options.mesh.indices.size();
  bufferId_ = 0;
}

void Engine::Render(const EngineRenderOptions &options) {
  render::BeginFrameOptions beginFrameOptions{};
  beginFrameOptions.renderPass = renderPass_;
  context_.BeginFrame(beginFrameOptions);

  render::RecordCommandBufferOptions recordOptions{};
  recordOptions.vertexBuffer = vertexBuffer_;
  recordOptions.indexBuffer = indexBuffer_;
  recordOptions.indexCount = indexCount_;
  recordOptions.descriptorSet = descriptorSets_[bufferId_];
  recordOptions.commandBuffer = commandBuffers_[bufferId_];
  recordOptions.renderPass = renderPass_;
  recordOptions.pipelineLayout = pipelineLayout_;
  recordOptions.pipeline = pipeline_;
  recordOptions.clearColor = options.clearColor;
  context_.RecordCommandBuffer(recordOptions);

  context_.UpdateUniformBuffer(
      render::UpdateUniformBufferOptions{bufferId_, options.ubo});

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

VkExtent2D Engine::Extent() { return context_.GetSwapChainExtent(); }

} // namespace graphics