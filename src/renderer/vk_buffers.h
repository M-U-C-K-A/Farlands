#pragma once

#include "../core/types.h"
#include "vk_context.h"

#include <vector>

const int MAX_FRAMES_IN_FLIGHT = 2;

/// Gère les buffers GPU Vulkan : vertex, index, uniform, descriptors.
class VkBufferManager {
public:
  void init(VkContext &ctx, VkDescriptorSetLayout descriptorSetLayout);
  void cleanup(VkDevice device);

  void updateMeshBuffers(VkContext &ctx, const std::vector<Vertex> &vertices,
                         const std::vector<uint32_t> &indices);
  void updateUniform(uint32_t frameIndex, const UniformBufferObject &ubo);

  uint32_t getIndexCount() const { return m_indexCount; }
  VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
  VkBuffer getIndexBuffer() const { return m_indexBuffer; }
  VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const {
    return m_descriptorSets[frameIndex];
  }
  VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }

private:
  VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
  VkBuffer m_indexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
  uint32_t m_indexCount = 0;

  std::vector<VkBuffer> m_uniformBuffers;
  std::vector<VkDeviceMemory> m_uniformBuffersMemory;
  std::vector<void *> m_uniformBuffersMapped;

  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> m_descriptorSets;

  VkCommandPool m_commandPool = VK_NULL_HANDLE;

  void createCommandPool(VkContext &ctx);
  void createUniformBuffers(VkContext &ctx);
  void createDescriptorPool(VkDevice device);
  void createDescriptorSets(VkDevice device,
                             VkDescriptorSetLayout descriptorSetLayout);

  void createBuffer(VkContext &ctx, VkDeviceSize size,
                    VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                    VkBuffer &buffer, VkDeviceMemory &memory);
  void copyBuffer(VkDevice device, VkQueue graphicsQueue, VkBuffer src,
                  VkBuffer dst, VkDeviceSize size);
};
