#pragma once

#include "vk_context.h"
#include <vector>

/// Gère le Swapchain Vulkan : création, recreation, image views, framebuffers.
class VkSwapchain {
public:
  void create(VkContext &ctx, GLFWwindow *window);
  void createFramebuffers(VkDevice device, VkRenderPass renderPass);
  void cleanup(VkDevice device);
  void recreate(VkContext &ctx, GLFWwindow *window, VkRenderPass renderPass);

  VkSwapchainKHR getSwapchain() const { return m_swapChain; }
  VkFormat getImageFormat() const { return m_imageFormat; }
  VkExtent2D getExtent() const { return m_extent; }
  const std::vector<VkFramebuffer> &getFramebuffers() const {
    return m_framebuffers;
  }
  uint32_t getImageCount() const {
    return static_cast<uint32_t>(m_images.size());
  }

  // Depth resources
  VkImageView getDepthImageView() const { return m_depthImageView; }

private:
  VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
  std::vector<VkImage> m_images;
  VkFormat m_imageFormat;
  VkExtent2D m_extent;
  std::vector<VkImageView> m_imageViews;
  std::vector<VkFramebuffer> m_framebuffers;

  // Depth
  VkImage m_depthImage = VK_NULL_HANDLE;
  VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
  VkImageView m_depthImageView = VK_NULL_HANDLE;

  void createSwapChain(VkContext &ctx, GLFWwindow *window);
  void createImageViews(VkDevice device);
  void createDepthResources(VkContext &ctx);
  void createFramebuffersInternal(VkDevice device, VkRenderPass renderPass);

  VkSurfaceFormatKHR
  chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
  VkPresentModeKHR
  chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &caps,
                              GLFWwindow *window);

  VkImageView createImageView(VkDevice device, VkImage image, VkFormat format,
                              VkImageAspectFlags aspectFlags);
  void createImage(VkContext &ctx, uint32_t w, uint32_t h, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags props, VkImage &image,
                   VkDeviceMemory &memory);
  VkFormat findDepthFormat(VkPhysicalDevice physDevice);
  VkFormat findSupportedFormat(VkPhysicalDevice physDevice,
                               const std::vector<VkFormat> &candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
};
