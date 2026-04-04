#include "vk_swapchain.h"

#include <algorithm>
#include <array>
#include <stdexcept>

// ── Public ──────────────────────────────────────────────────────
void VkSwapchain::create(VkContext &ctx, GLFWwindow *window)
{
	createSwapChain(ctx, window);
	createImageViews(ctx.getDevice());
	createDepthResources(ctx);
}

void VkSwapchain::createFramebuffers(VkDevice device, VkRenderPass renderPass)
{
	createFramebuffersInternal(device, renderPass);
}

void VkSwapchain::cleanup(VkDevice device)
{
	vkDestroyImageView(device, m_depthImageView, nullptr);
	vkDestroyImage(device, m_depthImage, nullptr);
	vkFreeMemory(device, m_depthImageMemory, nullptr);
	for(auto fb : m_framebuffers)
		vkDestroyFramebuffer(device, fb, nullptr);
	for(auto iv : m_imageViews)
		vkDestroyImageView(device, iv, nullptr);
	vkDestroySwapchainKHR(device, m_swapChain, nullptr);
}

void VkSwapchain::recreate(VkContext &ctx, GLFWwindow *window,
						   VkRenderPass renderPass)
{
	int w = 0, h = 0;
	glfwGetFramebufferSize(window, &w, &h);
	while(w == 0 || h == 0)
		{
			glfwGetFramebufferSize(window, &w, &h);
			glfwWaitEvents();
		}
	vkDeviceWaitIdle(ctx.getDevice());
	cleanup(ctx.getDevice());
	create(ctx, window);
	createFramebuffers(ctx.getDevice(), renderPass);
}

// ── Swap Chain Creation ─────────────────────────────────────────
void VkSwapchain::createSwapChain(VkContext &ctx, GLFWwindow *window)
{
	auto support = ctx.querySwapChainSupport(ctx.getPhysicalDevice());
	auto surfaceFormat = chooseSwapSurfaceFormat(support.formats);
	auto presentMode = chooseSwapPresentMode(support.presentModes);
	auto extent = chooseSwapExtent(support.capabilities, window);
	uint32_t imageCount = support.capabilities.minImageCount + 1;
	if(support.capabilities.maxImageCount > 0
	   && imageCount > support.capabilities.maxImageCount)
		imageCount = support.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR ci{};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = ctx.getSurface();
	ci.minImageCount = imageCount;
	ci.imageFormat = surfaceFormat.format;
	ci.imageColorSpace = surfaceFormat.colorSpace;
	ci.imageExtent = extent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto indices = ctx.findQueueFamilies(ctx.getPhysicalDevice());
	uint32_t queueFamilyIndices[]
	  = {indices.graphicsFamily.value(), indices.presentFamily.value()};
	if(indices.graphicsFamily != indices.presentFamily)
		{
			ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			ci.queueFamilyIndexCount = 2;
			ci.pQueueFamilyIndices = queueFamilyIndices;
		}
	else
		{
			ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
	ci.preTransform = support.capabilities.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = presentMode;
	ci.clipped = VK_TRUE;

	if(vkCreateSwapchainKHR(ctx.getDevice(), &ci, nullptr, &m_swapChain)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create swap chain");

	vkGetSwapchainImagesKHR(ctx.getDevice(), m_swapChain, &imageCount,
							nullptr);
	m_images.resize(imageCount);
	vkGetSwapchainImagesKHR(ctx.getDevice(), m_swapChain, &imageCount,
							m_images.data());
	m_imageFormat = surfaceFormat.format;
	m_extent = extent;
}

// ── Image Views ─────────────────────────────────────────────────
VkImageView
VkSwapchain::createImageView(VkDevice device, VkImage image, VkFormat format,
							 VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ci.image = image;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = format;
	ci.subresourceRange.aspectMask = aspectFlags;
	ci.subresourceRange.baseMipLevel = 0;
	ci.subresourceRange.levelCount = 1;
	ci.subresourceRange.baseArrayLayer = 0;
	ci.subresourceRange.layerCount = 1;
	VkImageView view;
	if(vkCreateImageView(device, &ci, nullptr, &view) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image view");
	return view;
}

void VkSwapchain::createImageViews(VkDevice device)
{
	m_imageViews.resize(m_images.size());
	for(size_t i = 0; i < m_images.size(); i++)
		m_imageViews[i] = createImageView(device, m_images[i], m_imageFormat,
										  VK_IMAGE_ASPECT_COLOR_BIT);
}

// ── Depth Resources ─────────────────────────────────────────────
VkFormat
VkSwapchain::findSupportedFormat(VkPhysicalDevice physDevice,
								 const std::vector<VkFormat> &candidates,
								 VkImageTiling tiling,
								 VkFormatFeatureFlags features)
{
	for(auto f : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physDevice, f, &props);
			if(tiling == VK_IMAGE_TILING_LINEAR
			   && (props.linearTilingFeatures & features) == features)
				return f;
			if(tiling == VK_IMAGE_TILING_OPTIMAL
			   && (props.optimalTilingFeatures & features) == features)
				return f;
		}
	throw std::runtime_error("Failed to find supported format");
}

VkFormat VkSwapchain::findDepthFormat(VkPhysicalDevice physDevice)
{
	return findSupportedFormat(
	  physDevice,
	  {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
	   VK_FORMAT_D24_UNORM_S8_UINT},
	  VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VkSwapchain::createImage(VkContext &ctx, uint32_t w, uint32_t h,
							  VkFormat format, VkImageTiling tiling,
							  VkImageUsageFlags usage,
							  VkMemoryPropertyFlags props, VkImage &image,
							  VkDeviceMemory &memory)
{
	VkImageCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.extent = {w, h, 1};
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.format = format;
	ci.tiling = tiling;
	ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ci.usage = usage;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(vkCreateImage(ctx.getDevice(), &ci, nullptr, &image) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image");

	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(ctx.getDevice(), image, &memReq);
	VkMemoryAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = memReq.size;
	ai.memoryTypeIndex = ctx.findMemoryType(memReq.memoryTypeBits, props);
	if(vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate image memory");
	vkBindImageMemory(ctx.getDevice(), image, memory, 0);
}

void VkSwapchain::createDepthResources(VkContext &ctx)
{
	VkFormat depthFormat = findDepthFormat(ctx.getPhysicalDevice());
	createImage(
	  ctx, m_extent.width, m_extent.height, depthFormat,
	  VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);
	m_depthImageView = createImageView(ctx.getDevice(), m_depthImage,
									   depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

// ── Framebuffers ────────────────────────────────────────────────
void VkSwapchain::createFramebuffersInternal(VkDevice device,
											 VkRenderPass renderPass)
{
	m_framebuffers.resize(m_imageViews.size());
	for(size_t i = 0; i < m_imageViews.size(); i++)
		{
			std::array<VkImageView, 2> attachments
			  = {m_imageViews[i], m_depthImageView};
			VkFramebufferCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			ci.renderPass = renderPass;
			ci.attachmentCount = static_cast<uint32_t>(attachments.size());
			ci.pAttachments = attachments.data();
			ci.width = m_extent.width;
			ci.height = m_extent.height;
			ci.layers = 1;
			if(vkCreateFramebuffer(device, &ci, nullptr, &m_framebuffers[i])
			   != VK_SUCCESS)
				throw std::runtime_error("Failed to create framebuffer");
		}
}

// ── Format Selection ────────────────────────────────────────────
VkSurfaceFormatKHR VkSwapchain::chooseSwapSurfaceFormat(
  const std::vector<VkSurfaceFormatKHR> &formats)
{
	for(auto &f : formats)
		if(f.format == VK_FORMAT_B8G8R8A8_SRGB
		   && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return f;
	return formats[0];
}

VkPresentModeKHR
VkSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes)
{
	// Priority 1: IMMEDIATE (Uncapped FPS)
	for(auto m : modes)
		if(m == VK_PRESENT_MODE_IMMEDIATE_KHR)
			return m;

	// Priority 2: MAILBOX (Triple buffering, uncapped internal but presenting at screen rate)
	for(auto m : modes)
		if(m == VK_PRESENT_MODE_MAILBOX_KHR)
			return m;

	return VK_PRESENT_MODE_FIFO_KHR; // Fallback: VSYNC
}

VkExtent2D VkSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &caps,
										 GLFWwindow *window)
{
	if(caps.currentExtent.width != UINT32_MAX)
		return caps.currentExtent;
	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	VkExtent2D ext = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
	ext.width = std::clamp(ext.width, caps.minImageExtent.width,
						   caps.maxImageExtent.width);
	ext.height = std::clamp(ext.height, caps.minImageExtent.height,
							caps.maxImageExtent.height);
	return ext;
}
