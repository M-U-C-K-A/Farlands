#pragma once

#include "vk_context.h"
#include <vector>

/// Gère le Swapchain Vulkan : création, recréation, image views, framebuffers.
/// La scène est rendue hors écran ; le swapchain ne reçoit que la passe
/// finale (composition + interface), sans tampon de profondeur.
class VkSwapchain
{
  public:
	void create(VkContext &ctx, GLFWwindow *window, bool vsync);
	void createFramebuffers(VkDevice device, VkRenderPass renderPass);
	void cleanup(VkDevice device);
	void recreate(VkContext &ctx, GLFWwindow *window, VkRenderPass renderPass,
				  bool vsync);

	VkSwapchainKHR getSwapchain() const { return m_swapChain; }
	VkFormat getImageFormat() const { return m_imageFormat; }
	VkExtent2D getExtent() const { return m_extent; }
	const std::vector<VkFramebuffer> &getFramebuffers() const
	{
		return m_framebuffers;
	}
	uint32_t getImageCount() const
	{
		return static_cast<uint32_t>(m_images.size());
	}
	VkImage getImage(uint32_t index) const { return m_images[index]; }
	bool supportsTransferSrc() const { return m_supportsTransferSrc; }
	VkPresentModeKHR getPresentMode() const { return m_presentMode; }
	/// Le format est sRGB : la conversion gamma est faite par le matériel
	bool isSrgb() const;

  private:
	VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> m_images;
	VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D m_extent{};
	bool m_supportsTransferSrc = false;
	VkPresentModeKHR m_presentMode = VK_PRESENT_MODE_FIFO_KHR;
	std::vector<VkImageView> m_imageViews;
	std::vector<VkFramebuffer> m_framebuffers;

	void createSwapChain(VkContext &ctx, GLFWwindow *window, bool vsync);
	void createImageViews(VkDevice device);

	VkSurfaceFormatKHR
	chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
	VkPresentModeKHR
	chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes,
						  bool vsync);
	VkExtent2D
	chooseSwapExtent(const VkSurfaceCapabilitiesKHR &caps, GLFWwindow *window);
};
