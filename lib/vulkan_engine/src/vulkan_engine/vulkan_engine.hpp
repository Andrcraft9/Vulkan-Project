#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <algorithm> // Necessary for std::clamp
#include <cstdint>   // Necessary for uint32_t
#include <filesystem>
#include <fstream>
#include <limits> // Necessary for std::numeric_limits

namespace vulkan_engine {

namespace fs = std::filesystem;

const std::string kVertexShaderPath{"../../../shaders/vert.spv"};
const std::string kFragmentShaderPath{"../../../shaders/frag.spv"};

constexpr int kMaxFramesInFlight{2};

/// Helper function to load the binary data from the files.
///
/// @param path  Path to file.
///
/// @return Loaded binary data.
std::vector<char> readFile(fs::path path);

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  /// Returns bindings description: spacing between data and whether the data is
  /// per-vertex or per-instance.
  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  /// Returns Attribute descriptions: type of the attributes passed to the
  /// vertex shader, which binding to load them from and at which offset.
  static std::array<VkVertexInputAttributeDescription, 2>
  getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);
    return attributeDescriptions;
  }
};

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

/// Vulkan application.
///
/// Based on the tutorial: [https://vulkan-tutorial.com/Introduction]
class VulkanApp final {
public:
  struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphicsFamily;
    std::optional<std::uint32_t> presentFamily;
  };

  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };

  void run() {
    initWindow();
    initVulkan();
    createSwapChain();
    createImageViews();

    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();

    createCommandPool();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();

    mainLoop();

    cleanup();
  }

private:
  /// Main engine loop.
  void mainLoop();

  /// @name Vulkan Setup
  /// @{

  /// Destroys all Vulkan created resources and terminates GLFW.
  void cleanup();

  /// Initializes GLWF and creates a window.
  void initWindow();

  static void framebufferResizeCallback(GLFWwindow *window, int width,
                                        int height) {
    auto app = reinterpret_cast<VulkanApp *>(glfwGetWindowUserPointer(window));
    app->framebufferResized_ = true;
  }

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
  void initVulkan();

  /// Checks if all of the requested layers are available.
  bool checkValidationLayerSupport();

  /// Checks if the physical device is suitable for the operations we want to
  /// perform, because not all graphics cards are created equal.
  ///
  /// @param device  Physical device to check.
  ///
  /// @return True if the device is suitable, false otherwise.
  bool isDeviceSuitable(VkPhysicalDevice device);

  /// Check if all of the requested extensions are supported by the physical
  /// device.
  ///
  /// @param device  Physical device.
  ///
  /// @return True if extensions are supported, false otherwise.
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);

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
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

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
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  /// Chooses the surface format for the swap chain.
  ///
  /// @param availableFormats  Available surface formats.
  ///
  /// @return Choosen surface format.
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
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
  VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);

  /// Chooses the swap extent for the swap chain.
  ///
  /// The swap extent is the resolution of the swap chain images.
  ///
  /// @param capabilities  VkSurfaceCapabilitiesKHR.
  ///
  /// @return Choosen swap extent.
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  /// Creates the swap chain.
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
  void createSwapChain();

  /// Creates image views for every image in the swap chain.
  void createImageViews();

  /// @}

  /// @name Graphics Pipeline Creation
  /// @{

  /// Graphics pipeline stages are the following in general:
  ///
  /// 1. The input assembler collects the raw vertex data from the buffers you
  /// specify and may also use an index buffer to repeat certain elements
  /// without having to duplicate the vertex data itself.
  ///
  /// 2. The vertex shader is run for every vertex and generally applies
  /// transformations to turn vertex positions from model space to screen space.
  /// It also passes per-vertex data down the pipeline.
  ///
  /// 3. The tessellation shaders allow you to subdivide geometry based on
  /// certain rules to increase the mesh quality. This is often used to make
  /// surfaces like brick walls and staircases look less flat when they are
  /// nearby.
  ///
  /// 4. The geometry shader is run on every primitive (triangle, line, point)
  /// and can discard it or output more primitives than came in. This is similar
  /// to the tessellation shader, but much more flexible. However, it is not
  /// used much in today's applications because the performance is not that good
  /// on most graphics cards except for Intel's integrated GPUs.
  ///
  /// 5. The rasterization stage discretizes the primitives into fragments.
  /// These are the pixel elements that they fill on the framebuffer. Any
  /// fragments that fall outside the screen are discarded and the attributes
  /// outputted by the vertex shader are interpolated across the fragments, as
  /// shown in the figure. Usually the fragments that are behind other primitive
  /// fragments are also discarded here because of depth testing.
  ///
  /// 6. The fragment shader is invoked for every fragment that survives and
  /// determines which framebuffer(s) the fragments are written to and with
  /// which color and depth values. It can do this using the interpolated data
  /// from the vertex shader, which can include things like texture coordinates
  /// and normals for lighting.
  ///
  /// 7. The color blending stage applies operations to mix different fragments
  /// that map to the same pixel in the framebuffer. Fragments can simply
  /// overwrite each other, add up or be mixed based upon transparency.

  /// Creates a shader module from the shader bytecode (SPIR-V).
  ///
  /// @param code  Shader SPIR-V bytecode.
  ///
  /// @return VkShaderModule.
  VkShaderModule createShaderModule(const std::vector<char> &code);

  /// Creates a render pass.
  ///
  /// Each render pass instance defines a set of image resources, referred to as
  /// attachments, used during rendering (framebuffer attachments).
  ///
  /// Render passes operate in conjunction with framebuffers.
  void createRenderPass();

  /// Creates a graphics pipeline. In order to create a pipeline, function also
  /// creates the following:
  ///
  /// Shader stages: the shader modules that define the functionality of the
  /// programmable stages of the graphics pipeline.
  ///
  /// Fixed-function state: all of the structures that define the fixed-function
  /// stages of the pipeline, like input assembly, rasterizer, viewport and
  /// color blending.
  ///
  /// Pipeline layout: the uniform and push values referenced by the shader that
  /// can be updated at draw time.
  ///
  /// Render pass: the attachments referenced by the pipeline stages and their
  /// usage.
  void createGraphicsPipeline();

  /// Creates a framebuffer object.
  ///
  /// The attachments specified during render pass creation are bound by
  /// wrapping them into a VkFramebuffer object. Framebuffers represent a
  /// collection of specific memory attachments that a render pass instance
  /// uses.
  void createFramebuffers();

  /// Creates a command pool.
  ///
  /// Command pools are opaque objects that command buffer memory is allocated
  /// from, and which allow the implementation to amortize the cost of resource
  /// creation across multiple command buffers. Command pools are externally
  /// synchronized, meaning that a command pool must not be used concurrently in
  /// multiple threads. That includes use via recording commands on any command
  /// buffers allocated from the pool, as well as operations that allocate,
  /// free, and reset command buffers or the pool itself.
  void createCommandPool();

  /// Creates command buffers from the command pool.
  void createCommandBuffers();

  /// Writes the commands to be executed into the command buffer.
  ///
  /// @param commandBuffer  Command buffer.
  /// @param imageIndex  Image (framebuffer) index to use in the render pass.
  void recordCommandBuffer(VkCommandBuffer commandBuffer,
                           std::uint32_t imageIndex);

  ///
  /// @param typeFilter
  /// @param properties
  /// @return
  std::uint32_t findMemoryType(std::uint32_t typeFilter,
                               VkMemoryPropertyFlags properties);

  /// Creates a Vulkan buffer.
  ///
  /// @param size  Buffer size.
  /// @param usage  Buffer usage.
  /// @param properties  Memory properties.
  /// @param buffer  Created buffer.
  /// @param bufferMemory  Allocated memory.
  void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer &buffer,
                    VkDeviceMemory &bufferMemory);

  /// Copies data from one Vulkan buffer to another one.
  ///
  /// @param srcBuffer  Source buffer.
  /// @param dstBuffer  Destination buffer.
  /// @param size  Buffer size.
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

  /// Creates a vertex buffer.
  ///
  /// Uses staging buffer to upload the data from the vertex array.
  void createVertexBuffer();

  /// Creates a index buffer.
  ///
  /// Uses staging buffer to upload the data from the index array.
  void createIndexBuffer();

  void createUniformBuffers();

  /// Creates a description set layout.
  ///
  /// Specifies the types of resources that are going to be accessed by the
  /// pipeline.
  void createDescriptorSetLayout();

  void createDescriptorPool();

  /// Creates descriptor sets from the descriptor pool.
  ///
  /// Specifies the actual buffer or image resources that will be bound to the
  /// descriptors.
  void createDescriptorSets();

  void cleanupSwapChain();

  /// @}

  /// @name Drawing
  /// @{

  void recreateSwapChain() {
    // Handling minimization:
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window_, &width, &height);
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createFramebuffers();
  }

  /// Creates the synchronization objects.
  void createSyncObjects() {
    imageAvailableSemaphores_.resize(kMaxFramesInFlight);
    renderFinishedSemaphores_.resize(kMaxFramesInFlight);
    inFlightFences_.resize(kMaxFramesInFlight);
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::size_t i{0}; i < kMaxFramesInFlight; ++i) {
      if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                            &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
          vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                            &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
          vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) !=
              VK_SUCCESS) {
        throw std::runtime_error("failed to create semaphores!");
      }
    }
  }

  void drawFrame() {
    // Waiting for the previous frame:
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE,
                    UINT64_MAX);

    // Acquiring an image from the swap chain:
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device_, swapChain_, UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapChain();
      return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Only reset the fence if we are submitting work.
    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    // Recording the command buffer;
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    // Updating uniform data:
    updateUniformBuffer(currentFrame_);

    // Submitting the command buffer:
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo,
                      inFlightFences_[currentFrame_]) != VK_SUCCESS) {
      throw std::runtime_error("failed to submit draw command buffer!");
    }

    // Presentation.
    // Submitting the result back to the swap chain to have it eventually show
    // up on the screen.
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapChains[] = {swapChain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional
    result = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        framebufferResized_) {
      framebufferResized_ = false;
      recreateSwapChain();
    } else if (result != VK_SUCCESS) {
      throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
  }

  void updateUniformBuffer(std::uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     currentTime - startTime)
                     .count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        swapChainExtent_.width / (float)swapChainExtent_.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;
    memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
  }

  /// @}

  const std::vector<Vertex> vertices_{{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
  const std::vector<std::uint16_t> indices_{0, 1, 2, 2, 3, 0};

  std::uint32_t width_{1600U};
  std::uint32_t height_{1200U};
  GLFWwindow *window_{nullptr};

  const bool enableValidationLayers_{true};
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
  VkSwapchainKHR swapChain_{VK_NULL_HANDLE};

  std::vector<VkImage> swapChainImages_{};
  VkFormat swapChainImageFormat_{};
  VkExtent2D swapChainExtent_{};

  std::vector<VkImageView> swapChainImageViews_{};

  VkRenderPass renderPass_{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
  VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};

  VkPipeline graphicsPipeline_{VK_NULL_HANDLE};

  std::vector<VkFramebuffer> swapChainFramebuffers_{};

  VkCommandPool commandPool_{VK_NULL_HANDLE};

  VkBuffer vertexBuffer_{VK_NULL_HANDLE};
  VkDeviceMemory vertexBufferMemory_{VK_NULL_HANDLE};
  VkBuffer indexBuffer_{VK_NULL_HANDLE};
  VkDeviceMemory indexBufferMemory_{VK_NULL_HANDLE};

  std::uint32_t currentFrame_{0};
  std::vector<VkCommandBuffer> commandBuffers_{};
  std::vector<VkSemaphore> imageAvailableSemaphores_{};
  std::vector<VkSemaphore> renderFinishedSemaphores_{};
  std::vector<VkFence> inFlightFences_{};
  bool framebufferResized_{false};

  VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
  std::vector<VkDescriptorSet> descriptorSets_{};

  std::vector<VkBuffer> uniformBuffers_{};
  std::vector<VkDeviceMemory> uniformBuffersMemory_{};
  std::vector<void *> uniformBuffersMapped_{};
};

} // namespace vulkan_engine