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
  render::DescriptorSetLayoutOptions descriptorSetLayoutOptions{};
  descriptorSetLayoutOptions.binding = 0;
  descriptorSetLayoutOptions.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorSetLayoutOptions.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
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

  LOG(INFO) << "Creating a descriptor pool...";
  render::DescriptorPoolOptions descriptorPoolOptions{};
  descriptorPoolOptions.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorPoolOptions.descriptorCount = render::kMaxFramesInFlight;
  const auto descriptorPool =
      context_.CreateDescriptorPool(descriptorPoolOptions);

  LOG(INFO) << "Creating descriptor sets...";
  descriptorSets_.clear();
  for (std::size_t id{0}; id < render::kMaxFramesInFlight; ++id) {
    render::DescriptorSetOptions descriptorSetOptions{};
    descriptorSetOptions.descriptorPool = descriptorPool;
    descriptorSetOptions.descriptorSetLayout = descriptorSetLayout;
    const auto descriptorSet =
        context_.CreateDescriptorSet(descriptorSetOptions);
    descriptorSets_.push_back(descriptorSet);
  }

  LOG(INFO) << "Creating an uniform buffer...";
  std::vector<VkBuffer> uniformBuffers{};
  for (const auto &descriptorSet : descriptorSets_) {
    const auto uniformBuffer = context_.CreateUniformBuffer();
    uniformBuffers.push_back(uniformBuffer);
    render::UpdateDescriptorSetOptions updateDescriptorSetOptions{};
    updateDescriptorSetOptions.descriptorSet = descriptorSet;
    updateDescriptorSetOptions.descriptorType =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    updateDescriptorSetOptions.uniformBuffer = uniformBuffer;
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