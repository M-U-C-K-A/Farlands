#include "vk_utils.h"

#include <cstring>
#include <stdexcept>

namespace vkutil
	{
		GpuImage createImage(VkContext &ctx, const ImageDesc &desc)
		{
			VkDevice device = ctx.getDevice();
			GpuImage img;
			img.format = desc.format;
			img.extent = {desc.width, desc.height, desc.depth};
			img.mipLevels = desc.mipLevels;
			img.layers = desc.layers;

			VkImageCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			ci.imageType = desc.type;
			ci.extent = img.extent;
			ci.mipLevels = desc.mipLevels;
			ci.arrayLayers = desc.layers;
			ci.format = desc.format;
			ci.tiling = VK_IMAGE_TILING_OPTIMAL;
			ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			ci.usage = desc.usage;
			ci.samples = VK_SAMPLE_COUNT_1_BIT;
			ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			if(vkCreateImage(device, &ci, nullptr, &img.image) != VK_SUCCESS)
				throw std::runtime_error("Failed to create image");

			VkMemoryRequirements req;
			vkGetImageMemoryRequirements(device, img.image, &req);
			VkMemoryAllocateInfo ai{};
			ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			ai.allocationSize = req.size;
			ai.memoryTypeIndex = ctx.findMemoryType(
			  req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if(vkAllocateMemory(device, &ai, nullptr, &img.memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate image memory");
			vkBindImageMemory(device, img.image, img.memory, 0);

			VkImageViewCreateInfo vi{};
			vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			vi.image = img.image;
			vi.viewType = desc.viewType;
			vi.format = desc.format;
			vi.subresourceRange = {desc.aspect, 0, desc.mipLevels, 0,
								   desc.layers};
			if(vkCreateImageView(device, &vi, nullptr, &img.view) != VK_SUCCESS)
				throw std::runtime_error("Failed to create image view");

			if(desc.perMipViews)
				{
					for(uint32_t m = 0; m < desc.mipLevels; ++m)
						{
							vi.subresourceRange = {desc.aspect, m, 1, 0, 1};
							vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
							VkImageView v;
							if(vkCreateImageView(device, &vi, nullptr, &v)
							   != VK_SUCCESS)
								throw std::runtime_error(
								  "Failed to create mip image view");
							img.mipViews.push_back(v);
						}
				}
			return img;
		}

		void destroyImage(VkDevice device, GpuImage &img)
		{
			for(auto v : img.mipViews)
				vkDestroyImageView(device, v, nullptr);
			img.mipViews.clear();
			if(img.view)
				vkDestroyImageView(device, img.view, nullptr);
			if(img.image)
				vkDestroyImage(device, img.image, nullptr);
			if(img.memory)
				vkFreeMemory(device, img.memory, nullptr);
			img = GpuImage{};
		}

		GpuBuffer createHostBuffer(VkContext &ctx, VkDeviceSize size,
								   VkBufferUsageFlags usage)
		{
			VkDevice device = ctx.getDevice();
			GpuBuffer buf;
			buf.size = size;

			VkBufferCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			ci.size = size;
			ci.usage = usage;
			ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			if(vkCreateBuffer(device, &ci, nullptr, &buf.buffer) != VK_SUCCESS)
				throw std::runtime_error("Failed to create buffer");

			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(device, buf.buffer, &req);
			const VkMemoryPropertyFlags hostFlags
			  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			auto type = ctx.tryFindMemoryType(
			  req.memoryTypeBits, hostFlags | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if(!type)
				type = ctx.findMemoryType(req.memoryTypeBits, hostFlags);

			VkMemoryAllocateInfo ai{};
			ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			ai.allocationSize = req.size;
			ai.memoryTypeIndex = *type;
			if(vkAllocateMemory(device, &ai, nullptr, &buf.memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate buffer memory");
			vkBindBufferMemory(device, buf.buffer, buf.memory, 0);
			vkMapMemory(device, buf.memory, 0, size, 0, &buf.mapped);
			return buf;
		}

		void destroyBuffer(VkDevice device, GpuBuffer &buf)
		{
			if(buf.mapped)
				vkUnmapMemory(device, buf.memory);
			if(buf.buffer)
				vkDestroyBuffer(device, buf.buffer, nullptr);
			if(buf.memory)
				vkFreeMemory(device, buf.memory, nullptr);
			buf = GpuBuffer{};
		}

		void transition(VkCommandBuffer cmd, VkImage image,
						VkImageAspectFlags aspect, VkImageLayout from,
						VkImageLayout to, uint32_t mipLevels, uint32_t layers)
		{
			VkImageMemoryBarrier b{};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			b.oldLayout = from;
			b.newLayout = to;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.image = image;
			b.subresourceRange = {aspect, 0, mipLevels, 0, layers};
			b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
			if(from == VK_IMAGE_LAYOUT_UNDEFINED)
				b.srcAccessMask = 0;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
								 nullptr, 0, nullptr, 1, &b);
		}

		void uploadImage(VkContext &ctx, GpuImage &img,
						 const std::vector<std::vector<uint8_t>> &mipData)
		{
			VkDeviceSize total = 0;
			for(auto &m : mipData)
				total += m.size();

			GpuBuffer staging
			  = createHostBuffer(ctx, total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
			std::vector<VkBufferImageCopy> regions;
			VkDeviceSize offset = 0;
			for(uint32_t m = 0; m < mipData.size(); ++m)
				{
					std::memcpy(static_cast<uint8_t *>(staging.mapped) + offset,
								mipData[m].data(), mipData[m].size());
					VkBufferImageCopy r{};
					r.bufferOffset = offset;
					r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m, 0,
										  img.layers};
					r.imageExtent = {std::max(1u, img.extent.width >> m),
									 std::max(1u, img.extent.height >> m),
									 std::max(1u, img.extent.depth >> m)};
					regions.push_back(r);
					offset += mipData[m].size();
				}

			ctx.immediateSubmit([&](VkCommandBuffer cmd) {
				transition(cmd, img.image, VK_IMAGE_ASPECT_COLOR_BIT,
						   VK_IMAGE_LAYOUT_UNDEFINED,
						   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, img.mipLevels,
						   img.layers);
				vkCmdCopyBufferToImage(cmd, staging.buffer, img.image,
									   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									   static_cast<uint32_t>(regions.size()),
									   regions.data());
				transition(cmd, img.image, VK_IMAGE_ASPECT_COLOR_BIT,
						   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						   img.mipLevels, img.layers);
			});
			destroyBuffer(ctx.getDevice(), staging);
		}

		VkSampler createSampler(VkDevice device, VkFilter filter,
								VkSamplerMipmapMode mipMode,
								VkSamplerAddressMode addressU,
								VkSamplerAddressMode addressV,
								VkSamplerAddressMode addressW, float maxLod,
								float anisotropy, bool compare)
		{
			VkSamplerCreateInfo si{};
			si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			si.magFilter = filter;
			si.minFilter = filter;
			si.mipmapMode = mipMode;
			si.addressModeU = addressU;
			si.addressModeV = addressV;
			si.addressModeW = addressW;
			si.minLod = 0.0f;
			si.maxLod = maxLod;
			si.anisotropyEnable = anisotropy > 1.0f ? VK_TRUE : VK_FALSE;
			si.maxAnisotropy = anisotropy > 1.0f ? anisotropy : 1.0f;
			si.compareEnable = compare ? VK_TRUE : VK_FALSE;
			si.compareOp = compare ? VK_COMPARE_OP_LESS_OR_EQUAL
								   : VK_COMPARE_OP_ALWAYS;
			si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			VkSampler s;
			if(vkCreateSampler(device, &si, nullptr, &s) != VK_SUCCESS)
				throw std::runtime_error("Failed to create sampler");
			return s;
		}
	}
