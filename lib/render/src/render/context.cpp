#include "render/context.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace render {

ImageData::ImageData() = default;

ImageData::ImageData(const std::string path) {
  data_ =
      stbi_load(path.c_str(), &width_, &height_, &components_, STBI_rgb_alpha);
  components_ = STBI_rgb_alpha;
}

ImageData::ImageData(const unsigned char *data, const std::size_t size) {
  data_ = stbi_load_from_memory(data, size, &width_, &height_, &components_,
                                STBI_rgb_alpha);
  components_ = STBI_rgb_alpha;
}

ImageData::ImageData(ImageData &&other)
    : width_{other.width_}, height_{other.height_},
      components_{other.components_}, data_{other.data_} {
  other.data_ = nullptr;
}

ImageData::~ImageData() {
  if (data_ != nullptr) {
    stbi_image_free(data_);
  }
}

void Context::Cleanup() {
  CleanupSwapchain();

  for (const auto &sampler : samplers_) {
    vkDestroySampler(device_, sampler, nullptr);
  }
  samplers_.clear();

  for (const auto &textureImage : textureImages_) {
    vkDestroyImage(device_, textureImage, nullptr);
  }
  textureImages_.clear();
  for (const auto &textureImageMemory : textureImageMemories_) {
    vkFreeMemory(device_, textureImageMemory, nullptr);
  }
  textureImageMemories_.clear();

  for (const auto &uniformBuffer : uniformBuffers_) {
    vkDestroyBuffer(device_, uniformBuffer, nullptr);
  }
  uniformBuffers_.clear();
  for (const auto &uniformBufferMemory : uniformBufferMemories_) {
    vkFreeMemory(device_, uniformBufferMemory, nullptr);
  }
  uniformBufferMemories_.clear();
  uniformBufferMappedMemories_.clear();

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

  for (const auto &framebuffer : framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  framebuffers_.clear();

  for (const auto &renderPass : renderPasses_) {
    vkDestroyRenderPass(device_, renderPass, nullptr);
  }
  renderPasses_.clear();

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
    vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
    vkDestroyFence(device_, inFlightFences_[i], nullptr);
  }

  for (const auto &commandPool : commandPools_) {
    vkDestroyCommandPool(device_, commandPool, nullptr);
  }
  commandPools_.clear();

  for (const auto &buffer : vertexBuffers_) {
    vkDestroyBuffer(device_, buffer, nullptr);
  }
  vertexBuffers_.clear();
  for (const auto &memory : vertexBufferMemories_) {
    vkFreeMemory(device_, memory, nullptr);
  }
  vertexBufferMemories_.clear();

  for (const auto &buffer : indexBuffers_) {
    vkDestroyBuffer(device_, buffer, nullptr);
  }
  indexBuffers_.clear();
  for (const auto &memory : indexBufferMemories_) {
    vkFreeMemory(device_, memory, nullptr);
  }
  indexBufferMemories_.clear();

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

  // Handling resizes explicitly:
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
}

void Context::CreateSwapchain() {
  vkb::SwapchainBuilder swapchain_builder{device_};
  auto swapchain_ret = swapchain_builder.build();
  if (!swapchain_ret) {
    throw std::runtime_error("failed to create swapchain" +
                             swapchain_ret.error().message());
  }
  swapchain_ = swapchain_ret.value();

  auto image_view_ret = swapchain_.get_image_views();
  if (!image_view_ret) {
    throw std::runtime_error("failed to get swapchain image views" +
                             image_view_ret.error().message());
  }
  swapchainImageViews_ = *image_view_ret;
}

void Context::CreateSyncObjects() {
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

void Context::Initialize(const ContextOptions &options) {
  // Init window:
  InitWindow();

  // Create instance:
  vkb::InstanceBuilder instance_builder{};
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
}

void Context::CreateSwapchainFramebuffers(VkRenderPass renderPass) {
  for (const auto &imageView : swapchainImageViews_) {
    FrameBufferOptions options{};
    options.renderPass = renderPass;
    options.extent = swapchain_.extent;
    options.imageAttachment = imageView;
    const auto framebuffer = CreateFramebuffer(options);
    framebuffers_.pop_back();
    swapchainFramebuffers_.push_back(framebuffer);
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

VkShaderModule Context::CreateShaderModule(const std::vector<char> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const std::uint32_t *>(code.data());
  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }

  shaderModules_.push_back(shaderModule);
  return shaderModule;
}

VkRenderPass Context::CreateRenderPass(const RenderPassOptions &options) {
  // Attachment description:
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = options.format;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  // Attachment references.
  // Specifies which attachment to reference by its index in the attachment
  // descriptions array.
  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Subpasses.
  // Subsequent rendering operations that depend on the contents of
  // framebuffers in previous passes.
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  // Subpass dependencies.
  // The subpasses in a render pass automatically take care of image layout
  // transitions. These transitions are controlled by subpass dependencies,
  // which specify memory and execution dependencies between subpasses. The
  // operations right before and right after this subpass also count as
  // implicit "subpasses".
  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  // Render pass.
  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  VkRenderPass renderPass{VK_NULL_HANDLE};
  if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }

  renderPasses_.push_back(renderPass);
  return renderPass;
}

VkFramebuffer Context::CreateFramebuffer(const FrameBufferOptions &options) {
  VkImageView attachments[] = {options.imageAttachment};
  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  // You can only use a framebuffer with the render passes that it is
  // compatible with, which roughly means that they use the same number and
  // type of attachments.
  framebufferInfo.renderPass = options.renderPass;
  framebufferInfo.attachmentCount = 1;
  framebufferInfo.pAttachments = attachments;
  framebufferInfo.width = options.extent.width;
  framebufferInfo.height = options.extent.height;
  // Our swap chain images are single images, so the number of layers is 1.
  framebufferInfo.layers = 1;

  VkFramebuffer framebuffer{VK_NULL_HANDLE};
  if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create framebuffer!");
  }

  framebuffers_.push_back(framebuffer);
  return framebuffer;
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
  // Specify states that can actually be changed without recreating the
  // pipeline at draw time.
  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount =
      static_cast<std::uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  // Vertex input.
  // Describe the format of the vertex data that will be passed to the vertex
  // shader.
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<std::uint32_t>(options.vertexInputAttribues.size());
  vertexInputInfo.pVertexBindingDescriptions = &options.vertexInputBinding;
  vertexInputInfo.pVertexAttributeDescriptions = options.vertexInputAttribues.data();

  // Input assembly.
  // Describe what kind of geometry will be drawn from the vertices and if
  // primitive restart should be enabled.
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = options.topology;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // Viewports and scissors.
  // A viewport basically describes the region of the framebuffer that the
  // output will be rendered to.
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(options.viewportExtent.width);
  viewport.height = static_cast<float>(options.viewportExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  // While viewports define the transformation from the image to the
  // framebuffer, scissor rectangles define in which regions pixels will
  // actually be stored. Any pixels outside the scissor rectangles will be
  // discarded by the rasterizer. They function like a filter rather than a
  // transformation.
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain_.extent;
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  // Rasterizer.
  // The rasterizer takes the geometry that is shaped by the vertices from the
  // vertex shader and turns it into fragments to be colored by the fragment
  // shader. It also performs depth testing, face culling and the scissor
  // test, and it can be configured to output fragments that fill entire
  // polygons or just the edges (wireframe rendering).
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = options.polygonMode;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

  // Graphics pipeline:
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = nullptr; // Optional
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = options.pipelineLayout;
  pipelineInfo.renderPass = options.renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
  pipelineInfo.basePipelineIndex = -1;              // Optional
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

std::uint32_t Context::FindMemoryType(std::uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
  DLOG(INFO) << "Vertex Buffer: Find memory type: Memory type count is "
             << memProperties.memoryTypeCount;
  for (std::uint32_t i{0}; i < memProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      DLOG(INFO) << "Vertex Buffer: Find memory type: Memory type " << i
                 << " has been chosen";
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
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
                           VkMemoryPropertyFlags properties, VkBuffer &buffer,
                           VkDeviceMemory &bufferMemory) {
  // Buffer creation:
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  // Memory requirements:
  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

  // Memory allocation:
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(memRequirements.memoryTypeBits, properties);
  if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate buffer memory!");
  }
  vkBindBufferMemory(device_, buffer, bufferMemory, 0);
}

void Context::CopyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer,
                         VkBuffer dstBuffer, VkDeviceSize size) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0; // Optional
  copyRegion.dstOffset = 0; // Optional
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  EndSingleTimeCommands(commandPool, commandBuffer);
}

VkBuffer Context::CreateVertexBuffer(const VertexBufferOptions &options) {
  VkBuffer stagingBuffer{VK_NULL_HANDLE};
  VkDeviceMemory stagingBufferMemory{VK_NULL_HANDLE};
  CreateBuffer(options.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);
  // Filling the vertex buffer:
  void *data;
  vkMapMemory(device_, stagingBufferMemory, 0, options.bufferSize, 0, &data);
  std::memcpy(data, options.bufferData,
              static_cast<size_t>(options.bufferSize));
  vkUnmapMemory(device_, stagingBufferMemory);

  VkBuffer vertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory vertexBufferMemory{VK_NULL_HANDLE};
  CreateBuffer(
      options.bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
  CopyBuffer(options.commandPool, stagingBuffer, vertexBuffer,
             options.bufferSize);

  vkDestroyBuffer(device_, stagingBuffer, nullptr);
  vkFreeMemory(device_, stagingBufferMemory, nullptr);

  vertexBuffers_.push_back(vertexBuffer);
  vertexBufferMemories_.push_back(vertexBufferMemory);
  return vertexBuffer;
}

VkBuffer Context::CreateIndexBuffer(const IndexBufferOptions &options) {
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(options.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void *data;
  vkMapMemory(device_, stagingBufferMemory, 0, options.bufferSize, 0, &data);
  std::memcpy(data, options.bufferData,
              static_cast<size_t>(options.bufferSize));
  vkUnmapMemory(device_, stagingBufferMemory);

  VkBuffer indexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory indexBufferMemory{VK_NULL_HANDLE};
  CreateBuffer(
      options.bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
  CopyBuffer(options.commandPool, stagingBuffer, indexBuffer,
             options.bufferSize);

  vkDestroyBuffer(device_, stagingBuffer, nullptr);
  vkFreeMemory(device_, stagingBufferMemory, nullptr);

  indexBuffers_.push_back(indexBuffer);
  indexBufferMemories_.push_back(indexBufferMemory);
  return indexBuffer;
}

VkBuffer Context::CreateUniformBuffer() {
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);
  VkBuffer uniformBuffer{VK_NULL_HANDLE};
  VkDeviceMemory uniformBufferMemory{VK_NULL_HANDLE};
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               uniformBuffer, uniformBufferMemory);

  void *mapped_memory{nullptr};
  vkMapMemory(device_, uniformBufferMemory, 0, bufferSize, 0, &mapped_memory);

  uniformBuffers_.push_back(uniformBuffer);
  uniformBufferMemories_.push_back(uniformBufferMemory);
  uniformBufferMappedMemories_.push_back(mapped_memory);
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

  // Starting a render pass:
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = options.renderPass;
  renderPassInfo.framebuffer =
      swapchainFramebuffers_[currentSwapchainImageIndex_];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapchain_.extent;
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &options.clearColor;
  vkCmdBeginRenderPass(options.commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Basic drawing commands:
  vkCmdBindPipeline(options.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    options.pipeline);
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

  vkCmdEndRenderPass(options.commandBuffer);
  if (vkEndCommandBuffer(options.commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

void Context::UpdateUniformBuffer(const UpdateUniformBufferOptions &options) {
  memcpy(uniformBufferMappedMemories_[options.uniformBufferIndex],
         &options.data, sizeof(options.data));
}

BeginFrameInfo Context::BeginFrame(const BeginFrameOptions &options) {
  // Waiting for the previous frame:
  vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE,
                  UINT64_MAX);

  // Acquiring an image from the swap chain:
  VkResult result = vkAcquireNextImageKHR(
      device_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame_],
      VK_NULL_HANDLE, &currentSwapchainImageIndex_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapchain(options.renderPass);
    return BeginFrameInfo{true};
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Only reset the fence if we are submitting work.
  vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
  return BeginFrameInfo{false};
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
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      framebufferResized_) {
    framebufferResized_ = false;
    RecreateSwapchain(options.renderPass);
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

  // Create staging buffer and copy image data.
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);
  void *data;
  vkMapMemory(device_, stagingBufferMemory, 0, imageSize, 0, &data);
  memcpy(data, options.imageData->Data(), static_cast<size_t>(imageSize));
  vkUnmapMemory(device_, stagingBufferMemory);

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
  VkImage textureImage;
  VkDeviceMemory textureImageMemory;
  CreateImage(texWidth, texHeight, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage,
              textureImageMemory);

  // Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
  TransitionImageLayout(options.commandPool, textureImage, format,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  // Execute the buffer to image copy operation.
  CopyBufferToImage(options.commandPool, stagingBuffer, textureImage,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  // Transition to prepare the texture image for shader access.
  TransitionImageLayout(options.commandPool, textureImage, format,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(device_, stagingBuffer, nullptr);
  vkFreeMemory(device_, stagingBufferMemory, nullptr);

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
  for (const auto &framebuffer : swapchainFramebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  swapchainFramebuffers_.clear();

  swapchain_.destroy_image_views(swapchainImageViews_);
  vkb::destroy_swapchain(swapchain_);
}

void Context::RecreateSwapchain(VkRenderPass renderPass) {
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
  CreateSwapchainFramebuffers(renderPass);
}

void Context::CreateImage(std::uint32_t width, std::uint32_t height,
                          VkFormat format, VkImageTiling tiling,
                          VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkImage &image,
                          VkDeviceMemory &imageMemory) {
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
  imageInfo.flags = 0; // Optional
  if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image!");
  }

  // Allocate device memory for an image.
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device_, image, &memRequirements);
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(memRequirements.memoryTypeBits, properties);
  if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory!");
  }
  vkBindImageMemory(device_, image, imageMemory, 0);
}

void Context::TransitionImageLayout(VkCommandPool commandPool, VkImage image,
                                    VkFormat format, VkImageLayout oldLayout,
                                    VkImageLayout newLayout) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

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

  // Transition barrier masks.
  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    // Undefined -> transfer destination: transfer writes that don't need to
    // wait on anything.
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    // Transfer destination -> shader reading: shader reads should wait on
    // transfer writes, specifically the shader reads in the fragment shader,
    // because that's where we're going to use the texture.
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

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