// -----------------------------------------------------------------------------
// Fichier : vk_utils.h
// Rôle : Petites briques Vulkan réutilisables : images (2D, tableaux, 3D,
// mipmaps), buffers, barrières de layout, samplers.
// -----------------------------------------------------------------------------
#pragma once

#include "vk_context.h"

#include <vector>

/// Image GPU + mémoire + vue (et une vue par niveau de mip si demandé)
struct GpuImage
{
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	std::vector<VkImageView> mipViews; // une vue 2D par niveau (optionnel)
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkExtent3D extent{0, 0, 0};
	uint32_t mipLevels = 1;
	uint32_t layers = 1;
};

/// Buffer GPU + mémoire (éventuellement mappée)
struct GpuBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDeviceSize size = 0;
	void *mapped = nullptr;
};

struct ImageDesc
{
	VkImageType type = VK_IMAGE_TYPE_2D;
	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	uint32_t width = 1, height = 1, depth = 1;
	uint32_t mipLevels = 1;
	uint32_t layers = 1;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	bool perMipViews = false;
};

namespace vkutil
	{
		GpuImage createImage(VkContext &ctx, const ImageDesc &desc);
		void destroyImage(VkDevice device, GpuImage &img);

		/// Buffer en mémoire visible par le CPU (et locale au GPU si possible,
		/// cas de la mémoire unifiée des puces Apple), mappé en permanence.
		GpuBuffer createHostBuffer(VkContext &ctx, VkDeviceSize size,
								   VkBufferUsageFlags usage);
		void destroyBuffer(VkDevice device, GpuBuffer &buf);

		/// Barrière de transition de layout (tous niveaux/couches)
		void transition(VkCommandBuffer cmd, VkImage image,
						VkImageAspectFlags aspect, VkImageLayout from,
						VkImageLayout to, uint32_t mipLevels = 1,
						uint32_t layers = 1);

		/// Copie des données CPU (par niveau de mip, couches contiguës) vers
		/// une image, puis passage en SHADER_READ_ONLY_OPTIMAL.
		void uploadImage(VkContext &ctx, GpuImage &img,
						 const std::vector<std::vector<uint8_t>> &mipData);

		VkSampler createSampler(VkDevice device, VkFilter filter,
								VkSamplerMipmapMode mipMode,
								VkSamplerAddressMode addressU,
								VkSamplerAddressMode addressV,
								VkSamplerAddressMode addressW,
								float maxLod = 0.0f, float anisotropy = 0.0f,
								bool compare = false);
	}
