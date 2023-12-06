#include "render/context.hpp"

namespace render {

void Context::Cleanup() {
  CleanupSwapChain();

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

  vkDestroyDevice(device_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkDestroyInstance(instance_, nullptr);

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

void Context::Initialize(const ContextOptions &options) {
  // 0) Init window
  InitWindow();

  // 0) Check validation layers:
  if (options.enableValidationLayers && !CheckValidationLayerSupport()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  // 1) Create instance:
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = options.title.c_str();
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  // Retrieve extension to interface with the window system:
  std::uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  createInfo.enabledExtensionCount = glfwExtensionCount;
  createInfo.ppEnabledExtensionNames = glfwExtensions;
  // Set validation layers:
  if (options.enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<std::uint32_t>(validationLayers_.size());
    createInfo.ppEnabledLayerNames = validationLayers_.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }
  if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create instance!");
  }

  // 2) Window surface creation:
  if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }

  // 3) Pick physical device:
  // List the graphics cards:
  std::uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
  // Pick the suitable one:
  for (const auto &device : devices) {
    if (IsDeviceSuitable(device)) {
      physicalDevice_ = device;
      break;
    }
  }
  if (physicalDevice_ == VK_NULL_HANDLE) {
    throw std::runtime_error("failed to find a suitable GPU!");
  }

  // 4) Create logical device:
  // 4.1) Specifying the queues to be created:
  QueueFamilyIndices indices = FindQueueFamilies(physicalDevice_);
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<std::uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                                 indices.presentFamily.value()};
  float queuePriority = 1.0f;
  for (std::uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }
  // 4.2) Set up logical device:
  // Specifying used device features:
  VkPhysicalDeviceFeatures deviceFeatures{};
  // Creating the logical device:
  VkDeviceCreateInfo deviceCreateInfo{};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
  deviceCreateInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions_.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions_.data();
  if (options.enableValidationLayers) {
    deviceCreateInfo.enabledLayerCount =
        static_cast<std::uint32_t>(validationLayers_.size());
    deviceCreateInfo.ppEnabledLayerNames = validationLayers_.data();
  } else {
    deviceCreateInfo.enabledLayerCount = 0;
  }
  if (vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  // 5) Retrieving queue handles:
  vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);

  // 6) Create default swapchain.
  CreateSwapChain();

  // 7) Create sync objects.
  CreateSyncObjects();
}

bool Context::CheckValidationLayerSupport() {
  // List all of the available layers:
  std::uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
  // Next, check if all of the requested layers exist in the available layers.
  for (const char *layerName : validationLayers_) {
    bool layerFound = false;
    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }
    if (!layerFound) {
      return false;
    }
  }
  return true;
}

bool Context::IsDeviceSuitable(VkPhysicalDevice device) {
  // Queries device properties/details:
  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);
  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
  // Checking for swap chain support:
  const bool extensionsSupported = CheckDeviceExtensionSupport(device);
  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }
  if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
      deviceFeatures.geometryShader &&
      FindQueueFamilies(device).graphicsFamily.has_value() &&
      FindQueueFamilies(device).presentFamily.has_value() &&
      extensionsSupported && swapChainAdequate) {
    std::cout << "Device ID: " << deviceProperties.deviceID << std::endl;
    std::cout << "Device name: " << std::string{deviceProperties.deviceName}
              << std::endl;
    return true;
  }
  return false;
}

bool Context::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions_.begin(),
                                           deviceExtensions_.end());
  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

Context::QueueFamilyIndices
Context::FindQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;
  std::uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());
  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
    if (presentSupport) {
      indices.presentFamily = i;
    }
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    // Is indicies complete?
    if (indices.presentFamily.has_value() &&
        indices.graphicsFamily.has_value()) {
      break;
    }

    i++;
  }
  return indices;
}

Context::SwapChainSupportDetails
Context::QuerySwapChainSupport(VkPhysicalDevice device) {
  SwapChainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_,
                                            &details.capabilities);
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount,
                                         details.formats.data());
  }
  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount,
                                            nullptr);
  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface_, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR Context::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      std::cout << "Swap chain: Surface mode: VK_FORMAT_B8G8R8A8_SRGB is used"
                << std::endl;
      return availableFormat;
    }
  }
  // Here, we could start ranking the available formats based on how "good"
  // they are, but in most cases it's okay to just settle with the first
  // format that is specified.
  std::cout << "Swap chain: Surface mode: First available format is used"
            << std::endl;
  return availableFormats[0];
}

VkPresentModeKHR Context::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      std::cout
          << "Swap chain: Present mode: VK_PRESENT_MODE_MAILBOX_KHR is used"
          << std::endl;
      return availablePresentMode;
    }
  }
  std::cout << "Swap chain: Present mode: VK_PRESENT_MODE_FIFO_KHR is used"
            << std::endl;
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
Context::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    std::cout
        << "Swap chain: Extent: width and height are set to special values"
        << std::endl;
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)};

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    std::cout << "Swap chain: Extent: Width=" << actualExtent.width
              << " Height=" << actualExtent.height << std::endl;

    return actualExtent;
  }
}

void Context::CreateSwapChain() {
  SwapChainSupportDetails swapChainSupport =
      QuerySwapChainSupport(physicalDevice_);
  VkSurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode =
      ChooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);
  std::cout << "Swap chain: Creation: Minimum image count="
            << swapChainSupport.capabilities.minImageCount
            << " Maximum image count="
            << swapChainSupport.capabilities.maxImageCount << std::endl;
  // Simply sticking to this minimum means that we may sometimes have to wait
  // on the driver to complete internal operations before we can acquire
  // another image to render to. Therefore it is recommended to request at
  // least one more image than the minimum:
  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }
  std::cout << "Swap chain: Creation: Choosen image count: " << imageCount
            << std::endl;

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface_;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = FindQueueFamilies(physicalDevice_);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
    std::cout << "Swap chain: Creation: VK_SHARING_MODE_CONCURRENT is used"
              << std::endl;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;     // Optional
    createInfo.pQueueFamilyIndices = nullptr; // Optional
    std::cout << "Swap chain: Creation: VK_SHARING_MODE_EXCLUSIVE is used"
              << std::endl;
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
  swapChainImages_.resize(imageCount);
  std::cout << "Swap chain: Images: Final count: " << imageCount << std::endl;
  vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount,
                          swapChainImages_.data());

  swapChainImageFormat_ = surfaceFormat.format;
  swapChainExtent_ = extent;

  // Create image views for swapchain images:
  for (const auto &image : swapChainImages_) {
    ImageViewOptions options{};
    options.image = image;
    options.format = swapChainImageFormat_;
    const auto imageView = CreateImageView(options);
    imageViews_.pop_back();
    swapChainImageViews_.push_back(imageView);
  }

  currentSwapchainImageIndex_ = 0;
}

void Context::CreateSwapChainFramebuffers(VkRenderPass renderPass) {
  for (const auto &imageView : swapChainImageViews_) {
    FrameBufferOptions options{};
    options.renderPass = renderPass;
    options.extent = swapChainExtent_;
    options.imageAttachment = imageView;
    const auto framebuffer = CreateFramebuffer(options);
    framebuffers_.pop_back();
    swapChainFramebuffers_.push_back(framebuffer);
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
    throw std::runtime_error("failed to create image views!");
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
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = options.binding;
  layoutBinding.descriptorType = options.type;
  layoutBinding.descriptorCount = 1;
  layoutBinding.stageFlags = options.stageFlags;
  layoutBinding.pImmutableSamplers = nullptr; // Optional
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &layoutBinding;
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
  auto bindingDescription = GetBindingDescription();
  auto attributeDescriptions = GetAttributeDescriptions();
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<std::uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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
  scissor.extent = swapChainExtent_;
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
  QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice_);
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
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
  std::cout << "Vertex Buffer: Find memory type: Memory type count is "
            << memProperties.memoryTypeCount << std::endl;
  for (std::uint32_t i{0}; i < memProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      std::cout << "Vertex Buffer: Find memory type: Memory type " << i
                << " has been chosen" << std::endl;
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
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
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0; // Optional
  copyRegion.dstOffset = 0; // Optional
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue_);

  vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
}

VkBuffer Context::CreateVertexBuffer(const VertexBufferOptions &options) {
  VkDeviceSize bufferSize =
      sizeof(options.vertices[0]) * options.vertices.size();
  VkBuffer stagingBuffer{VK_NULL_HANDLE};
  VkDeviceMemory stagingBufferMemory{VK_NULL_HANDLE};
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);
  // Filling the vertex buffer:
  void *data;
  vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
  std::memcpy(data, options.vertices.data(), static_cast<size_t>(bufferSize));
  vkUnmapMemory(device_, stagingBufferMemory);

  VkBuffer vertexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory vertexBufferMemory{VK_NULL_HANDLE};
  CreateBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
  CopyBuffer(options.commandPool, stagingBuffer, vertexBuffer, bufferSize);

  vkDestroyBuffer(device_, stagingBuffer, nullptr);
  vkFreeMemory(device_, stagingBufferMemory, nullptr);

  vertexBuffers_.push_back(vertexBuffer);
  vertexBufferMemories_.push_back(vertexBufferMemory);
  return vertexBuffer;
}

VkBuffer Context::CreateIndexBuffer(const IndexBufferOptions &options) {
  VkDeviceSize bufferSize = sizeof(options.indices[0]) * options.indices.size();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void *data;
  vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, options.indices.data(), (size_t)bufferSize);
  vkUnmapMemory(device_, stagingBufferMemory);

  VkBuffer indexBuffer{VK_NULL_HANDLE};
  VkDeviceMemory indexBufferMemory{VK_NULL_HANDLE};
  CreateBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
  CopyBuffer(options.commandPool, stagingBuffer, indexBuffer, bufferSize);

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
  VkDescriptorPoolSize poolSize{};
  poolSize.type = options.type;
  poolSize.descriptorCount = options.descriptorCount;
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = options.descriptorCount;
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
  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = options.uniformBuffer;
  bufferInfo.offset = 0;
  bufferInfo.range = sizeof(UniformBufferObject);
  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = options.descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = options.descriptorType;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &bufferInfo;
  descriptorWrite.pImageInfo = nullptr;       // Optional
  descriptorWrite.pTexelBufferView = nullptr; // Optional
  vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
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
      swapChainFramebuffers_[currentSwapchainImageIndex_];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapChainExtent_;
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
  viewport.width = static_cast<float>(swapChainExtent_.width);
  viewport.height = static_cast<float>(swapChainExtent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(options.commandBuffer, 0, 1, &viewport);
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapChainExtent_;
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
      device_, swapChain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame_],
      VK_NULL_HANDLE, &currentSwapchainImageIndex_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapChain(options.renderPass);
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
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapChain_;
  presentInfo.pImageIndices = &currentSwapchainImageIndex_;
  presentInfo.pResults = nullptr; // Optional
  VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      framebufferResized_) {
    framebufferResized_ = false;
    RecreateSwapChain(options.renderPass);
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
  return {};
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

void Context::CleanupSwapChain() {
  for (const auto &framebuffer : swapChainFramebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  swapChainFramebuffers_.clear();

  for (const auto &imageView : swapChainImageViews_) {
    vkDestroyImageView(device_, imageView, nullptr);
  }
  swapChainImageViews_.clear();

  vkDestroySwapchainKHR(device_, swapChain_, nullptr);
  swapChain_ = VK_NULL_HANDLE;
  swapChainImages_.clear();
}

void Context::RecreateSwapChain(VkRenderPass renderPass) {
  // Handling minimization:
  int width = 0, height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(device_);

  CleanupSwapChain();
  CreateSwapChain();
  CreateSwapChainFramebuffers(renderPass);
}

} // namespace render