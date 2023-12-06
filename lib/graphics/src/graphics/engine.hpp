#pragma once

#include <render/context.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace graphics {

namespace fs = std::filesystem;

const std::string kVertexShaderPath{"../../../shaders/vert.spv"};
const std::string kFragmentShaderPath{"../../../shaders/frag.spv"};

/// Helper function to load the binary data from the files.
///
/// @param path  Path to file.
///
/// @return Loaded binary data.
std::vector<char> ReadFile(fs::path path);

/// Vulkan Engine.
class Engine final {
public:
  void Run() {
    render::ContextOptions options{};
    options.enableValidationLayers = true;
    options.title = "Graphics Engine";
    std::cout << "Initializing the engine..." << std::endl;
    context_.Initialize(options);

    std::cout << "Creating a render pass..." << std::endl;
    render::RenderPassOptions renderPassOptions{};
    renderPassOptions.format = context_.GetSwapChainImageFormat();
    const auto renderPass = context_.CreateRenderPass(renderPassOptions);

    std::cout << "Creating a descriptor set layout..." << std::endl;
    render::DescriptorSetLayoutOptions descriptorSetLayoutOptions{};
    descriptorSetLayoutOptions.binding = 0;
    descriptorSetLayoutOptions.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetLayoutOptions.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    const auto descriptorSetLayout =
        context_.CreateDescriptorSetLayout(descriptorSetLayoutOptions);

    std::cout << "Creating a pipeline layout..." << std::endl;
    render::PipelineLayoutOptions pipelineLayoutOptions{};
    pipelineLayoutOptions.descriptorSetLayout = descriptorSetLayout;
    const auto pipelineLayout =
        context_.CreatePipelineLayout(pipelineLayoutOptions);

    std::cout << "Loading vertex shader: " << kVertexShaderPath << std::endl;
    auto vertexShaderCode = ReadFile(kVertexShaderPath);
    const auto vertexShaderModule =
        context_.CreateShaderModule(vertexShaderCode);
    std::cout << "Loading fragment shader: " << kFragmentShaderPath
              << std::endl;
    auto fragmentShaderCode = ReadFile(kFragmentShaderPath);
    const auto fragmentShaderModule =
        context_.CreateShaderModule(fragmentShaderCode);

    std::cout << "Creating a graphics pipeline..." << std::endl;
    render::GraphicsPipelineOptions pipelineOptions{};
    pipelineOptions.pipelineLayout = pipelineLayout;
    pipelineOptions.renderPass = renderPass;
    pipelineOptions.vertexShader = vertexShaderModule;
    pipelineOptions.fragmentShader = fragmentShaderModule;
    pipelineOptions.viewportExtent = context_.GetSwapChainExtent();
    pipelineOptions.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineOptions.polygonMode = VK_POLYGON_MODE_FILL;
    const auto pipeline = context_.CreateGraphicsPipeline(pipelineOptions);

    std::cout << "Creating framebuffers..." << std::endl;
    context_.CreateSwapChainFramebuffers(renderPass);

    std::cout << "Creating a command pool..." << std::endl;
    render::CommandPoolOptions commandPoolOptions{};
    const auto commandPool = context_.CreateCommandPool(commandPoolOptions);

    std::cout << "Creating a vertex buffer..." << std::endl;
    render::VertexBufferOptions vertexBufferOptions{};
    vertexBufferOptions.commandPool = commandPool;
    vertexBufferOptions.vertices = vertices_;
    const auto vertexBuffer = context_.CreateVertexBuffer(vertexBufferOptions);

    std::cout << "Creating a index buffer..." << std::endl;
    render::IndexBufferOptions indexBufferOptions{};
    indexBufferOptions.commandPool = commandPool;
    indexBufferOptions.indices = indices_;
    const auto indexBuffer = context_.CreateIndexBuffer(indexBufferOptions);

    std::cout << "Creating a descriptor pool..." << std::endl;
    render::DescriptorPoolOptions descriptorPoolOptions{};
    descriptorPoolOptions.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolOptions.descriptorCount = render::kMaxFramesInFlight;
    const auto descriptorPool =
        context_.CreateDescriptorPool(descriptorPoolOptions);

    std::cout << "Creating descriptor sets..." << std::endl;
    std::vector<VkDescriptorSet> descriptorSets{};
    for (std::size_t id{0}; id < render::kMaxFramesInFlight; ++id) {
      render::DescriptorSetOptions descriptorSetOptions{};
      descriptorSetOptions.descriptorPool = descriptorPool;
      descriptorSetOptions.descriptorSetLayout = descriptorSetLayout;
      const auto descriptorSet =
          context_.CreateDescriptorSet(descriptorSetOptions);
      descriptorSets.push_back(descriptorSet);
    }

    std::cout << "Creating an uniform buffer..." << std::endl;
    std::vector<VkBuffer> uniformBuffers{};
    for (const auto &descriptorSet : descriptorSets) {
      const auto uniformBuffer = context_.CreateUniformBuffer();
      uniformBuffers.push_back(uniformBuffer);
      render::UpdateDescriptorSetOptions updateDescriptorSetOptions{};
      updateDescriptorSetOptions.descriptorSet = descriptorSet;
      updateDescriptorSetOptions.descriptorType =
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      updateDescriptorSetOptions.uniformBuffer = uniformBuffer;
      context_.UpdateDescriptorSet(updateDescriptorSetOptions);
    }

    std::cout << "Creating command buffers..." << std::endl;
    std::vector<VkCommandBuffer> commandBuffers{};
    for (std::size_t id{0}; id < render::kMaxFramesInFlight; ++id) {
      render::CommandBufferOptions commandBufferOptions{};
      commandBufferOptions.commandPool = commandPool;
      const auto commandBuffer =
          context_.CreateCommandBuffer(commandBufferOptions);
      commandBuffers.push_back(commandBuffer);
    }

    const auto window = context_.GetWindow();

    /// Main engine loop.
    const auto startTime = std::chrono::high_resolution_clock::now();
    std::uint32_t bufferId{0};
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      render::BeginFrameOptions beginFrameOptions{};
      beginFrameOptions.renderPass = renderPass;
      context_.BeginFrame(beginFrameOptions);

      render::RecordCommandBufferOptions recordOptions{};
      recordOptions.vertexBuffer = vertexBuffer;
      recordOptions.indexBuffer = indexBuffer;
      recordOptions.indexCount = indices_.size();
      recordOptions.descriptorSet = descriptorSets[bufferId];
      recordOptions.commandBuffer = commandBuffers[bufferId];
      recordOptions.renderPass = renderPass;
      recordOptions.pipelineLayout = pipelineLayout;
      recordOptions.pipeline = pipeline;
      recordOptions.clearColor = VkClearValue{127, 127, 127, 127};
      context_.RecordCommandBuffer(recordOptions);

      const auto swapChainExtent = context_.GetSwapChainExtent();
      const auto currentTime = std::chrono::high_resolution_clock::now();
      const float time =
          std::chrono::duration<float, std::chrono::seconds::period>(
              currentTime - startTime)
              .count();
      render::UniformBufferObject ubo{};
      ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                              glm::vec3(0.0f, 0.0f, 1.0f));
      ubo.view =
          glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                      glm::vec3(0.0f, 0.0f, 1.0f));
      ubo.proj = glm::perspective(
          glm::radians(45.0f),
          swapChainExtent.width / static_cast<float>(swapChainExtent.height),
          0.1f, 10.0f);
      ubo.proj[1][1] *= -1;
      context_.UpdateUniformBuffer(
          render::UpdateUniformBufferOptions{bufferId, ubo});

      render::EndFrameOptions endFrameOptions{};
      endFrameOptions.renderPass = renderPass;
      endFrameOptions.commandBuffer = commandBuffers[bufferId];
      context_.EndFrame(endFrameOptions);

      bufferId = (bufferId + 1) % render::kMaxFramesInFlight;
    }

    context_.WaitIdle();
    context_.Cleanup();
  }

private:
  const std::vector<render::Vertex> vertices_{
      {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
  const std::vector<std::uint16_t> indices_{0, 1, 2, 2, 3, 0};
  render::Context context_{};
};

} // namespace graphics