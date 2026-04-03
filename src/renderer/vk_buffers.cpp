#include "vk_buffers.h"

#include <cstring>
#include <stdexcept>

// ── Public ──────────────────────────────────────────────────────
void VkBufferManager::init(VkContext &ctx,
						   VkDescriptorSetLayout descriptorSetLayout, VkImageView imageView, VkSampler sampler)
{
	createCommandPool(ctx);
	createUniformBuffers(ctx);
	createDescriptorPool(ctx.getDevice());
	createDescriptorSets(ctx.getDevice(), descriptorSetLayout, imageView, sampler);
}

void VkBufferManager::cleanup(VkDevice device)
{
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
			vkFreeMemory(device, m_uniformBuffersMemory[i], nullptr);
		}
	vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
	if(m_indexBuffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(device, m_indexBuffer, nullptr);
			vkFreeMemory(device, m_indexBufferMemory, nullptr);
		}
	if(m_vertexBuffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(device, m_vertexBuffer, nullptr);
			vkFreeMemory(device, m_vertexBufferMemory, nullptr);
		}
	vkDestroyCommandPool(device, m_commandPool, nullptr);
}

void VkBufferManager::updateUniform(uint32_t frameIndex,
									const UniformBufferObject &ubo)
{
	memcpy(m_uniformBuffersMapped[frameIndex], &ubo, sizeof(ubo));
}

// ── Mesh Buffers ────────────────────────────────────────────────
void VkBufferManager::updateMeshBuffers(VkContext &ctx,
										const std::vector<Vertex> &vertices,
										const std::vector<uint32_t> &indices)
{
	vkDeviceWaitIdle(ctx.getDevice());

	m_indexCount = static_cast<uint32_t>(indices.size());
	if(m_indexCount == 0 || vertices.empty())
		return;

	if(m_vertexBuffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(ctx.getDevice(), m_vertexBuffer, nullptr);
			vkFreeMemory(ctx.getDevice(), m_vertexBufferMemory, nullptr);
		}
	if(m_indexBuffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(ctx.getDevice(), m_indexBuffer, nullptr);
			vkFreeMemory(ctx.getDevice(), m_indexBufferMemory, nullptr);
		}

	// Vertex buffer
	VkDeviceSize vSize = sizeof(vertices[0]) * vertices.size();
	VkBuffer vStaging;
	VkDeviceMemory vStagingMem;
	createBuffer(ctx, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				 vStaging, vStagingMem);
	void *vData;
	vkMapMemory(ctx.getDevice(), vStagingMem, 0, vSize, 0, &vData);
	memcpy(vData, vertices.data(), (size_t)vSize);
	vkUnmapMemory(ctx.getDevice(), vStagingMem);
	createBuffer(ctx, vSize,
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT
				   | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer,
				 m_vertexBufferMemory);
	copyBuffer(ctx.getDevice(), ctx.getGraphicsQueue(), vStaging,
			   m_vertexBuffer, vSize);
	vkDestroyBuffer(ctx.getDevice(), vStaging, nullptr);
	vkFreeMemory(ctx.getDevice(), vStagingMem, nullptr);

	// Index buffer
	VkDeviceSize iSize = sizeof(indices[0]) * indices.size();
	VkBuffer iStaging;
	VkDeviceMemory iStagingMem;
	createBuffer(ctx, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				 iStaging, iStagingMem);
	void *iData;
	vkMapMemory(ctx.getDevice(), iStagingMem, 0, iSize, 0, &iData);
	memcpy(iData, indices.data(), (size_t)iSize);
	vkUnmapMemory(ctx.getDevice(), iStagingMem);
	createBuffer(
	  ctx, iSize,
	  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory);
	copyBuffer(ctx.getDevice(), ctx.getGraphicsQueue(), iStaging,
			   m_indexBuffer, iSize);
	vkDestroyBuffer(ctx.getDevice(), iStaging, nullptr);
	vkFreeMemory(ctx.getDevice(), iStagingMem, nullptr);
}

// ── Command Pool ────────────────────────────────────────────────
void VkBufferManager::createCommandPool(VkContext &ctx)
{
	auto indices = ctx.findQueueFamilies(ctx.getPhysicalDevice());
	VkCommandPoolCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = indices.graphicsFamily.value();
	if(vkCreateCommandPool(ctx.getDevice(), &ci, nullptr, &m_commandPool)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create command pool");
}

// ── Buffer Helpers ──────────────────────────────────────────────
void VkBufferManager::createBuffer(VkContext &ctx, VkDeviceSize size,
								   VkBufferUsageFlags usage,
								   VkMemoryPropertyFlags props,
								   VkBuffer &buffer, VkDeviceMemory &memory)
{
	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;
	ci.usage = usage;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(vkCreateBuffer(ctx.getDevice(), &ci, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to create buffer");

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(ctx.getDevice(), buffer, &memReq);
	VkMemoryAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = memReq.size;
	ai.memoryTypeIndex = ctx.findMemoryType(memReq.memoryTypeBits, props);
	if(vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate buffer memory");
	vkBindBufferMemory(ctx.getDevice(), buffer, memory, 0);
}

void VkBufferManager::copyBuffer(VkDevice device, VkQueue graphicsQueue,
								 VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandPool = m_commandPool;
	ai.commandBufferCount = 1;
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(device, &ai, &cmd);

	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);
	VkBufferCopy region{};
	region.size = size;
	vkCmdCopyBuffer(cmd, src, dst, 1, &region);
	vkEndCommandBuffer(cmd);

	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);
	vkFreeCommandBuffers(device, m_commandPool, 1, &cmd);
}

// ── Uniform Buffers ─────────────────────────────────────────────
void VkBufferManager::createUniformBuffers(VkContext &ctx)
{
	VkDeviceSize size = sizeof(UniformBufferObject);
	m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			createBuffer(ctx, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
						   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						 m_uniformBuffers[i], m_uniformBuffersMemory[i]);
			vkMapMemory(ctx.getDevice(), m_uniformBuffersMemory[i], 0, size, 0,
						&m_uniformBuffersMapped[i]);
		}
}

// ── Descriptor Pool & Sets ──────────────────────────────────────
void VkBufferManager::createDescriptorPool(VkDevice device)
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	ci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	ci.pPoolSizes = poolSizes.data();
	ci.maxSets = MAX_FRAMES_IN_FLIGHT;
	if(vkCreateDescriptorPool(device, &ci, nullptr, &m_descriptorPool)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool");
}

void VkBufferManager::createDescriptorSets(
  VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkImageView imageView, VkSampler sampler)
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
											   descriptorSetLayout);
	VkDescriptorSetAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = m_descriptorPool;
	ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	ai.pSetLayouts = layouts.data();
	m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	if(vkAllocateDescriptorSets(device, &ai, m_descriptorSets.data())
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate descriptor sets");

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VkDescriptorBufferInfo bufInfo{};
			bufInfo.buffer = m_uniformBuffers[i];
			bufInfo.offset = 0;
			bufInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = imageView;
			imageInfo.sampler = sampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = m_descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufInfo;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = m_descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
}
