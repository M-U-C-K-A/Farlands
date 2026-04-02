#pragma once

#include "vk_context.h"
#include "vk_swapchain.h"
#include "../core/types.h"

#include <string>
#include <vector>

/// Gère le Render Pass et le Graphics Pipeline Vulkan.
class VkPipelineManager {
public:
  void create(VkContext &ctx, VkSwapchain &swapchain);
  void cleanup(VkDevice device);

  VkRenderPass getRenderPass() const { return m_renderPass; }
  VkPipeline getPipeline() const { return m_graphicsPipeline; }
  VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
  VkDescriptorSetLayout getDescriptorSetLayout() const {
    return m_descriptorSetLayout;
  }

private:
  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

  void createRenderPass(VkDevice device, VkFormat swapchainFormat,
                        VkPhysicalDevice physDevice);
  void createDescriptorSetLayout(VkDevice device);
  void createGraphicsPipeline(VkDevice device);

  VkShaderModule createShaderModule(VkDevice device,
                                    const std::vector<char> &code);

  VkFormat findDepthFormat(VkPhysicalDevice physDevice);
  VkFormat findSupportedFormat(VkPhysicalDevice physDevice,
                               const std::vector<VkFormat> &candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

  static std::vector<char> readFile(const std::string &filename);
};
