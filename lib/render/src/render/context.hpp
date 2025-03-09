#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glog/logging.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <stb/stb_image.h>

namespace render {

class ImageData final {
public:
  ImageData();
  ImageData(std::string path);
  ImageData(const unsigned char* data, std::size_t size);

  ImageData(ImageData&& other);

  ~ImageData();

  int Width() const {
    return width_;
  }

  int Height() const {
    return height_;
  }

  int Components() const {
    return components_;
  }

  stbi_uc* Data() const {
    return data_;
  }

  ImageData(const ImageData&) = delete;
  ImageData& operator=(const ImageData&) = delete;
  ImageData& operator=(ImageData&&) = delete;

private:
  int width_{};
  int height_{};
  int components_{};
  stbi_uc *data_{};
};

constexpr int kMaxFramesInFlight{2};

/// Defines vertex data.
struct Vertex final {
  glm::vec2 pos;
  glm::vec3 color;
  glm::vec2 texCoord;
};

/// Returns bindings description: spacing between data and whether the data is
/// per-vertex or per-instance.
inline VkVertexInputBindingDescription GetBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescription;
}

/// Returns Attribute descriptions: type of the attributes passed to the
/// vertex shader, which binding to load them from and at which offset.
inline std::array<VkVertexInputAttributeDescription, 3>
GetAttributeDescriptions() {
  std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex, pos);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex, color);

  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

  return attributeDescriptions;
}

/// Defines uniform buffer.
struct UniformBufferObject final {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
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

struct FrameBufferOptions final {
  VkRenderPass renderPass{VK_NULL_HANDLE};
  VkImageView imageAttachment{VK_NULL_HANDLE};
  VkExtent2D extent{};
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
  VkShaderModule fragmentShader{VK_NULL_HANDLE};
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  VkRenderPass renderPass{VK_NULL_HANDLE};
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
  std::vector<Vertex> vertices{};
};

struct IndexBufferOptions final {
  VkCommandPool commandPool{VK_NULL_HANDLE};
  std::vector<std::uint16_t> indices{};
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
  VkRenderPass renderPass{VK_NULL_HANDLE};
  VkClearValue clearColor{};
  VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
  VkBuffer vertexBuffer{VK_NULL_HANDLE};
  VkBuffer indexBuffer{VK_NULL_HANDLE};
  std::uint32_t indexCount{};
};

struct BeginFrameOptions final {
  VkRenderPass renderPass{VK_NULL_HANDLE};
};

struct UpdateUniformBufferOptions final {
  std::uint32_t uniformBufferIndex{};
  UniformBufferObject data{};
};

struct BeginFrameInfo final {
  bool ifSwapchainRecreated{};
};

struct EndFrameOptions final {
  VkRenderPass renderPass{VK_NULL_HANDLE};
  VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
};

struct EndFrameInfo final {};

struct TextureImageOptions final {
  VkCommandPool commandPool{VK_NULL_HANDLE};
  const ImageData* imageData{nullptr};
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

  struct SwapChainSupportDetails final {
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

  /// Create framebuffers for default swapchain.
  void CreateSwapChainFramebuffers(VkRenderPass renderPass);

  /// Destroys all Vulkan created resources and terminates GLFW.
  void Cleanup();

  /// Creates an image view.
  VkImageView CreateImageView(const ImageViewOptions &options);

  /// @}

  /// @name Graphics Pipeline Creation
  /// @{

  /// Creates a shader module from the shader bytecode (SPIR-V).
  VkShaderModule CreateShaderModule(const std::vector<char> &code);

  /// Creates a render pass.
  VkRenderPass CreateRenderPass(const RenderPassOptions &options);

  /// Creates a framebuffer object.
  VkFramebuffer CreateFramebuffer(const FrameBufferOptions &options);

  /// Creates a description set layout.
  VkDescriptorSetLayout
  CreateDescriptorSetLayout(const DescriptorSetLayoutOptions &options);

  /// Creates a pipeline layout.
  VkPipelineLayout CreatePipelineLayout(const PipelineLayoutOptions &options);

  /// Creates a graphics pipeline.
  VkPipeline CreateGraphicsPipeline(const GraphicsPipelineOptions &options);

  /// Creates a command pool.
  VkCommandPool CreateCommandPool(const CommandPoolOptions &options);

  /// Creates command buffers from the command pool.
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

  /// Creates descriptor sets from the descriptor pool.
  VkDescriptorSet CreateDescriptorSet(const DescriptorSetOptions &options);

  void UpdateDescriptorSet(const UpdateDescriptorSetOptions &options);

  /// Writes the commands to be executed into the command buffer.
  void RecordCommandBuffer(const RecordCommandBufferOptions &options);

  void UpdateUniformBuffer(const UpdateUniformBufferOptions &options);

  /// Begins a new frame.
  BeginFrameInfo BeginFrame(const BeginFrameOptions &options);

  /// Ends the current frame.
  EndFrameInfo EndFrame(const EndFrameOptions &options);

  /// Creates a texture image.
  VkImage CreateTextureImage(const TextureImageOptions &options);

  VkSampler CreateTextureSampler(const TextureSamplerOptions &options);

  /// @}

  VkFormat GetSwapChainImageFormat() { return swapChainImageFormat_; }

  VkExtent2D GetSwapChainExtent() { return swapChainExtent_; }

  GLFWwindow *GetWindow() { return window_; }

  void WaitIdle() { vkDeviceWaitIdle(device_); }

private:
  static void FramebufferResizeCallback(GLFWwindow *window, int width,
                                        int height) {
    auto app = reinterpret_cast<Context *>(glfwGetWindowUserPointer(window));
    app->framebufferResized_ = true;
  }

  /// Initializes GLWF and creates a window.
  void InitWindow();

  /// Checks if all of the requested layers are available.
  bool CheckValidationLayerSupport();

  /// Checks if the physical device is suitable for the operations we want to
  /// perform, because not all graphics cards are created equal.
  bool IsDeviceSuitable(VkPhysicalDevice device);

  /// Check if all of the requested extensions are supported by the physical
  /// device.
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

  /// Function checks which queue families are supported by the device and
  /// which one of these supports the commands that we want to use.
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

  /// Creates a swapchain.
  void CreateSwapChain();

  /// Queries details of swap chain support.
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

  /// Chooses the surface format for the swap chain.
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);

  /// Chooses the present mode for the swap chain.
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);

  /// Chooses the swap extent for the swap chain.
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  /// Finds suitable memory type.
  std::uint32_t FindMemoryType(std::uint32_t typeFilter,
                               VkMemoryPropertyFlags properties);

  VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool);

  void EndSingleTimeCommands(VkCommandPool commandPool,
                             VkCommandBuffer commandBuffer);

  /// Creates a Vulkan buffer.
  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer &buffer,
                    VkDeviceMemory &bufferMemory);

  /// Copies data from one Vulkan buffer to another one.
  void CopyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer,
                  VkBuffer dstBuffer, VkDeviceSize size);

  /// Creates the synchronization objects.
  void CreateSyncObjects();

  void CleanupSwapChain();

  void RecreateSwapChain(VkRenderPass renderPass);

  void CreateImage(std::uint32_t width, std::uint32_t height, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage &image,
                   VkDeviceMemory &imageMemory);

  void TransitionImageLayout(VkCommandPool commandPool, VkImage image,
                             VkFormat format, VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void CopyBufferToImage(VkCommandPool commandPool, VkBuffer buffer,
                         VkImage image, uint32_t width, uint32_t height);

  /// Window resources.
  std::uint32_t width_{1600U};
  std::uint32_t height_{1200U};
  GLFWwindow *window_{nullptr};

  /// Vulkan System resources.
  const std::vector<const char *> validationLayers_{
      "VK_LAYER_KHRONOS_validation"};
  const std::vector<const char *> deviceExtensions_{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkInstance instance_{VK_NULL_HANDLE};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue graphicsQueue_{VK_NULL_HANDLE};
  VkQueue presentQueue_{VK_NULL_HANDLE};

  /// Swapchain resources.
  VkSwapchainKHR swapChain_{VK_NULL_HANDLE};
  VkFormat swapChainImageFormat_{};
  VkExtent2D swapChainExtent_{};
  std::vector<VkImage> swapChainImages_{};
  std::vector<VkImageView> swapChainImageViews_{};
  std::vector<VkFramebuffer> swapChainFramebuffers_{};
  std::uint32_t currentSwapchainImageIndex_{};

  /// Image view resources.
  std::vector<VkImageView> imageViews_{};

  /// Shader module resources.
  std::vector<VkShaderModule> shaderModules_{};

  /// Render pass resources.
  std::vector<VkRenderPass> renderPasses_{};

  /// Framebuffer resources.
  std::vector<VkFramebuffer> framebuffers_{};

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

  std::uint32_t currentFrame_{0};

  std::vector<VkSemaphore> imageAvailableSemaphores_{};
  std::vector<VkSemaphore> renderFinishedSemaphores_{};
  std::vector<VkFence> inFlightFences_{};
  bool framebufferResized_{false};
};

} // namespace render