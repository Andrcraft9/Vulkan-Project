#pragma once

#include <render/utils.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <VkBootstrap.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glog/logging.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace render {

constexpr std::uint8_t kMaxFramesInFlight{2};
constexpr std::uint8_t kMaxSwapchainImages{4};

struct Vertex final {
  glm::vec3 position{};
  glm::vec2 uv{};
};

inline VkVertexInputBindingDescription GetBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescription;
}

inline std::vector<VkVertexInputAttributeDescription>
GetAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

  VkVertexInputAttributeDescription vertexInputAttribute0{};
  vertexInputAttribute0.binding = 0;
  vertexInputAttribute0.location = 0;
  vertexInputAttribute0.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttribute0.offset = offsetof(Vertex, position);
  attributeDescriptions.push_back(vertexInputAttribute0);

  VkVertexInputAttributeDescription vertexInputAttribute1{};
  vertexInputAttribute1.binding = 0;
  vertexInputAttribute1.location = 1;
  vertexInputAttribute1.format = VK_FORMAT_R32G32_SFLOAT;
  vertexInputAttribute1.offset = offsetof(Vertex, uv);
  attributeDescriptions.push_back(vertexInputAttribute1);

  return attributeDescriptions;
}

struct UniformBufferObject final {
  glm::mat4 model{};
  glm::mat4 view{};
  glm::mat4 proj{};
};

struct ShaderModuleOptions final {
  const void *data{};
  std::size_t size{};
};

struct ContextOptions final {
  bool enableValidationLayers{true};
  std::string title{"Vulkan Project Engine"};
};

struct ImageViewOptions final {
  VkImage image{VK_NULL_HANDLE};
  VkFormat format{};
};

struct RenderPassOptions final {
  VkFormat format{};
};

struct DescriptorSetLayoutBindingOptions final {
  std::uint32_t binding{};
  VkDescriptorType type{};
  VkShaderStageFlags stageFlags{};
};

struct DescriptorSetLayoutOptions final {
  std::vector<DescriptorSetLayoutBindingOptions> bindingOptions{};
};

struct PipelineLayoutOptions final {
  VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
};

struct GraphicsPipelineOptions final {
  VkShaderModule vertexShader{VK_NULL_HANDLE};
  VkVertexInputBindingDescription vertexInputBinding{};
  std::vector<VkVertexInputAttributeDescription> vertexInputAttribues{};
  VkShaderModule fragmentShader{VK_NULL_HANDLE};
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  VkPrimitiveTopology topology{};
  VkPolygonMode polygonMode{};
  VkExtent2D viewportExtent{};
};

struct CommandPoolOptions final {};

struct CommandBufferOptions final {
  VkCommandPool commandPool{VK_NULL_HANDLE};
};

struct VertexBufferOptions final {
  VkCommandPool commandPool{VK_NULL_HANDLE};
  const void *bufferData{};
  VkDeviceSize bufferSize{};
};

struct IndexBufferOptions final {
  VkCommandPool commandPool{VK_NULL_HANDLE};
  const std::uint16_t* bufferData{};
  VkDeviceSize bufferSize{};
};

struct DescriptorPoolSizeOptions final {
  VkDescriptorType type{};
  std::uint32_t descriptorCount{};
};

struct DescriptorPoolOptions final {
  std::vector<DescriptorPoolSizeOptions> poolSizeOptions{};
  std::uint32_t maxSets{};
};

struct DescriptorSetOptions final {
  VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
};

struct DescriptorUniformBufferInfo final {
  VkBuffer buffer{VK_NULL_HANDLE};
  std::uint32_t binding{};
};

struct DescriptorImageInfo final {
  VkImageView imageView{VK_NULL_HANDLE};
  VkSampler sampler{VK_NULL_HANDLE};
  std::uint32_t binding{};
};

struct UpdateDescriptorSetOptions final {
  VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
  std::vector<DescriptorUniformBufferInfo> descriptorUniformBuffers{};
  std::vector<DescriptorImageInfo> descriptorImages{};
};

struct RecordCommandBufferOptions final {
  VkPipeline pipeline{VK_NULL_HANDLE};
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
  VkClearValue clearColor{};
  VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
  VkBuffer vertexBuffer{VK_NULL_HANDLE};
  VkBuffer indexBuffer{VK_NULL_HANDLE};
  std::uint32_t indexCount{};
};

struct BeginFrameOptions final {
};

struct UpdateUniformBufferOptions final {
  std::uint32_t uniformBufferIndex{};
  UniformBufferObject data{};
};

struct BeginFrameInfo final {
  bool ifSwapchainRecreated{};
};

struct EndFrameOptions final {
  VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
};

struct EndFrameInfo final {};

struct TextureImageOptions final {
  VkCommandPool commandPool{VK_NULL_HANDLE};
  const ImageData *imageData{nullptr};
};

struct TextureSamplerOptions final {
  VkFilter magFilter{VK_FILTER_LINEAR};
  VkFilter minFilter{VK_FILTER_LINEAR};
  VkSamplerAddressMode addressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};
  bool anisotropyEnable{false};
  VkBorderColor borderColor{VK_BORDER_COLOR_INT_OPAQUE_BLACK};
};

/// Context provides render interfaces based on Vulkan API.
class Context final {
public:
  struct QueueFamilyIndices final {
    std::optional<std::uint32_t> graphicsFamily;
    std::optional<std::uint32_t> presentFamily;
  };

  struct SwapchainSupportDetails final {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };

  /// @name Vulkan Initialization
  /// @{

  /// Initializes the Vulkan library doing the following steps:
  /// - Checks validation layers.
  /// - Creates an instance.
  /// - Creates surface.
  /// - Selects a physical device.
  /// - Queries queue families supported.
  /// - Creates a logical device.
  /// - Creates default swapchain.
  void Initialize(const ContextOptions &options);

  /// Destroys all Vulkan created resources and terminates GLFW.
  void Cleanup();

  /// @}

  /// @name Resource Creation
  /// @{

  VkImageView CreateImageView(const ImageViewOptions &options);

  VkShaderModule CreateShaderModule(const ShaderModuleOptions &options);

  VkDescriptorSetLayout
  CreateDescriptorSetLayout(const DescriptorSetLayoutOptions &options);

  VkPipelineLayout CreatePipelineLayout(const PipelineLayoutOptions &options);

  VkPipeline CreateGraphicsPipeline(const GraphicsPipelineOptions &options);

  VkCommandPool CreateCommandPool(const CommandPoolOptions &options);

  VkCommandBuffer CreateCommandBuffer(const CommandBufferOptions &options);

  /// Creates a vertex buffer.
  ///
  /// Uses staging buffer to upload the data from the vertex array.
  VkBuffer CreateVertexBuffer(const VertexBufferOptions &options);

  /// Creates a index buffer.
  ///
  /// Uses staging buffer to upload the data from the index array.
  VkBuffer CreateIndexBuffer(const IndexBufferOptions &options);

  /// Creates an uniform buffer with mapped memory.
  VkBuffer CreateUniformBuffer();

  VkDescriptorPool CreateDescriptorPool(const DescriptorPoolOptions &options);

  VkDescriptorSet CreateDescriptorSet(const DescriptorSetOptions &options);

  VkImage CreateTextureImage(const TextureImageOptions &options);

  VkSampler CreateTextureSampler(const TextureSamplerOptions &options);

  /// @}

  /// @name Management
  /// @{

  void UpdateDescriptorSet(const UpdateDescriptorSetOptions &options);

  /// Writes the commands to be executed into the command buffer.
  void RecordCommandBuffer(const RecordCommandBufferOptions &options);

  void UpdateUniformBuffer(const UpdateUniformBufferOptions &options);

  BeginFrameInfo BeginFrame(const BeginFrameOptions &options);

  EndFrameInfo EndFrame(const EndFrameOptions &options);

  VkFormat GetSwapchainImageFormat() { return swapchain_.image_format; }

  VkExtent2D GetSwapchainExtent() { return swapchain_.extent; }

  GLFWwindow *GetWindow() { return window_; }

  void WaitIdle() { vkDeviceWaitIdle(device_); }

  /// @}

private:
  /// @name Internal
  /// @{

  /// Initializes GLFW and creates a window.
  void InitWindow();

  /// Creates the synchronization objects.
  void CreateSyncObjects();

  /// Creates the default swapchain.
  void CreateSwapchain();

  /// Finds suitable memory type.
  std::uint32_t FindMemoryType(std::uint32_t typeFilter,
                               VkMemoryPropertyFlags properties);

  VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool);

  void EndSingleTimeCommands(VkCommandPool commandPool,
                             VkCommandBuffer commandBuffer);

  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer &buffer,
                    VkDeviceMemory &bufferMemory);

  void CopyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer,
                  VkBuffer dstBuffer, VkDeviceSize size);

  void CleanupSwapchain();

  void RecreateSwapchain();

  void CreateImage(std::uint32_t width, std::uint32_t height, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage &image,
                   VkDeviceMemory &imageMemory);

  void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkAccessFlags srcAccessMask,
                             VkAccessFlags dstAccessMask,
                             VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage);

  void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void TransitionImageLayout(VkCommandPool commandPool, VkImage image,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void CopyBufferToImage(VkCommandPool commandPool, VkBuffer buffer,
                         VkImage image, uint32_t width, uint32_t height);

  /// @}

  /// Window resources.
  std::uint32_t width_{1600U};
  std::uint32_t height_{1200U};
  GLFWwindow *window_{nullptr};

  /// Vulkan System resources.
  vkb::Instance instance_{};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  vkb::PhysicalDevice physicalDevice_{};
  vkb::Device device_{};
  // TODO: Use dispatch table for all Vulkan calls.
  vkb::DispatchTable dispatch_{};
  VkQueue graphicsQueue_{VK_NULL_HANDLE};
  VkQueue presentQueue_{VK_NULL_HANDLE};

  /// Swapchain resources.
  vkb::Swapchain swapchain_{};
  std::vector<VkImage> swapchainImages_{};
  std::vector<VkImageView> swapchainImageViews_{};
  std::uint32_t currentSwapchainImageIndex_{};

  /// Image view resources.
  std::vector<VkImageView> imageViews_{};

  /// Shader module resources.
  std::vector<VkShaderModule> shaderModules_{};

  /// Pipeline layout resources.
  std::vector<VkPipelineLayout> pipelineLayouts_;

  /// Descriptor set layout resources.
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts_{};

  /// Graphics pipeline resources.
  std::vector<VkPipeline> pipelines_;

  /// Command pool resources.
  std::vector<VkCommandPool> commandPools_{};

  /// Command buffer resources.
  std::vector<VkCommandBuffer> commandBuffers_{};

  /// Vertex buffer resources.
  std::vector<VkBuffer> vertexBuffers_{};
  std::vector<VkDeviceMemory> vertexBufferMemories_{};

  /// Index buffer resources.
  std::vector<VkBuffer> indexBuffers_{};
  std::vector<VkDeviceMemory> indexBufferMemories_{};

  /// Uniform buffer resources.
  std::vector<VkBuffer> uniformBuffers_{};
  std::vector<VkDeviceMemory> uniformBufferMemories_{};
  std::vector<void *> uniformBufferMappedMemories_{};

  /// Texture image resources.
  std::vector<VkImage> textureImages_{};
  std::vector<VkDeviceMemory> textureImageMemories_{};

  // Texture samplers.
  std::vector<VkSampler> samplers_{};

  /// Descriptor pool resources.
  std::vector<VkDescriptorPool> descriptorPools_{};

  /// Descriptor set resources.
  std::vector<VkDescriptorSet> descriptorSets_{};

  /// Synchronization resources.
  std::vector<VkSemaphore> imageAvailableSemaphores_{};
  std::vector<VkSemaphore> renderFinishedSemaphores_{};
  std::vector<VkFence> inFlightFences_{};
  std::uint32_t currentFrame_{0};
};

} // namespace render
