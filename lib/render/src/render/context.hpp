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
  std::string pathToImage{};
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
  /// - Checks validation layers:
  ///   Validation layers are optional components that hook into Vulkan function
  ///   calls to apply additional operations. VK_LAYER_KHRONOS_validation is
  ///   used provided by Vulkan SDK.
  ///
  /// - Creates an instance:
  ///   There is no global state in Vulkan and all per-application state is
  ///   stored in a VkInstance object. Creating a VkInstance object initializes
  ///   the Vulkan library and allows the application to pass information about
  ///   itself to the implementation.
  ///
  /// - Creates surface:
  ///   VK_KHR_surface is used to establish the connection between Vulkan and
  ///   the window system to present results to the screen. A VkSurfaceKHR
  ///   represents an abstract type of surface to present rendered images to.
  ///   The surface in our program will be backed by the window that we've
  ///   already opened with GLFW.
  ///
  /// - Selects a physical device:
  ///   After initializing the Vulkan library through a VkInstance we need to
  ///   look for and select a graphics card in the system that supports the
  ///   features we need.
  ///
  /// - Queries queue families supported for the selected physical device.
  ///   Here, queues with graphics capabilities and presentation support are
  ///   essential for us.
  ///
  /// - Creates a logical device:
  ///   The created logical device is the primary interface to the physical
  ///   device. Creating a logical device also creates the queues associated
  ///   with that device.
  ///
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
  ///
  /// @param code  Shader SPIR-V bytecode.
  ///
  /// @return VkShaderModule.
  VkShaderModule CreateShaderModule(const std::vector<char> &code);

  /// Creates a render pass.
  ///
  /// Each render pass instance defines a set of image resources, referred to as
  /// attachments, used during rendering (framebuffer attachments).
  ///
  /// Render passes operate in conjunction with framebuffers.
  VkRenderPass CreateRenderPass(const RenderPassOptions &options);

  /// Creates a framebuffer object.
  ///
  /// The attachments specified during render pass creation are bound by
  /// wrapping them into a VkFramebuffer object. Framebuffers represent a
  /// collection of specific memory attachments that a render pass instance
  /// uses.
  VkFramebuffer CreateFramebuffer(const FrameBufferOptions &options);

  /// Creates a description set layout.
  ///
  /// Specifies the types of resources that are going to be accessed by the
  /// pipeline.
  VkDescriptorSetLayout
  CreateDescriptorSetLayout(const DescriptorSetLayoutOptions &options);

  /// Creates a pipeline layout.
  VkPipelineLayout CreatePipelineLayout(const PipelineLayoutOptions &options);

  /// Creates a graphics pipeline.
  VkPipeline CreateGraphicsPipeline(const GraphicsPipelineOptions &options);

  /// Creates a command pool.
  ///
  /// Command pools are opaque objects that command buffer memory is allocated
  /// from, and which allow the implementation to amortize the cost of resource
  /// creation across multiple command buffers. Command pools are externally
  /// synchronized, meaning that a command pool must not be used concurrently in
  /// multiple threads. That includes use via recording commands on any command
  /// buffers allocated from the pool, as well as operations that allocate,
  /// free, and reset command buffers or the pool itself.
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
  ///
  /// Specifies the actual buffer or image resources that will be bound to the
  /// descriptors.
  VkDescriptorSet CreateDescriptorSet(const DescriptorSetOptions &options);

  void UpdateDescriptorSet(const UpdateDescriptorSetOptions &options);

  /// Writes the commands to be executed into the command buffer.
  ///
  /// @param commandBuffer  Command buffer.
  /// @param imageIndex  Image (framebuffer) index to use in the render pass.
  void RecordCommandBuffer(const RecordCommandBufferOptions &options);

  void UpdateUniformBuffer(const UpdateUniformBufferOptions &options);

  /// Begins a new frame.
  BeginFrameInfo BeginFrame(const BeginFrameOptions &options);

  /// Ends the current frame.
  EndFrameInfo EndFrame(const EndFrameOptions &options);

  /// Creates a texture image.
  ///
  /// @param options  Texture image options.
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
  ///
  /// @param device  Physical device to check.
  ///
  /// @return True if the device is suitable, false otherwise.
  bool IsDeviceSuitable(VkPhysicalDevice device);

  /// Check if all of the requested extensions are supported by the physical
  /// device.
  ///
  /// @param device  Physical device.
  ///
  /// @return True if extensions are supported, false otherwise.
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

  /// There are different types of queues that originate from different queue
  /// families and each family of queues allows only a subset of commands.
  /// Function checks which queue families are supported by the device and
  /// which one of these supports the commands that we want to use. We need to
  /// find queue families that support VK_QUEUE_GRAPHICS_BIT (queue with
  /// graphics capabilities) and presentation.
  ///
  /// @param device  Physical device.
  ///
  /// @return QueueFamilyIndices.
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

  /// Creates a swapchain.
  ///
  /// A swapchain object provides the ability to present rendering results to a
  /// surface. A swapchain is an abstraction for an array of presentable images
  /// that are associated with a surface. The presentable images are represented
  /// by VkImage objects created by the platform. One image (which can be an
  /// array image for multiview/stereoscopic-3D surfaces) is displayed at a
  /// time, but multiple images can be queued for presentation. An application
  /// renders to the image, and then queues the image for presentation to the
  /// surface.
  ///
  /// The presentable images of a swapchain are owned by the presentation
  /// engine. An application can acquire use of a presentable image from the
  /// presentation engine. Use of a presentable image must occur only after the
  /// image is returned by vkAcquireNextImageKHR, and before it is released by
  /// vkQueuePresentKHR. This includes transitioning the image layout and
  /// rendering commands.
  void CreateSwapChain();

  /// Queries details of swap chain support.
  ///
  /// There are basically three kinds of properties we need to check:
  /// - Basic surface capabilities (min/max number of images in swap chain,
  ///   min/max width and height of images).
  /// - Surface formats (pixel format, color space).
  /// - Available presentation modes.
  ///
  /// @param device  Physical device.
  ///
  /// @return SwapChainSupportDetails.
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

  /// Chooses the surface format for the swap chain.
  ///
  /// @param availableFormats  Available surface formats.
  ///
  /// @return Choosen surface format.
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);

  /// Chooses the present mode for the swap chain.
  ///
  /// The presentation mode is arguably the most important setting for the swap
  /// chain, because it represents the actual conditions for showing images to
  /// the screen. There are four possible modes available in Vulkan:
  /// - VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are
  ///   transferred to the screen right away, which may result in tearing.
  /// - VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display
  ///   takes an image from the front of the queue when the display is refreshed
  ///   and the program inserts rendered images at the back of the queue. If the
  ///   queue is full then the program has to wait. This is most similar to
  ///   vertical sync as found in modern games. The moment that the display is
  ///   refreshed is known as "vertical blank".
  /// - VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the
  ///   previous one if the application is late and the queue was empty at the
  ///   last vertical blank. Instead of waiting for the next vertical blank, the
  ///   image is transferred right away when it finally arrives. This may result
  ///   in visible tearing.
  /// - VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second
  ///   mode. Instead of blocking the application when the queue is full, the
  ///   images that are already queued are simply replaced with the newer ones.
  ///   This mode can be used to render frames as fast as possible while still
  ///   avoiding tearing, resulting in fewer latency issues than standard
  ///   vertical sync. This is commonly known as "triple buffering", although
  ///   the existence of three buffers alone does not necessarily mean that the
  ///   framerate is unlocked.
  ///
  /// @param availablePresentModes  Available present modes.
  ///
  /// @return Choosen present mode.
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);

  /// Chooses the swap extent for the swap chain.
  ///
  /// The swap extent is the resolution of the swap chain images.
  ///
  /// @param capabilities  VkSurfaceCapabilitiesKHR.
  ///
  /// @return Choosen swap extent.
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  /// Finds suitable memory type.
  std::uint32_t FindMemoryType(std::uint32_t typeFilter,
                               VkMemoryPropertyFlags properties);

  VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool);

  void EndSingleTimeCommands(VkCommandPool commandPool,
                             VkCommandBuffer commandBuffer);

  /// Creates a Vulkan buffer.
  ///
  /// @param size  Buffer size.
  /// @param usage  Buffer usage.
  /// @param properties  Memory properties.
  /// @param buffer  Created buffer.
  /// @param bufferMemory  Allocated memory.
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