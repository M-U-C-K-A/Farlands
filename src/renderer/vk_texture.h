#pragma once

#include "vk_context.h"
#include <string>
#include <vulkan/vulkan.h>

class VkTexture {
public:
    void init(VkContext& ctx, const std::string& path);
    void cleanup(VkDevice device);

    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }

private:
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    void createImage(VkContext& ctx, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    void transitionImageLayout(VkContext& ctx, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkContext& ctx, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
};
