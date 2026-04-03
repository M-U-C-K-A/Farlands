#include "vk_pipeline.h"

#include <array>
#include <fstream>
#include <stdexcept>

// ── Public ──────────────────────────────────────────────────────
void VkPipelineManager::create(VkContext &ctx, VkSwapchain &swapchain)
{
	createRenderPass(ctx.getDevice(), swapchain.getImageFormat(),
					 ctx.getPhysicalDevice());
	createDescriptorSetLayout(ctx.getDevice());
	createGraphicsPipeline(ctx.getDevice());
}

void VkPipelineManager::cleanup(VkDevice device)
{
	vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
	vkDestroyRenderPass(device, m_renderPass, nullptr);
}

// ── File Reading ────────────────────────────────────────────────
std::vector<char> VkPipelineManager::readFile(const std::string &filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if(!file.is_open())
		throw std::runtime_error("Failed to open file: " + filename);
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	return buffer;
}

// ── Depth Format ────────────────────────────────────────────────
VkFormat
VkPipelineManager::findSupportedFormat(VkPhysicalDevice physDevice,
									   const std::vector<VkFormat> &candidates,
									   VkImageTiling tiling,
									   VkFormatFeatureFlags features)
{
	for(auto f : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physDevice, f, &props);
			if(tiling == VK_IMAGE_TILING_LINEAR
			   && (props.linearTilingFeatures & features) == features)
				return f;
			if(tiling == VK_IMAGE_TILING_OPTIMAL
			   && (props.optimalTilingFeatures & features) == features)
				return f;
		}
	throw std::runtime_error("Failed to find supported format");
}

VkFormat VkPipelineManager::findDepthFormat(VkPhysicalDevice physDevice)
{
	return findSupportedFormat(
	  physDevice,
	  {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
	   VK_FORMAT_D24_UNORM_S8_UINT},
	  VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

// ── Render Pass ─────────────────────────────────────────────────
void VkPipelineManager::createRenderPass(VkDevice device,
										 VkFormat swapchainFormat,
										 VkPhysicalDevice physDevice)
{
	VkAttachmentDescription colorAtt{};
	colorAtt.format = swapchainFormat;
	colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAtt{};
	depthAtt.format = findDepthFormat(physDevice);
	depthAtt.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef{0,
								   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depthRef{
	  1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
					   | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
					   | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
						| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = {colorAtt, depthAtt};
	VkRenderPassCreateInfo rpci{};
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = static_cast<uint32_t>(attachments.size());
	rpci.pAttachments = attachments.data();
	rpci.subpassCount = 1;
	rpci.pSubpasses = &subpass;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;
	if(vkCreateRenderPass(device, &rpci, nullptr, &m_renderPass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass");
}

// ── Descriptor Set Layout ───────────────────────────────────────
void VkPipelineManager::createDescriptorSetLayout(VkDevice device)
{
	VkDescriptorSetLayoutBinding uboBinding{};
	uboBinding.binding = 0;
	uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboBinding.descriptorCount = 1;
	uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboBinding, samplerLayoutBinding};

	VkDescriptorSetLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.bindingCount = static_cast<uint32_t>(bindings.size());
	ci.pBindings = bindings.data();
	if(vkCreateDescriptorSetLayout(device, &ci, nullptr,
								   &m_descriptorSetLayout)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor set layout");
}

// ── Shader Module ───────────────────────────────────────────────
VkShaderModule
VkPipelineManager::createShaderModule(VkDevice device,
									  const std::vector<char> &code)
{
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = code.size();
	ci.pCode = reinterpret_cast<const uint32_t *>(code.data());
	VkShaderModule mod;
	if(vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
		throw std::runtime_error("Failed to create shader module");
	return mod;
}

// ── Graphics Pipeline ───────────────────────────────────────────
void VkPipelineManager::createGraphicsPipeline(VkDevice device)
{
	auto vertCode = readFile(std::string(SHADER_DIR) + "/shader.vert.spv");
	auto fragCode = readFile(std::string(SHADER_DIR) + "/shader.frag.spv");
	VkShaderModule vertModule = createShaderModule(device, vertCode);
	VkShaderModule fragModule = createShaderModule(device, fragCode);

	VkPipelineShaderStageCreateInfo vertStage{};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = vertModule;
	vertStage.pName = "main";

	VkPipelineShaderStageCreateInfo fragStage{};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.module = fragModule;
	fragStage.pName = "main";

	VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

	auto bindingDesc = Vertex::getBindingDescription();
	auto attrDesc = Vertex::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType
	  = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &bindingDesc;
	vertexInput.vertexAttributeDescriptionCount
	  = static_cast<uint32_t>(attrDesc.size());
	vertexInput.pVertexAttributeDescriptions = attrDesc.data();

	VkPipelineInputAssemblyStateCreateInfo inputAsm{};
	inputAsm.sType
	  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType
	  = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rast{};
	rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rast.polygonMode = VK_POLYGON_MODE_FILL;
	rast.lineWidth = 1.0f;
	rast.cullMode = VK_CULL_MODE_BACK_BIT;
	rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo ms{};
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType
	  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	VkPipelineColorBlendAttachmentState colorBlendAtt{};
	colorBlendAtt.colorWriteMask = 0xF;

	VkPipelineColorBlendStateCreateInfo colorBlend{};
	colorBlend.sType
	  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &colorBlendAtt;

	std::vector<VkDynamicState> dynStates
	  = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynState{};
	dynState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynState.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
	dynState.pDynamicStates = dynStates.data();

	VkPipelineLayoutCreateInfo layoutCI{};
	layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCI.setLayoutCount = 1;
	layoutCI.pSetLayouts = &m_descriptorSetLayout;
	if(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_pipelineLayout)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipeline layout");

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.stageCount = 2;
	pipelineCI.pStages = stages;
	pipelineCI.pVertexInputState = &vertexInput;
	pipelineCI.pInputAssemblyState = &inputAsm;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pRasterizationState = &rast;
	pipelineCI.pMultisampleState = &ms;
	pipelineCI.pDepthStencilState = &depthStencil;
	pipelineCI.pColorBlendState = &colorBlend;
	pipelineCI.pDynamicState = &dynState;
	pipelineCI.layout = m_pipelineLayout;
	pipelineCI.renderPass = m_renderPass;
	pipelineCI.subpass = 0;
	if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI,
								 nullptr, &m_graphicsPipeline)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create graphics pipeline");

	vkDestroyShaderModule(device, fragModule, nullptr);
	vkDestroyShaderModule(device, vertModule, nullptr);
}
