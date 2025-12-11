#include "render/context.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace render {

void Context::Cleanup() {
  for (const auto &sampler : samplers_) {
    vkDestroySampler(device_, sampler, nullptr);
  }
  samplers_.clear();

  for (std::size_t i{0}; i < textureImages_.size(); ++i) {
    vmaDestroyImage(vmaAllocator_, textureImages_[i], textureImageMemories_[i]);
  }
  textureImages_.clear();
  textureImageMemories_.clear();

  for (std::size_t i{0}; i < uniformBuffers_.size(); ++i) {
    vmaDestroyBuffer(vmaAllocator_, uniformBuffers_[i],
                     uniformBufferMemories_[i]);
  }
  uniformBuffers_.clear();
  uniformBufferMemories_.clear();
  uniformBufferInfos_.clear();

  for (const auto &descriptorPool : descriptorPools_) {
    vkDestroyDescriptorPool(device_, descriptorPool, nullptr);
  }
  descriptorPools_.clear();

  for (const auto &descriptorSetLayout : descriptorSetLayouts_) {
    vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, nullptr);
  }
  descriptorSetLayouts_.clear();

  for (const auto &graphicsPipeline : pipelines_) {
    vkDestroyPipeline(device_, graphicsPipeline, nullptr);
  }
  pipelines_.clear();

  for (const auto &pipelineLayout : pipelineLayouts_) {
    vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
  }
  pipelineLayouts_.clear();

  for (const auto &shaderModule : shaderModules_) {
    vkDestroyShaderModule(device_, shaderModule, nullptr);
  }
  shaderModules_.clear();

  for (const auto &imageView : imageViews_) {
    vkDestroyImageView(device_, imageView, nullptr);
  }
  imageViews_.clear();

  for (std::size_t i{0}; i < kMaxFramesInFlight; ++i) {
    vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
    vkDestroyFence(device_, inFlightFences_[i], nullptr);
  }

  for (std::size_t i{0}; i < kMaxSwapchainImages; ++i) {
    vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
  }

  for (const auto &commandPool : commandPools_) {
    vkDestroyCommandPool(device_, commandPool, nullptr);
  }
  commandPools_.clear();

  for (std::size_t i{0}; i < vertexBuffers_.size(); ++i) {
    vmaDestroyBuffer(vmaAllocator_, vertexBuffers_[i],
                     vertexBufferMemories_[i]);
  }
  vertexBuffers_.clear();
  vertexBufferMemories_.clear();

  for (std::size_t i{0}; i < indexBuffers_.size(); ++i) {
    vmaDestroyBuffer(vmaAllocator_, indexBuffers_[i], indexBufferMemories_[i]);
  }
  indexBuffers_.clear();
  indexBufferMemories_.clear();

  vmaDestroyAllocator(vmaAllocator_);

  CleanupSwapchain();

  vkb::destroy_device(device_);
  vkb::destroy_surface(instance_, surface_);
  vkb::destroy_instance(instance_);

  glfwDestroyWindow(window_);
  glfwTerminate();
}

void Context::InitWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window_ = glfwCreateWindow(width_, height_, "Vulkan", nullptr, nullptr);
}

void Context::CreateSwapchain() {
  vkb::SwapchainBuilder swapchain_builder{device_};
  auto swapchain_ret = swapchain_builder.build();
  if (!swapchain_ret) {
    throw std::runtime_error("failed to create swapchain" +
                             swapchain_ret.error().message());
  }
  swapchain_ = swapchain_ret.value();

  auto image_ret = swapchain_.get_images();
  if (!image_ret) {
    throw std::runtime_error("failed to get swapchain images" + image_ret.error().message());
  }
  LOG(INFO) << "Swapchain created with " << image_ret->size() << " images";
  swapchainImages_ = std::move(*image_ret);

  auto image_view_ret = swapchain_.get_image_views();
  if (!image_view_ret) {
    throw std::runtime_error("failed to get swapchain image views" +
                             image_view_ret.error().message());
  }
  if (image_view_ret->size() > kMaxSwapchainImages) {
    throw std::runtime_error("swapchain image count exceeds maximum limit");
  }
  swapchainImageViews_ = std::move(*image_view_ret);
}

void Context::CreateSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  imageAvailableSemaphores_.resize(kMaxFramesInFlight);
  inFlightFences_.resize(kMaxFramesInFlight);
  for (std::size_t i{0}; i < kMaxFramesInFlight; ++i) {
    if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                          &imageAvailableSemaphores_[i]) != VK_SUCCESS ||

        vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) !=
            VK_SUCCESS) {
      throw std::runtime_error("failed to create semaphores or fences!");
    }
  }

  renderFinishedSemaphores_.resize(kMaxSwapchainImages);
  for (std::size_t i{0}; i < kMaxSwapchainImages; ++i) {
    if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr,
                          &renderFinishedSemaphores_[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create semaphores!");
    }
  }
}

void Context::Initialize(const ContextOptions &options) {
  // Init window:
  InitWindow();

  // Create instance:
  vkb::InstanceBuilder instance_builder{};
  instance_builder.require_api_version(VK_API_VERSION_1_3);
  instance_builder.use_default_debug_messenger();
  if (options.enableValidationLayers) {
    instance_builder.request_validation_layers();
  }
  // Retrieve extension to interface with the window system:
  std::uint32_t glfwExtensionCount{};
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  instance_builder.enable_extensions(glfwExtensionCount, glfwExtensions);
  auto instance_ret = instance_builder.build();
  if (!instance_ret) {
    throw std::runtime_error("failed to create instance:" +
                             instance_ret.error().message());
  }
  instance_ = *instance_ret;

  // Window surface creation:
  if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }

  // Pick physical device:
  vkb::PhysicalDeviceSelector selector{instance_, surface_};
  selector.add_required_extension("VK_KHR_swapchain");
  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = VK_TRUE;
  selector.set_required_features_13(features13);
  auto physical_device_ret = selector.select();
  if (!physical_device_ret) {
    throw std::runtime_error("failed to select physical device: " +
                             physical_device_ret.error().message());
  }
  physicalDevice_ = *physical_device_ret;

  // Create logical device:
  vkb::DeviceBuilder device_builder{physicalDevice_};
  auto device_ret = device_builder.build();
  if (!device_ret) {
    throw std::runtime_error("failed to create device: " +
                             device_ret.error().message());
  }
  device_ = *device_ret;
  dispatch_ = device_.make_table();

  // Retrieving queues:
  auto graphics_queue_ret = device_.get_queue(vkb::QueueType::graphics);
  if (!graphics_queue_ret) {
    throw std::runtime_error("failed to get graphics queue: " +
                             graphics_queue_ret.error().message());
  }
  graphicsQueue_ = *graphics_queue_ret;
  auto present_queue_ret = device_.get_queue(vkb::QueueType::present);
  if (!present_queue_ret) {
    throw std::runtime_error("failed to get present queue: " +
                             present_queue_ret.error().message());
  }
  presentQueue_ = *present_queue_ret;

  // Create default swapchain.
  CreateSwapchain();

  // Create sync objects.
  CreateSyncObjects();

  // Vulkan memory allocator creation:
  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorInfo.physicalDevice = physicalDevice_;
  allocatorInfo.device = device_;
  allocatorInfo.instance = instance_;
  if (vmaCreateAllocator(&allocatorInfo, &vmaAllocator_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create Vulkan memory allocator!");
  }
}

VkImageView Context::CreateImageView(const ImageViewOptions &options) {
  VkImageViewCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  createInfo.image = options.image;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.format = options.format;
  createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  createInfo.subresourceRange.baseMipLevel = 0;
  createInfo.subresourceRange.levelCount = 1;
  createInfo.subresourceRange.baseArrayLayer = 0;
  createInfo.subresourceRange.layerCount = 1;
  VkImageView imageView{VK_NULL_HANDLE};
  if (vkCreateImageView(device_, &createInfo, nullptr, &imageView) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create image view!");
  }

  imageViews_.push_back(imageView);
  return imageView;
}

VkShaderModule Context::CreateShaderModule(const ShaderModuleOptions &options) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pCode = static_cast<const std::uint32_t *>(options.data);
  createInfo.codeSize = options.size;
  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }

  shaderModules_.push_back(shaderModule);
  return shaderModule;
}

VkDescriptorSetLayout
Context::CreateDescriptorSetLayout(const DescriptorSetLayoutOptions &options) {
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
  for (const auto &bindingOption : options.bindingOptions) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = bindingOption.binding;
    layoutBinding.descriptorType = bindingOption.type;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = bindingOption.stageFlags;
    layoutBinding.pImmutableSamplers = nullptr; // Optional
    layoutBindings.push_back(layoutBinding);
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<std::uint32_t>(layoutBindings.size());
  layoutInfo.pBindings = layoutBindings.data();
  VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
  if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr,
                                  &descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }

  descriptorSetLayouts_.push_back(descriptorSetLayout);
  return descriptorSetLayout;
}

VkPipelineLayout
Context::CreatePipelineLayout(const PipelineLayoutOptions &options) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &options.descriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;    // Optional
  pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr,
                             &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  pipelineLayouts_.push_back(pipelineLayout);
  return pipelineLayout;
}

VkPipeline
Context::CreateGraphicsPipeline(const GraphicsPipelineOptions &options) {
  // Shader stage creation:
  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = options.vertexShader;
  vertShaderStageInfo.pName = "main";
  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = options.fragmentShader;
  fragShaderStageInfo.pName = "main";
  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // Dynamic state.
  std::vector<VkDynamicState> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_FRONT_FACE,
      VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount =
      static_cast<std::uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  // Vertex input.
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<std::uint32_t>(options.vertexInputAttribues.size());
  vertexInputInfo.pVertexBindingDescriptions = &options.vertexInputBinding;
  vertexInputInfo.pVertexAttributeDescriptions = options.vertexInputAttribues.data();

  // Input assembly.
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // Viewports and scissors.
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  // Rasterizer.
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f; // Optional
  rasterizer.depthBiasClamp = 0.0f;          // Optional
  rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

  // Multisampling:
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading = 1.0f;          // Optional
  multisampling.pSampleMask = nullptr;            // Optional
  multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
  multisampling.alphaToOneEnable = VK_FALSE;      // Optional

  // Color blending:
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f; // Optional
  colorBlending.blendConstants[1] = 0.0f; // Optional
  colorBlending.blendConstants[2] = 0.0f; // Optional
  colorBlending.blendConstants[3] = 0.0f; // Optional

  // Pipeline rendering info (for dynamic rendering).
  VkPipelineRenderingCreateInfo pipeline_rendering_info{};
  pipeline_rendering_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  pipeline_rendering_info.colorAttachmentCount = 1;
  pipeline_rendering_info.pColorAttachmentFormats = &swapchain_.image_format;

  // Graphics pipeline:
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &pipeline_rendering_info;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = options.pipelineLayout;
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not used with dynamic rendering
  VkPipeline pipeline{VK_NULL_HANDLE};
  if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &pipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  pipelines_.push_back(pipeline);
  return pipeline;
}

VkCommandPool Context::CreateCommandPool(const CommandPoolOptions &options) {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex =
      *device_.get_queue_index(vkb::QueueType::graphics);
  VkCommandPool commandPool{VK_NULL_HANDLE};
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create command pool!");
  }
  commandPools_.push_back(commandPool);
  return commandPool;
}

VkCommandBuffer
Context::CreateCommandBuffer(const CommandBufferOptions &options) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = options.commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
  if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }

  commandBuffers_.push_back(commandBuffer);
  return commandBuffer;
}

VkCommandBuffer Context::BeginSingleTimeCommands(VkCommandPool commandPool) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void Context::EndSingleTimeCommands(VkCommandPool commandPool,
                                    VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue_);

  vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
}

void Context::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaAllocationCreateFlags flags, VkBuffer &buffer,
                           VmaAllocation &bufferMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.flags = flags;
  if (vmaCreateBuffer(vmaAllocator_, &bufferInfo, &allocInfo, &buffer,
                      &bufferMemory, nullptr) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer memory!");
  }
}

void Context::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaAllocationCreateFlags flags, VkBuffer &buffer,
                           VmaAllocation &bufferMemory,
                           VmaAllocationInfo &allocationInfo) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.flags = flags;
  if (vmaCreateBuffer(vmaAllocator_, &bufferInfo, &allocInfo, &buffer,
                      &bufferMemory, &allocationInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer memory!");
  }
}

void Context::CopyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer,
                         VkBuffer dstBuffer, VkDeviceSize size) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  EndSingleTimeCommands(commandPool, commandBuffer);
}

VkBuffer Context::CreateVertexBuffer(const VertexBufferOptions &options) {
  // Create a temporary staging buffer:
  VkBuffer stagingBuffer{};
  VmaAllocation stagingBufferMemory{};
  CreateBuffer(options.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
               stagingBuffer, stagingBufferMemory);

  // Filling the staging buffer:
  if (vmaCopyMemoryToAllocation(vmaAllocator_, options.bufferData,
                                stagingBufferMemory, 0,
                                options.bufferSize) != VK_SUCCESS) {
    throw std::runtime_error("failed to copy data to vertex buffer memory!");
  }

  // Create the vertex buffer:
  VkBuffer vertexBuffer{};
  VmaAllocation vertexBufferMemory{};
  CreateBuffer(options.bufferSize,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               0U, vertexBuffer, vertexBufferMemory);
  CopyBuffer(options.commandPool, stagingBuffer, vertexBuffer,
             options.bufferSize);

  vmaDestroyBuffer(vmaAllocator_, stagingBuffer, stagingBufferMemory);

  vertexBuffers_.push_back(vertexBuffer);
  vertexBufferMemories_.push_back(vertexBufferMemory);
  return vertexBuffer;
}

VkBuffer Context::CreateIndexBuffer(const IndexBufferOptions &options) {
  // Create a temporary staging buffer:
  VkBuffer stagingBuffer{};
  VmaAllocation stagingBufferMemory{};
  CreateBuffer(options.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
               stagingBuffer, stagingBufferMemory);

  // Filling the staging buffer:
  if (vmaCopyMemoryToAllocation(vmaAllocator_, options.bufferData,
                                stagingBufferMemory, 0,
                                options.bufferSize) != VK_SUCCESS) {
    throw std::runtime_error("failed to copy data to vertex buffer memory!");
  }

  // Create the index buffer:
  VkBuffer indexBuffer{};
  VmaAllocation indexBufferMemory{};
  CreateBuffer(options.bufferSize,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               0U, indexBuffer, indexBufferMemory);
  CopyBuffer(options.commandPool, stagingBuffer, indexBuffer,
             options.bufferSize);

  vmaDestroyBuffer(vmaAllocator_, stagingBuffer, stagingBufferMemory);

  indexBuffers_.push_back(indexBuffer);
  indexBufferMemories_.push_back(indexBufferMemory);
  return indexBuffer;
}

VkBuffer Context::CreateUniformBuffer() {
  VkDeviceSize bufferSize{sizeof(UniformBufferObject)};
  VkBuffer uniformBuffer{};
  VmaAllocation uniformBufferMemory{};
  VmaAllocationInfo uniformBufferMemoryInfo{};
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT,
               uniformBuffer, uniformBufferMemory, uniformBufferMemoryInfo);

  uniformBuffers_.push_back(uniformBuffer);
  uniformBufferMemories_.push_back(uniformBufferMemory);
  uniformBufferInfos_.push_back(uniformBufferMemoryInfo);
  return uniformBuffer;
}

VkDescriptorPool
Context::CreateDescriptorPool(const DescriptorPoolOptions &options) {
  std::vector<VkDescriptorPoolSize> poolSizes{};
  for (const auto &poolSizeOption : options.poolSizeOptions) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = poolSizeOption.type;
    poolSize.descriptorCount = poolSizeOption.descriptorCount;
    poolSizes.push_back(poolSize);
  }

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = options.maxSets;
  VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
  if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  descriptorPools_.push_back(descriptorPool);
  return descriptorPool;
}

VkDescriptorSet
Context::CreateDescriptorSet(const DescriptorSetOptions &options) {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = options.descriptorPool;
  allocInfo.descriptorSetCount = 1U;
  allocInfo.pSetLayouts = &options.descriptorSetLayout;
  VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
  if (vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  descriptorSets_.push_back(descriptorSet);
  return descriptorSet;
}

void Context::UpdateDescriptorSet(const UpdateDescriptorSetOptions &options) {
  std::vector<VkWriteDescriptorSet> descriptorWrites{};

  std::vector<VkDescriptorBufferInfo> bufferInfos{};
  for (const auto &descriptorUniformBuffer : options.descriptorUniformBuffers) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = descriptorUniformBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);
    bufferInfos.push_back(bufferInfo);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = options.descriptorSet;
    descriptorWrite.dstBinding = descriptorUniformBuffer.binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &*bufferInfos.rbegin();
    descriptorWrites.push_back(descriptorWrite);
  }

  std::vector<VkDescriptorImageInfo> imageInfos{};
  for (const auto &descriptorImage : options.descriptorImages) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = descriptorImage.imageView;
    imageInfo.sampler = descriptorImage.sampler;
    imageInfos.push_back(imageInfo);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = options.descriptorSet;
    descriptorWrite.dstBinding = descriptorImage.binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &*imageInfos.rbegin();
    descriptorWrites.push_back(descriptorWrite);
  }

  vkUpdateDescriptorSets(device_,
                         static_cast<std::uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(), 0, nullptr);
}

void Context::RecordCommandBuffer(const RecordCommandBufferOptions &options) {
  // Reset the command buffer first.
  vkResetCommandBuffer(options.commandBuffer, 0);

  // Start recording the command buffer.
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;                  // Optional
  beginInfo.pInheritanceInfo = nullptr; // Optional
  if (vkBeginCommandBuffer(options.commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  // Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL.
  TransitionImageLayout(
      options.commandBuffer, swapchainImages_[currentSwapchainImageIndex_],
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

  // Starting a render pass:
  VkRenderingAttachmentInfo colorAttachment{};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.imageView = swapchainImageViews_[currentSwapchainImageIndex_];
  colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.clearValue = options.clearColor;
  VkRenderingInfo renderInfo{};
  renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderInfo.colorAttachmentCount = 1;
  renderInfo.pColorAttachments = &colorAttachment;
  renderInfo.layerCount = 1;
  renderInfo.renderArea.offset = {0, 0};
  renderInfo.renderArea.extent = swapchain_.extent;
  vkCmdBeginRendering(options.commandBuffer, &renderInfo);

  // Dynamic pipeline states:
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swapchain_.extent.width);
  viewport.height = static_cast<float>(swapchain_.extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(options.commandBuffer, 0, 1, &viewport);
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain_.extent;
  vkCmdSetScissor(options.commandBuffer, 0, 1, &scissor);
  vkCmdSetCullMode(options.commandBuffer, VK_CULL_MODE_BACK_BIT);
  vkCmdSetFrontFace(options.commandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
  vkCmdSetPrimitiveTopology(options.commandBuffer, options.topology);

  // Binding the vertex buffer:
  vkCmdBindPipeline(options.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    options.pipeline);
  VkBuffer vertexBuffers[] = {options.vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(options.commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(options.commandBuffer, options.indexBuffer, 0,
                       VK_INDEX_TYPE_UINT16);
  vkCmdBindDescriptorSets(
      options.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      options.pipelineLayout, 0, 1, &options.descriptorSet, 0, nullptr);
  vkCmdDrawIndexed(options.commandBuffer, options.indexCount, 1, 0, 0, 0);

  // Ending the render pass:
  vkCmdEndRendering(options.commandBuffer);

  // After rendering , transition the swapchain image to PRESENT_SRC
  TransitionImageLayout(
      options.commandBuffer, swapchainImages_[currentSwapchainImageIndex_],
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  if (vkEndCommandBuffer(options.commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

void Context::UpdateUniformBuffer(const UpdateUniformBufferOptions &options) {
  memcpy(uniformBufferInfos_[options.uniformBufferIndex].pMappedData,
         &options.data, sizeof(options.data));
}

BeginFrameInfo Context::BeginFrame(const BeginFrameOptions &options) {
  BeginFrameInfo info{false};

  // Waiting for the previous frame:
  vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE,
                  UINT64_MAX);

  // Acquiring an image from the swap chain:
  VkResult result = vkAcquireNextImageKHR(
      device_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame_],
      VK_NULL_HANDLE, &currentSwapchainImageIndex_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    LOG(INFO) << "BeginFrame(): Swapchain is out of date, recreating swapchain";
    RecreateSwapchain();
    result =
        vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                              imageAvailableSemaphores_[currentFrame_],
                              VK_NULL_HANDLE, &currentSwapchainImageIndex_);
    info.ifSwapchainRecreated = true;
  }

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    LOG(INFO) << "BeginFrame(): Recreating swapchain again";
    RecreateSwapchain();
    result =
        vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                              imageAvailableSemaphores_[currentFrame_],
                              VK_NULL_HANDLE, &currentSwapchainImageIndex_);
    info.ifSwapchainRecreated = true;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Only reset the fence if we are submitting work.
  vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

  return info;
}

EndFrameInfo Context::EndFrame(const EndFrameOptions &options) {
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
  submitInfo.pCommandBuffers = &options.commandBuffer;
  VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentSwapchainImageIndex_]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;
  if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo,
                    inFlightFences_[currentFrame_]) != VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  // Presentation.
  // Submitting the result back to the swap chain to have it eventually show
  // up on the screen.
  VkSwapchainKHR swapchains[] = {swapchain_};
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &currentSwapchainImageIndex_;
  presentInfo.pResults = nullptr; // Optional
  VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    LOG(INFO) << "EndFrame(): Swapchain is out of date, recreating swapchain";
    RecreateSwapchain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
  return {};
}

VkImage Context::CreateTextureImage(const TextureImageOptions &options) {
  if (!options.imageData) {
    throw std::runtime_error("no image!");
  }

  int texWidth{options.imageData->Width()};
  int texHeight{options.imageData->Height()};
  int texComponents{options.imageData->Components()};
  VkDeviceSize imageSize = texWidth * texHeight * texComponents;

  // Create a temporary staging buffer and copy image data.
  VkBuffer stagingBuffer{};
  VmaAllocation stagingBufferMemory{};
  CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
               stagingBuffer, stagingBufferMemory);

  if (vmaCopyMemoryToAllocation(vmaAllocator_, options.imageData->Data(),
                                stagingBufferMemory, 0,
                                imageSize) != VK_SUCCESS) {
    throw std::runtime_error("failed to copy data to texture image memory!");
  }

  VkFormat format{};
  switch (texComponents) {
  case 1: {
    format = VK_FORMAT_R8_SRGB;
    break;
  }
  case 3: {
    format = VK_FORMAT_R8G8B8_SRGB;
    break;
  }
  case 4: {
    format = VK_FORMAT_R8G8B8A8_SRGB;
    break;
  }
  default: {
    throw std::runtime_error("unsupported image format!");
    break;
  }
  }

  // Create an image.
  VkImage textureImage{};
  VmaAllocation textureImageMemory{};
  CreateImage(texWidth, texHeight, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              textureImage, textureImageMemory);

  // Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
  TransitionImageLayout(options.commandPool, textureImage,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  // Execute the buffer to image copy operation.
  CopyBufferToImage(options.commandPool, stagingBuffer, textureImage,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  // Transition to prepare the texture image for shader access.
  TransitionImageLayout(options.commandPool, textureImage,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vmaDestroyBuffer(vmaAllocator_, stagingBuffer, stagingBufferMemory);

  textureImages_.push_back(textureImage);
  textureImageMemories_.push_back(textureImageMemory);
  return textureImage;
}

VkSampler Context::CreateTextureSampler(const TextureSamplerOptions &options) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = options.magFilter;
  samplerInfo.minFilter = options.minFilter;
  samplerInfo.addressModeU = options.addressMode;
  samplerInfo.addressModeV = options.addressMode;
  samplerInfo.addressModeW = options.addressMode;
  if (options.anisotropyEnable) {
    samplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    LOG(INFO) << "Anisotropy is enabled for the sampler, maxAnisotropy="
              << properties.limits.maxSamplerAnisotropy;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  } else {
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0F;
  }
  samplerInfo.borderColor = options.borderColor;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;
  VkSampler textureSampler;
  if (vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }

  samplers_.push_back(textureSampler);
  return textureSampler;
}

void Context::CleanupSwapchain() {
  swapchain_.destroy_image_views(swapchainImageViews_);
  vkb::destroy_swapchain(swapchain_);
  swapchainImages_.clear();
  swapchainImageViews_.clear();
  currentSwapchainImageIndex_ = 0U;
}

void Context::RecreateSwapchain() {
  // Handling minimization:
  int width = 0, height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(device_);

  CleanupSwapchain();
  CreateSwapchain();
}

void Context::CreateImage(std::uint32_t width, std::uint32_t height,
                          VkFormat format, VkImageTiling tiling,
                          VkImageUsageFlags usage, VkImage &image,
                          VmaAllocation &imageMemory) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  if (vmaCreateImage(vmaAllocator_, &imageInfo, &allocInfo, &image,
                     &imageMemory, nullptr) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image memory!");
  }
}

void Context::TransitionImageLayout(VkCommandBuffer commandBuffer,
                                    VkImage image, VkImageLayout oldLayout,
                                    VkImageLayout newLayout,
                                    VkAccessFlags srcAccessMask,
                                    VkAccessFlags dstAccessMask,
                                    VkPipelineStageFlags srcStage,
                                    VkPipelineStageFlags dstStage) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = srcAccessMask;
  barrier.dstAccessMask = dstAccessMask;
  vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

void Context::TransitionImageLayout(VkCommandBuffer commandBuffer,
                                    VkImage image, VkImageLayout oldLayout,
                                    VkImageLayout newLayout) {
  VkAccessFlags srcAccessMask{};
  VkAccessFlags dstAccessMask{};
  VkPipelineStageFlags sourceStage{};
  VkPipelineStageFlags destinationStage{};
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    // Undefined -> transfer destination: transfer writes that don't need to
    // wait on anything.
    srcAccessMask = 0;
    dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    // Transfer destination -> shader reading: shader reads should wait on
    // transfer writes, specifically the shader reads in the fragment shader,
    // because that's where we're going to use the texture.
    srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  TransitionImageLayout(commandBuffer, image, oldLayout, newLayout,
                        srcAccessMask, dstAccessMask, sourceStage,
                        destinationStage);
}

void Context::TransitionImageLayout(VkCommandPool commandPool, VkImage image,
                                    VkImageLayout oldLayout,
                                    VkImageLayout newLayout) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);
  TransitionImageLayout(commandBuffer, image, oldLayout, newLayout);
  EndSingleTimeCommands(commandPool, commandBuffer);
}

void Context::CopyBufferToImage(VkCommandPool commandPool, VkBuffer buffer,
                                VkImage image, uint32_t width,
                                uint32_t height) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};
  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  EndSingleTimeCommands(commandPool, commandBuffer);
}

} // namespace render