#include "renderer.h"

#include <fstream>
#include <stdexcept>
#include <iostream>
#include <set>
#include <algorithm>
#include <cstring>

const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

static bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());
    for (auto layerName : validationLayers) {
        bool found = false;
        for (auto& prop : available) { if (strcmp(layerName, prop.layerName) == 0) { found = true; break; } }
        if (!found) return false;
    }
    return true;
}

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = checkValidationLayerSupport();
#endif
const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "Validation: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

std::vector<char> Renderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filename);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

void Renderer::init(GLFWwindow* window) {
    m_window = window;
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void Renderer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Minecraft Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Custom";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");
}

void Renderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    ci.pfnUserCallback = debugCallback;
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) func(m_instance, &ci, nullptr, &m_debugMessenger);
}

void Renderer::createSurface() {
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present);
        if (present) indices.presentFamily = i;
        if (indices.isComplete()) break;
    }
    return indices;
}

SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount) { details.formats.resize(formatCount); vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data()); }
    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount, nullptr);
    if (modeCount) { details.presentModes.resize(modeCount); vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount, details.presentModes.data()); }
    return details;
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device) {
    auto indices = findQueueFamilies(device);
    uint32_t extCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, available.data());
    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (auto& ext : available) required.erase(ext.extensionName);
    bool swapChainOk = false;
    if (required.empty()) { auto s = querySwapChainSupport(device); swapChainOk = !s.formats.empty() && !s.presentModes.empty(); }
    return indices.isComplete() && required.empty() && swapChainOk;
}

void Renderer::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No GPUs with Vulkan support");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
    for (auto& d : devices) { if (isDeviceSuitable(d)) { m_physicalDevice = d; break; } }
    if (m_physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable GPU found");
}

void Renderer::createLogicalDevice() {
    auto indices = findQueueFamilies(m_physicalDevice);
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    float priority = 1.0f;
    for (auto family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{}; qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family; qi.queueCount = 1; qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }
    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    ci.pQueueCreateInfos = queueInfos.data();
    ci.pEnabledFeatures = &features;
    ci.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    ci.ppEnabledExtensionNames = deviceExtensions.data();
    if (vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device");
    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (auto& f : formats) if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
    return formats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w, h; glfwGetFramebufferSize(m_window, &w, &h);
    VkExtent2D ext = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    ext.width = std::clamp(ext.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return ext;
}

void Renderer::createSwapChain() {
    auto support = querySwapChainSupport(m_physicalDevice);
    auto surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    auto presentMode = chooseSwapPresentMode(support.presentModes);
    auto extent = chooseSwapExtent(support.capabilities);
    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR ci{}; ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = m_surface; ci.minImageCount = imageCount;
    ci.imageFormat = surfaceFormat.format; ci.imageColorSpace = surfaceFormat.colorSpace;
    ci.imageExtent = extent; ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = findQueueFamilies(m_physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT; ci.queueFamilyIndexCount = 2; ci.pQueueFamilyIndices = queueFamilyIndices;
    } else { ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
    ci.preTransform = support.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode; ci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapChain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swap chain");
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image; ci.viewType = VK_IMAGE_VIEW_TYPE_2D; ci.format = format;
    ci.subresourceRange.aspectMask = aspectFlags; ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1; ci.subresourceRange.baseArrayLayer = 0; ci.subresourceRange.layerCount = 1;
    VkImageView view;
    if (vkCreateImageView(m_device, &ci, nullptr, &view) != VK_SUCCESS) throw std::runtime_error("Failed to create image view");
    return view;
}

void Renderer::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());
    for (size_t i = 0; i < m_swapChainImages.size(); i++)
        m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (auto f : candidates) {
        VkFormatProperties props; vkGetPhysicalDeviceFormatProperties(m_physicalDevice, f, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return f;
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return f;
    }
    throw std::runtime_error("Failed to find supported format");
}

VkFormat Renderer::findDepthFormat() {
    return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void Renderer::createRenderPass() {
    VkAttachmentDescription colorAtt{}; colorAtt.format = m_swapChainImageFormat;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT; colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE; colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAtt{}; depthAtt.format = findDepthFormat();
    depthAtt.samples = VK_SAMPLE_COUNT_1_BIT; depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; depthAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; depthAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorRef; subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{}; dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAtt, depthAtt};
    VkRenderPassCreateInfo rpci{}; rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = static_cast<uint32_t>(attachments.size()); rpci.pAttachments = attachments.data();
    rpci.subpassCount = 1; rpci.pSubpasses = &subpass; rpci.dependencyCount = 1; rpci.pDependencies = &dep;
    if (vkCreateRenderPass(m_device, &rpci, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
}

void Renderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0; uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1; uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1; ci.pBindings = &uboBinding;
    if (vkCreateDescriptorSetLayout(m_device, &ci, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout");
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size(); ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    if (vkCreateShaderModule(m_device, &ci, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
    return mod;
}

void Renderer::createGraphicsPipeline() {
    auto vertCode = readFile(std::string(SHADER_DIR) + "/shader.vert.spv");
    auto fragCode = readFile(std::string(SHADER_DIR) + "/shader.frag.spv");
    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{}; vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT; vertStage.module = vertModule; vertStage.pName = "main";
    VkPipelineShaderStageCreateInfo fragStage{}; fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fragStage.module = fragModule; fragStage.pName = "main";
    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    auto bindingDesc = Vertex::getBindingDescription();
    auto attrDesc = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInput{}; vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1; vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size()); vertexInput.pVertexAttributeDescriptions = attrDesc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAsm{}; inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{}; viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1; viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rast{}; rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = VK_POLYGON_MODE_FILL; rast.lineWidth = 1.0f;
    rast.cullMode = VK_CULL_MODE_BACK_BIT; rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{}; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{}; depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE; depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAtt{}; colorBlendAtt.colorWriteMask = 0xF;

    VkPipelineColorBlendStateCreateInfo colorBlend{}; colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1; colorBlend.pAttachments = &colorBlendAtt;

    std::vector<VkDynamicState> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynState{}; dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = static_cast<uint32_t>(dynStates.size()); dynState.pDynamicStates = dynStates.data();

    VkPipelineLayoutCreateInfo layoutCI{}; layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = 1; layoutCI.pSetLayouts = &m_descriptorSetLayout;
    if (vkCreatePipelineLayout(m_device, &layoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineCI{}; pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2; pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInput; pipelineCI.pInputAssemblyState = &inputAsm;
    pipelineCI.pViewportState = &viewportState; pipelineCI.pRasterizationState = &rast;
    pipelineCI.pMultisampleState = &ms; pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.pColorBlendState = &colorBlend; pipelineCI.pDynamicState = &dynState;
    pipelineCI.layout = m_pipelineLayout; pipelineCI.renderPass = m_renderPass; pipelineCI.subpass = 0;
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    vkDestroyShaderModule(m_device, fragModule, nullptr);
    vkDestroyShaderModule(m_device, vertModule, nullptr);
}

void Renderer::createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags props, VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ci.imageType = VK_IMAGE_TYPE_2D;
    ci.extent = {w, h, 1}; ci.mipLevels = 1; ci.arrayLayers = 1; ci.format = format;
    ci.tiling = tiling; ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.usage = usage; ci.samples = VK_SAMPLE_COUNT_1_BIT; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(m_device, &ci, nullptr, &image) != VK_SUCCESS) throw std::runtime_error("Failed to create image");
    VkMemoryRequirements memReq; vkGetImageMemoryRequirements(m_device, image, &memReq);
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = memReq.size; ai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, props);
    if (vkAllocateMemory(m_device, &ai, nullptr, &memory) != VK_SUCCESS) throw std::runtime_error("Failed to allocate image memory");
    vkBindImageMemory(m_device, image, memory, 0);
}

void Renderer::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    createImage(m_swapChainExtent.width, m_swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);
    m_depthImageView = createImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::createFramebuffers() {
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
    for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {m_swapChainImageViews[i], m_depthImageView};
        VkFramebufferCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = m_renderPass; ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        ci.pAttachments = attachments.data();
        ci.width = m_swapChainExtent.width; ci.height = m_swapChainExtent.height; ci.layers = 1;
        if (vkCreateFramebuffer(m_device, &ci, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
}

void Renderer::createCommandPool() {
    auto indices = findQueueFamilies(m_physicalDevice);
    VkCommandPoolCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = indices.graphicsFamily.value();
    if (vkCreateCommandPool(m_device, &ci, nullptr, &m_commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
    throw std::runtime_error("Failed to find suitable memory type");
}

void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
    VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &ci, nullptr, &buffer) != VK_SUCCESS) throw std::runtime_error("Failed to create buffer");
    VkMemoryRequirements memReq; vkGetBufferMemoryRequirements(m_device, buffer, &memReq);
    VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = memReq.size; ai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, props);
    if (vkAllocateMemory(m_device, &ai, nullptr, &memory) != VK_SUCCESS) throw std::runtime_error("Failed to allocate buffer memory");
    vkBindBufferMemory(m_device, buffer, memory, 0);
}

void Renderer::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandPool = m_commandPool; ai.commandBufferCount = 1;
    VkCommandBuffer cmd; vkAllocateCommandBuffers(m_device, &ai, &cmd);
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy region{}; region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    vkQueueSubmit(m_graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void Renderer::updateBuffers(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    vkDeviceWaitIdle(m_device);

    m_indexCount = static_cast<uint32_t>(indices.size());
    if (m_indexCount == 0 || vertices.empty()) return;

    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    }
    if (m_indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
    }

    VkDeviceSize vSize = sizeof(vertices[0]) * vertices.size();
    VkBuffer vStaging; VkDeviceMemory vStagingMem;
    createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vStaging, vStagingMem);
    void* vData; vkMapMemory(m_device, vStagingMem, 0, vSize, 0, &vData);
    memcpy(vData, vertices.data(), (size_t)vSize); vkUnmapMemory(m_device, vStagingMem);
    createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);
    copyBuffer(vStaging, m_vertexBuffer, vSize);
    vkDestroyBuffer(m_device, vStaging, nullptr); vkFreeMemory(m_device, vStagingMem, nullptr);

    VkDeviceSize iSize = sizeof(indices[0]) * indices.size();
    VkBuffer iStaging; VkDeviceMemory iStagingMem;
    createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, iStaging, iStagingMem);
    void* iData; vkMapMemory(m_device, iStagingMem, 0, iSize, 0, &iData);
    memcpy(iData, indices.data(), (size_t)iSize); vkUnmapMemory(m_device, iStagingMem);
    createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory);
    copyBuffer(iStaging, m_indexBuffer, iSize);
    vkDestroyBuffer(m_device, iStaging, nullptr); vkFreeMemory(m_device, iStagingMem, nullptr);
}

void Renderer::createUniformBuffers() {
    VkDeviceSize size = sizeof(UniformBufferObject);
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_uniformBuffers[i], m_uniformBuffersMemory[i]);
        vkMapMemory(m_device, m_uniformBuffersMemory[i], 0, size, 0, &m_uniformBuffersMapped[i]);
    }
}

void Renderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{}; poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;
    VkDescriptorPoolCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = 1; ci.pPoolSizes = &poolSize; ci.maxSets = MAX_FRAMES_IN_FLIGHT;
    if (vkCreateDescriptorPool(m_device, &ci, nullptr, &m_descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
}

void Renderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_descriptorPool; ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT; ai.pSetLayouts = layouts.data();
    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device, &ai, m_descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor sets");
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufInfo{}; bufInfo.buffer = m_uniformBuffers[i]; bufInfo.offset = 0; bufInfo.range = sizeof(UniformBufferObject);
        VkWriteDescriptorSet write{}; write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSets[i]; write.dstBinding = 0; write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; write.descriptorCount = 1; write.pBufferInfo = &bufInfo;
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
}

void Renderer::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = m_commandPool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (vkAllocateCommandBuffers(m_device, &ai, m_commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

void Renderer::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi{}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &si, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &si, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fi, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create sync objects");
    }
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) throw std::runtime_error("Failed to begin command buffer");

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.05f, 0.05f, 0.08f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rpbi{}; rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = m_renderPass; rpbi.framebuffer = m_swapChainFramebuffers[imageIndex];
    rpbi.renderArea.extent = m_swapChainExtent;
    rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size()); rpbi.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    VkViewport viewport{}; viewport.width = (float)m_swapChainExtent.width; viewport.height = (float)m_swapChainExtent.height;
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{}; scissor.extent = m_swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (m_indexCount > 0 && m_vertexBuffer != VK_NULL_HANDLE) {
        VkBuffer vertBufs[] = {m_vertexBuffer}; VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertBufs, offsets);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) throw std::runtime_error("Failed to record command buffer");
}

void Renderer::drawFrame(const UniformBufferObject& ubo) {
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapChain(); return; }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) throw std::runtime_error("Failed to acquire swap chain image");

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    memcpy(m_uniformBuffersMapped[m_currentFrame], &ubo, sizeof(ubo));

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSems[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = waitSems; si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1; si.pCommandBuffers = &m_commandBuffers[m_currentFrame];
    VkSemaphore signalSems[] = {m_renderFinishedSemaphores[m_currentFrame]};
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = signalSems;
    if (vkQueueSubmit(m_graphicsQueue, 1, &si, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit draw command buffer");

    VkPresentInfoKHR pi{}; pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = signalSems;
    VkSwapchainKHR swapChains[] = {m_swapChain};
    pi.swapchainCount = 1; pi.pSwapchains = swapChains; pi.pImageIndices = &imageIndex;
    result = vkQueuePresentKHR(m_presentQueue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false; recreateSwapChain();
    } else if (result != VK_SUCCESS) throw std::runtime_error("Failed to present swap chain image");
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::cleanupSwapChain() {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);
    for (auto fb : m_swapChainFramebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto iv : m_swapChainImageViews) vkDestroyImageView(m_device, iv, nullptr);
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

void Renderer::recreateSwapChain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    while (w == 0 || h == 0) { glfwGetFramebufferSize(m_window, &w, &h); glfwWaitEvents(); }
    vkDeviceWaitIdle(m_device);
    cleanupSwapChain();
    createSwapChain(); createImageViews(); createDepthResources(); createFramebuffers();
}

void Renderer::waitIdle() { vkDeviceWaitIdle(m_device); }

void Renderer::cleanup() {
    cleanupSwapChain();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr);
        vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
    }
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr); vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr); vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyDevice(m_device, nullptr);
    if (enableValidationLayers) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_instance, m_debugMessenger, nullptr);
    }
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}
