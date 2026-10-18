#include "vk_swapchain.h"

#include <algorithm>
#include <stdexcept>
#include "core/logger.h"

// ── Public ──────────────────────────────────────────────────────
void VkSwapchain::create(VkContext &ctx, GLFWwindow *window, bool vsync)
{
	createSwapChain(ctx, window, vsync);
	createImageViews(ctx.getDevice());
}

void VkSwapchain::createFramebuffers(VkDevice device, VkRenderPass renderPass)
{
	m_framebuffers.resize(m_imageViews.size());
	for(size_t i = 0; i < m_imageViews.size(); i++)
		{
			VkFramebufferCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			ci.renderPass = renderPass;
			ci.attachmentCount = 1;
			ci.pAttachments = &m_imageViews[i];
			ci.width = m_extent.width;
			ci.height = m_extent.height;
			ci.layers = 1;
			if(vkCreateFramebuffer(device, &ci, nullptr, &m_framebuffers[i])
			   != VK_SUCCESS)
				throw std::runtime_error("Failed to create framebuffer");
		}
}

void VkSwapchain::cleanup(VkDevice device)
{
	for(auto fb : m_framebuffers)
		vkDestroyFramebuffer(device, fb, nullptr);
	m_framebuffers.clear();
	for(auto iv : m_imageViews)
		vkDestroyImageView(device, iv, nullptr);
	m_imageViews.clear();
	vkDestroySwapchainKHR(device, m_swapChain, nullptr);
	m_swapChain = VK_NULL_HANDLE;
}

void VkSwapchain::recreate(VkContext &ctx, GLFWwindow *window,
						   VkRenderPass renderPass, bool vsync)
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
	create(ctx, window, vsync);
	createFramebuffers(ctx.getDevice(), renderPass);
}

bool VkSwapchain::isSrgb() const
{
	return m_imageFormat == VK_FORMAT_B8G8R8A8_SRGB
		   || m_imageFormat == VK_FORMAT_R8G8B8A8_SRGB;
}

// ── Swap Chain Creation ─────────────────────────────────────────
void VkSwapchain::createSwapChain(VkContext &ctx, GLFWwindow *window,
								  bool vsync)
{
	auto support = ctx.querySwapChainSupport(ctx.getPhysicalDevice());
	auto surfaceFormat = chooseSwapSurfaceFormat(support.formats);
	auto presentMode = chooseSwapPresentMode(support.presentModes, vsync);
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
	// Permet la copie des images (captures d'écran F2) si supporté
	m_supportsTransferSrc = (support.capabilities.supportedUsageFlags
							 & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
							!= 0;
	if(m_supportsTransferSrc)
		ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

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
	m_presentMode = presentMode;

	const char *modeName = presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "immediate"
						   : presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "mailbox"
																		: "fifo";
	LOG_INFO("Swapchain created: " << m_extent.width << "x" << m_extent.height << " with "
								   << m_images.size() << " images, present mode " << modeName);
}

// ── Image Views ─────────────────────────────────────────────────
void VkSwapchain::createImageViews(VkDevice device)
{
	m_imageViews.resize(m_images.size());
	for(size_t i = 0; i < m_images.size(); i++)
		{
			VkImageViewCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			ci.image = m_images[i];
			ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			ci.format = m_imageFormat;
			ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
			if(vkCreateImageView(device, &ci, nullptr, &m_imageViews[i])
			   != VK_SUCCESS)
				throw std::runtime_error("Failed to create image view");
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
VkSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes,
								   bool vsync)
{
	if(vsync)
		return VK_PRESENT_MODE_FIFO_KHR; // toujours disponible

	// Sans vsync : IMMEDIATE (FPS non plafonnés) sinon MAILBOX
	for(auto m : modes)
		if(m == VK_PRESENT_MODE_IMMEDIATE_KHR)
			return m;
	for(auto m : modes)
		if(m == VK_PRESENT_MODE_MAILBOX_KHR)
			return m;
	return VK_PRESENT_MODE_FIFO_KHR;
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
