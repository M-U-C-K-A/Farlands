#include "pipelines.h"

#include "../core/types.h"

#include <array>
#include <fstream>
#include <stdexcept>

namespace
	{
		std::vector<char> readFile(const std::string &filename)
		{
			std::ifstream file(filename, std::ios::ate | std::ios::binary);
			if(!file.is_open())
				throw std::runtime_error("Failed to open file: " + filename);
			size_t fileSize = static_cast<size_t>(file.tellg());
			std::vector<char> buffer(fileSize);
			file.seekg(0);
			file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
			return buffer;
		}

		VkShaderModule loadShader(VkDevice device, const std::string &name)
		{
			auto code = readFile(std::string(SHADER_DIR) + "/" + name);
			VkShaderModuleCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			ci.codeSize = code.size();
			ci.pCode = reinterpret_cast<const uint32_t *>(code.data());
			VkShaderModule mod;
			if(vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
				throw std::runtime_error("Failed to create shader module: " + name);
			return mod;
		}
	}

namespace pipelines
	{
		VkRenderPass createRenderPass(VkDevice device,
									  const std::vector<AttachmentDesc> &colors,
									  const AttachmentDesc *depth)
		{
			std::vector<VkAttachmentDescription> attachments;
			std::vector<VkAttachmentReference> colorRefs;
			for(const auto &c : colors)
				{
					VkAttachmentDescription a{};
					a.format = c.format;
					a.samples = VK_SAMPLE_COUNT_1_BIT;
					a.loadOp = c.load;
					a.storeOp = c.store;
					a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					a.initialLayout = c.initialLayout;
					a.finalLayout = c.finalLayout;
					colorRefs.push_back({static_cast<uint32_t>(attachments.size()),
										 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
					attachments.push_back(a);
				}
			VkAttachmentReference depthRef{};
			if(depth)
				{
					VkAttachmentDescription a{};
					a.format = depth->format;
					a.samples = VK_SAMPLE_COUNT_1_BIT;
					a.loadOp = depth->load;
					a.storeOp = depth->store;
					a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					a.initialLayout = depth->initialLayout;
					a.finalLayout = depth->finalLayout;
					depthRef = {static_cast<uint32_t>(attachments.size()),
								VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
					attachments.push_back(a);
				}

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
			subpass.pColorAttachments = colorRefs.data();
			subpass.pDepthStencilAttachment = depth ? &depthRef : nullptr;

			const VkPipelineStageFlags stages
			  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
				| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				| VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			const VkAccessFlags writes
			  = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
				| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			const VkAccessFlags all
			  = writes | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
				| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
				| VK_ACCESS_SHADER_READ_BIT;

			std::array<VkSubpassDependency, 2> deps{};
			deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			deps[0].dstSubpass = 0;
			deps[0].srcStageMask = stages;
			deps[0].srcAccessMask = writes;
			deps[0].dstStageMask = stages;
			deps[0].dstAccessMask = all;
			deps[1].srcSubpass = 0;
			deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			deps[1].srcStageMask = stages;
			deps[1].srcAccessMask = writes;
			deps[1].dstStageMask = stages;
			deps[1].dstAccessMask = all;

			VkRenderPassCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			ci.attachmentCount = static_cast<uint32_t>(attachments.size());
			ci.pAttachments = attachments.data();
			ci.subpassCount = 1;
			ci.pSubpasses = &subpass;
			ci.dependencyCount = static_cast<uint32_t>(deps.size());
			ci.pDependencies = deps.data();
			VkRenderPass rp;
			if(vkCreateRenderPass(device, &ci, nullptr, &rp) != VK_SUCCESS)
				throw std::runtime_error("Failed to create render pass");
			return rp;
		}

		VkPipeline createPipeline(VkDevice device, VkPipelineLayout layout,
								  const PipelineDesc &desc)
		{
			VkShaderModule vert = loadShader(device, desc.vert);
			// Pas de fragment shader : pipeline de profondeur seule
			VkShaderModule frag = desc.frag.empty() ? VK_NULL_HANDLE
													: loadShader(device, desc.frag);

			VkPipelineShaderStageCreateInfo stages[2]{};
			stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			stages[0].module = vert;
			stages[0].pName = "main";
			stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			stages[1].module = frag;
			stages[1].pName = "main";

			auto binding = Vertex::getBindingDescription();
			auto attrs = Vertex::getAttributeDescriptions();
			VkPipelineVertexInputStateCreateInfo vertexInput{};
			vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			if(desc.chunkVertices)
				{
					vertexInput.vertexBindingDescriptionCount = 1;
					vertexInput.pVertexBindingDescriptions = &binding;
					vertexInput.vertexAttributeDescriptionCount
					  = static_cast<uint32_t>(attrs.size());
					vertexInput.pVertexAttributeDescriptions = attrs.data();
				}
			// Particules : un quad (6 sommets générés) par instance de 32 octets
			VkVertexInputBindingDescription instBinding{0, 32, VK_VERTEX_INPUT_RATE_INSTANCE};
			VkVertexInputAttributeDescription instAttrs[3] = {
			  {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
			  {1, 0, VK_FORMAT_R8G8B8A8_UNORM, 16},
			  {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 20}};
			VkVertexInputBindingDescription lodBinding{0, 20, VK_VERTEX_INPUT_RATE_VERTEX};
			VkVertexInputAttributeDescription lodAttrs[3] = {
			  {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
			  {1, 0, VK_FORMAT_R8G8B8A8_SNORM, 12},
			  {2, 0, VK_FORMAT_R8G8B8A8_UNORM, 16}};
			if(desc.lodVertices)
				{
					vertexInput.vertexBindingDescriptionCount = 1;
					vertexInput.pVertexBindingDescriptions = &lodBinding;
					vertexInput.vertexAttributeDescriptionCount = 3;
					vertexInput.pVertexAttributeDescriptions = lodAttrs;
				}
			if(desc.particleInstances)
				{
					vertexInput.vertexBindingDescriptionCount = 1;
					vertexInput.pVertexBindingDescriptions = &instBinding;
					vertexInput.vertexAttributeDescriptionCount = 3;
					vertexInput.pVertexAttributeDescriptions = instAttrs;
				}

			VkPipelineInputAssemblyStateCreateInfo inputAsm{};
			inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			VkPipelineViewportStateCreateInfo viewportState{};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			VkPipelineRasterizationStateCreateInfo rast{};
			rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rast.polygonMode = VK_POLYGON_MODE_FILL;
			rast.lineWidth = 1.0f;
			rast.cullMode = desc.cull;
			rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rast.depthBiasEnable = desc.depthBias ? VK_TRUE : VK_FALSE;
			rast.depthBiasConstantFactor = desc.depthBiasConstant;
			rast.depthBiasSlopeFactor = desc.depthBiasSlope;

			VkPipelineMultisampleStateCreateInfo ms{};
			ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineDepthStencilStateCreateInfo depth{};
			depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depth.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
			depth.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
			depth.depthCompareOp = desc.depthOp;

			std::vector<VkPipelineColorBlendAttachmentState> blends(desc.colorCount);
			for(auto &b : blends)
				{
					b.colorWriteMask = 0xF;
					b.blendEnable = desc.blend != BlendMode::None ? VK_TRUE : VK_FALSE;
					if(desc.blend == BlendMode::Alpha)
						{
							b.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
							b.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
						}
					else
						{
							b.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
							b.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
						}
					b.colorBlendOp = VK_BLEND_OP_ADD;
					b.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
					b.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
					b.alphaBlendOp = VK_BLEND_OP_ADD;
				}
			VkPipelineColorBlendStateCreateInfo colorBlend{};
			colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlend.attachmentCount = desc.colorCount;
			colorBlend.pAttachments = blends.data();

			VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
										  VK_DYNAMIC_STATE_SCISSOR};
			VkPipelineDynamicStateCreateInfo dynState{};
			dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynState.dynamicStateCount = 2;
			dynState.pDynamicStates = dynStates;

			VkGraphicsPipelineCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			ci.stageCount = frag ? 2 : 1;
			ci.pStages = stages;
			ci.pVertexInputState = &vertexInput;
			ci.pInputAssemblyState = &inputAsm;
			ci.pViewportState = &viewportState;
			ci.pRasterizationState = &rast;
			ci.pMultisampleState = &ms;
			ci.pDepthStencilState = &depth;
			ci.pColorBlendState = &colorBlend;
			ci.pDynamicState = &dynState;
			ci.layout = layout;
			ci.renderPass = desc.renderPass;
			ci.subpass = 0;

			VkPipeline pipeline;
			VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci,
													 nullptr, &pipeline);
			if(frag)
				vkDestroyShaderModule(device, frag, nullptr);
			vkDestroyShaderModule(device, vert, nullptr);
			if(res != VK_SUCCESS)
				throw std::runtime_error("Failed to create pipeline " + desc.vert
										 + " / " + desc.frag);
			return pipeline;
		}
	}
