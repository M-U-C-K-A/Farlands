#include "renderer.h"

#include "../world/block.h"
#include "atmosphere.h"
#include "core/logger.h"
#include "pipelines.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../core/stb_image_write.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace
	{
		constexpr VkFormat HDR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
		constexpr VkFormat DEPTH_LINEAR_FORMAT = VK_FORMAT_R32_SFLOAT;

		// Éclairement solaire hors atmosphère, dans les mêmes unités que le
		// modèle de ciel (shaders/atmosphere.glsl : SUN_INTENSITY)
		constexpr float SUN_IRRADIANCE = 22.0f;

		// Profondeur couverte par la shadow map de part et d'autre de la
		// caméra, le long de la direction du soleil (blocs)
		constexpr float SHADOW_DEPTH_RANGE = 300.0f;

		// Bindings du set global (voir shaders/common.glsl)
		enum Binding : uint32_t
		{
			B_UBO = 0,
			B_BLOCK_ALBEDO = 1,
			B_BLOCK_NORMAL = 2,
			B_SHADOW_MAP = 3,
			B_SKY_LUT = 4,
			B_SKY_ENV = 5,
			B_CLOUD_NOISE = 6,
			B_SCENE_COLOR = 7,
			B_LINEAR_DEPTH = 8,
			B_HDR_COLOR = 9,
			B_BLOOM = 10,
			B_CLOUDS = 11,
			B_COUNT = 12
		};

		VkFormat findDepthFormat(VkContext &ctx, VkFormatFeatureFlags extra)
		{
			for(VkFormat f : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
							  VK_FORMAT_D24_UNORM_S8_UINT})
				if(ctx.formatSupports(
					 f, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | extra))
					return f;
			throw std::runtime_error("No supported depth format");
		}

		VkFramebuffer makeFramebuffer(VkDevice device, VkRenderPass rp,
									  const std::vector<VkImageView> &views,
									  uint32_t w, uint32_t h)
		{
			VkFramebufferCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			ci.renderPass = rp;
			ci.attachmentCount = static_cast<uint32_t>(views.size());
			ci.pAttachments = views.data();
			ci.width = w;
			ci.height = h;
			ci.layers = 1;
			VkFramebuffer fb;
			if(vkCreateFramebuffer(device, &ci, nullptr, &fb) != VK_SUCCESS)
				throw std::runtime_error("Failed to create framebuffer");
			return fb;
		}

		VkClearValue clearColor(float r, float g, float b, float a)
		{
			VkClearValue v{};
			v.color = {{r, g, b, a}};
			return v;
		}
		VkClearValue clearDepth(float d)
		{
			VkClearValue v{};
			v.depthStencil = {d, 0};
			return v;
		}
	}

// ═════════════════════════════════════════════════════════════════
// Initialisation
// ═════════════════════════════════════════════════════════════════
void Renderer::init(GLFWwindow *window, const RenderSettings &settings)
{
	m_window = window;
	m_vsync = settings.vsync;
	m_renderScale = settings.renderScale;

	m_context.init(window);
	m_swapchain.create(m_context, window, m_vsync);

	createRenderPasses();
	m_swapchain.createFramebuffers(m_context.getDevice(), m_rpFinal);
	createSamplers(); // avant les layouts : sampler de comparaison immuable
	createLayouts();
	createPipelines();
	createStaticResources();
	createShadowMap(settings.shadowResolution);
	createRenderTargets();
	createDescriptors();
	writeDescriptors();
	createCommandsAndSync();
	initImGui();
}

void Renderer::createRenderPasses()
{
	VkDevice device = m_context.getDevice();
	m_depthFormat = findDepthFormat(m_context, 0);
	VkFormat shadowFormat
	  = findDepthFormat(m_context, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	if(shadowFormat != m_depthFormat)
		LOG_WARNING("Shadow map uses a different depth format");

	const auto RO = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	const auto UNDEF = VK_IMAGE_LAYOUT_UNDEFINED;
	const auto DONT = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	const auto CLEAR = VK_ATTACHMENT_LOAD_OP_CLEAR;
	const auto LOAD = VK_ATTACHMENT_LOAD_OP_LOAD;
	const auto STORE = VK_ATTACHMENT_STORE_OP_STORE;
	const auto NOSTORE = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	const auto DEPTH_ATT = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	m_rpSky = pipelines::createRenderPass(device, {{HDR_FORMAT, DONT, STORE, UNDEF, RO}},
										  nullptr);

	AttachmentDesc shadowDepth{shadowFormat, CLEAR, STORE, UNDEF,
							   VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
	m_rpShadow = pipelines::createRenderPass(device, {}, &shadowDepth);

	AttachmentDesc sceneDepth{m_depthFormat, CLEAR, STORE, UNDEF, DEPTH_ATT};
	m_rpScene = pipelines::createRenderPass(
	  device,
	  {{HDR_FORMAT, CLEAR, STORE, UNDEF, RO},
	   {DEPTH_LINEAR_FORMAT, CLEAR, STORE, UNDEF, RO}},
	  &sceneDepth);

	AttachmentDesc waterDepth{m_depthFormat, LOAD, NOSTORE, DEPTH_ATT, DEPTH_ATT};
	m_rpWater = pipelines::createRenderPass(
	  device, {{HDR_FORMAT, DONT, STORE, UNDEF, RO}}, &waterDepth);

	m_rpBloomDown = pipelines::createRenderPass(
	  device, {{HDR_FORMAT, DONT, STORE, UNDEF, RO}}, nullptr);
	m_rpBloomUp = pipelines::createRenderPass(
	  device, {{HDR_FORMAT, LOAD, STORE, RO, RO}}, nullptr);

	m_rpFinal = pipelines::createRenderPass(
	  device,
	  {{m_swapchain.getImageFormat(), CLEAR, STORE, UNDEF,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}},
	  nullptr);
}

void Renderer::createLayouts()
{
	VkDevice device = m_context.getDevice();

	std::vector<VkDescriptorSetLayoutBinding> bindings;
	for(uint32_t b = 0; b < B_COUNT; ++b)
		{
			VkDescriptorSetLayoutBinding lb{};
			lb.binding = b;
			lb.descriptorType = b == B_UBO
								  ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
								  : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			lb.descriptorCount = 1;
			lb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			// MoltenVK (portability) : les samplers de comparaison doivent
			// être immuables dans le layout
			if(b == B_SHADOW_MAP)
				lb.pImmutableSamplers = &m_samplerShadow;
			bindings.push_back(lb);
		}
	VkDescriptorSetLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.bindingCount = static_cast<uint32_t>(bindings.size());
	ci.pBindings = bindings.data();
	if(vkCreateDescriptorSetLayout(device, &ci, nullptr, &m_globalLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create global descriptor layout");

	VkDescriptorSetLayoutBinding tex{};
	tex.binding = 0;
	tex.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	tex.descriptorCount = 1;
	tex.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	ci.bindingCount = 1;
	ci.pBindings = &tex;
	if(vkCreateDescriptorSetLayout(device, &ci, nullptr, &m_textureLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create texture descriptor layout");

	VkPushConstantRange push{};
	push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	push.size = sizeof(PushConstants);
	VkDescriptorSetLayout sets[] = {m_globalLayout, m_textureLayout};
	VkPipelineLayoutCreateInfo pl{};
	pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl.setLayoutCount = 2;
	pl.pSetLayouts = sets;
	pl.pushConstantRangeCount = 1;
	pl.pPushConstantRanges = &push;
	if(vkCreatePipelineLayout(device, &pl, nullptr, &m_pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipeline layout");
}

void Renderer::createPipelines()
{
	VkDevice device = m_context.getDevice();
	auto make = [&](PipelineDesc d) {
		return pipelines::createPipeline(device, m_pipelineLayout, d);
	};

	PipelineDesc d;
	d.vert = "fullscreen.vert.spv";
	d.renderPass = m_rpSky;
	d.frag = "skylut.frag.spv";
	m_pipe.skyLut = make(d);
	d.frag = "skyenv.frag.spv";
	m_pipe.skyEnv = make(d);
	d.frag = "clouds.frag.spv";
	m_pipe.clouds = make(d);

	PipelineDesc s;
	s.vert = "shadow.vert.spv";
	s.frag = "shadow.frag.spv";
	s.renderPass = m_rpShadow;
	s.colorCount = 0;
	s.chunkVertices = true;
	s.cull = VK_CULL_MODE_NONE;
	s.depthTest = s.depthWrite = true;
	s.depthOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	s.depthBias = true;
	s.depthBiasConstant = 1.0f;
	s.depthBiasSlope = 1.75f;
	m_pipe.shadowCutout = make(s);
	s.frag.clear(); // blocs opaques : profondeur seule
	m_pipe.shadow = make(s);

	PipelineDesc t;
	t.vert = "terrain.vert.spv";
	t.frag = "terrain.frag.spv";
	t.renderPass = m_rpScene;
	t.colorCount = 2;
	t.chunkVertices = true;
	t.cull = VK_CULL_MODE_BACK_BIT;
	t.depthTest = t.depthWrite = true;
	t.depthOp = VK_COMPARE_OP_GREATER; // reverse-Z
	m_pipe.terrain = make(t);
	t.frag = "terrain_cutout.frag.spv";
	m_pipe.terrainCutout = make(t);

	PipelineDesc sky;
	sky.vert = "fullscreen.vert.spv";
	sky.frag = "sky.frag.spv";
	sky.renderPass = m_rpScene;
	sky.colorCount = 2;
	sky.depthTest = true; // uniquement là où rien n'a été dessiné (z = 0)
	sky.depthOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	m_pipe.sky = make(sky);

	PipelineDesc blit;
	blit.vert = "fullscreen.vert.spv";
	blit.frag = "blit.frag.spv";
	blit.renderPass = m_rpWater;
	m_pipe.blit = make(blit);

	PipelineDesc w;
	w.vert = "water.vert.spv";
	w.frag = "water.frag.spv";
	w.renderPass = m_rpWater;
	w.chunkVertices = true;
	w.cull = VK_CULL_MODE_NONE; // surface visible aussi depuis le dessous
	w.depthTest = w.depthWrite = true;
	w.depthOp = VK_COMPARE_OP_GREATER;
	m_pipe.water = make(w);

	PipelineDesc bd;
	bd.vert = "fullscreen.vert.spv";
	bd.frag = "bloom_down.frag.spv";
	bd.renderPass = m_rpBloomDown;
	m_pipe.bloomDown = make(bd);
	PipelineDesc bu = bd;
	bu.frag = "bloom_up.frag.spv";
	bu.renderPass = m_rpBloomUp;
	bu.blend = BlendMode::Additive;
	m_pipe.bloomUp = make(bu);

	PipelineDesc c;
	c.vert = "fullscreen.vert.spv";
	c.frag = "composite.frag.spv";
	c.renderPass = m_rpFinal;
	m_pipe.composite = make(c);
}

void Renderer::createSamplers()
{
	VkDevice device = m_context.getDevice();
	const auto REPEAT = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	const auto CLAMP = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	const auto BORDER = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	float aniso = m_context.hasAnisotropy()
					? std::min(8.0f, m_context.getProperties().limits.maxSamplerAnisotropy)
					: 0.0f;

	// Blocs : pixels nets de près, mipmaps + anisotropie au loin
	m_samplerBlocks = vkutil::createSampler(device, VK_FILTER_NEAREST,
											VK_SAMPLER_MIPMAP_MODE_LINEAR, REPEAT,
											REPEAT, REPEAT, 16.0f, aniso);
	m_samplerLinearClamp = vkutil::createSampler(device, VK_FILTER_LINEAR,
												 VK_SAMPLER_MIPMAP_MODE_LINEAR,
												 CLAMP, CLAMP, CLAMP, 16.0f);
	m_samplerLinearRepeat = vkutil::createSampler(device, VK_FILTER_LINEAR,
												  VK_SAMPLER_MIPMAP_MODE_LINEAR,
												  REPEAT, REPEAT, REPEAT, 16.0f);
	m_samplerSky = vkutil::createSampler(device, VK_FILTER_LINEAR,
										 VK_SAMPLER_MIPMAP_MODE_NEAREST, REPEAT,
										 CLAMP, CLAMP);
	m_samplerNearestClamp = vkutil::createSampler(device, VK_FILTER_NEAREST,
												  VK_SAMPLER_MIPMAP_MODE_NEAREST,
												  CLAMP, CLAMP, CLAMP);
	bool linearShadow = m_context.formatSupports(
	  m_depthFormat, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
	m_samplerShadow = vkutil::createSampler(
	  device, linearShadow ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
	  VK_SAMPLER_MIPMAP_MODE_NEAREST, BORDER, BORDER, BORDER, 0.0f, 0.0f, true);
}

void Renderer::createStaticResources()
{
	std::vector<std::string> absPaths;
	for(const auto &p : BlockDatabase::GetTexturePaths())
		absPaths.push_back(p.empty() ? "" : std::string(ASSETS_DIR) + "/" + p);
	m_blockTex = textures::loadBlockTextures(m_context, absPaths);
	m_cloudNoise = textures::createCloudNoise(m_context);
	m_panorama = textures::loadTexture2D(
	  m_context,
	  std::string(ASSETS_DIR) + "/textures/gui/title/background/panorama_0.png");
	m_logo = textures::loadTexture2D(m_context,
									 std::string(ASSETS_DIR) + "/Farlands.png");

	VkDevice device = m_context.getDevice();
	ImageDesc d;
	d.format = HDR_FORMAT;
	d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	d.width = SKY_LUT_W;
	d.height = SKY_LUT_H;
	m_skyLut = vkutil::createImage(m_context, d);
	d.width = SKY_ENV_W;
	d.height = SKY_ENV_H;
	m_skyEnv = vkutil::createImage(m_context, d);
	m_fbSkyLut = makeFramebuffer(device, m_rpSky, {m_skyLut.view}, SKY_LUT_W, SKY_LUT_H);
	m_fbSkyEnv = makeFramebuffer(device, m_rpSky, {m_skyEnv.view}, SKY_ENV_W, SKY_ENV_H);
}

void Renderer::createShadowMap(int resolution)
{
	m_shadowRes = std::clamp(resolution, 512, 8192);
	ImageDesc d;
	d.format = findDepthFormat(m_context, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	d.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	d.width = d.height = static_cast<uint32_t>(m_shadowRes);
	d.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	m_shadowMap = vkutil::createImage(m_context, d);
	m_fbShadow = makeFramebuffer(m_context.getDevice(), m_rpShadow, {m_shadowMap.view},
								 d.width, d.height);
}

void Renderer::destroyShadowMap()
{
	vkDestroyFramebuffer(m_context.getDevice(), m_fbShadow, nullptr);
	vkutil::destroyImage(m_context.getDevice(), m_shadowMap);
}

void Renderer::createRenderTargets()
{
	VkDevice device = m_context.getDevice();
	VkExtent2D sc = m_swapchain.getExtent();
	m_renderExtent.width = std::max(64u, static_cast<uint32_t>(sc.width * m_renderScale));
	m_renderExtent.height = std::max(64u, static_cast<uint32_t>(sc.height * m_renderScale));
	uint32_t w = m_renderExtent.width, h = m_renderExtent.height;

	ImageDesc d;
	d.width = w;
	d.height = h;
	d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	d.format = HDR_FORMAT;
	m_sceneColor = vkutil::createImage(m_context, d);
	m_hdrColor = vkutil::createImage(m_context, d);
	d.format = DEPTH_LINEAR_FORMAT;
	m_linearDepth = vkutil::createImage(m_context, d);

	ImageDesc dd;
	dd.width = w;
	dd.height = h;
	dd.format = m_depthFormat;
	dd.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	dd.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	m_sceneDepth = vkutil::createImage(m_context, dd);

	ImageDesc bd;
	bd.format = HDR_FORMAT;
	bd.width = std::max(1u, w / 2);
	bd.height = std::max(1u, h / 2);
	bd.mipLevels = BLOOM_MIPS;
	bd.perMipViews = true;
	bd.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	m_bloom = vkutil::createImage(m_context, bd);

	ImageDesc cd;
	cd.format = HDR_FORMAT;
	cd.width = std::max(1u, w / 2);
	cd.height = std::max(1u, h / 2);
	cd.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	m_cloudTex = vkutil::createImage(m_context, cd);
	m_fbClouds = makeFramebuffer(device, m_rpSky, {m_cloudTex.view}, cd.width, cd.height);

	m_fbScene = makeFramebuffer(device, m_rpScene,
								{m_sceneColor.view, m_linearDepth.view, m_sceneDepth.view},
								w, h);
	m_fbWater = makeFramebuffer(device, m_rpWater, {m_hdrColor.view, m_sceneDepth.view}, w, h);
	m_fbBloom.clear();
	for(uint32_t m = 0; m < BLOOM_MIPS; ++m)
		m_fbBloom.push_back(makeFramebuffer(device, m_rpBloomDown, {m_bloom.mipViews[m]},
											std::max(1u, bd.width >> m),
											std::max(1u, bd.height >> m)));
}

void Renderer::destroyRenderTargets()
{
	VkDevice device = m_context.getDevice();
	vkDestroyFramebuffer(device, m_fbClouds, nullptr);
	vkutil::destroyImage(device, m_cloudTex);
	vkDestroyFramebuffer(device, m_fbScene, nullptr);
	vkDestroyFramebuffer(device, m_fbWater, nullptr);
	for(auto fb : m_fbBloom)
		vkDestroyFramebuffer(device, fb, nullptr);
	m_fbBloom.clear();
	vkutil::destroyImage(device, m_sceneColor);
	vkutil::destroyImage(device, m_hdrColor);
	vkutil::destroyImage(device, m_linearDepth);
	vkutil::destroyImage(device, m_sceneDepth);
	vkutil::destroyImage(device, m_bloom);
}

void Renderer::createDescriptors()
{
	VkDevice device = m_context.getDevice();
	std::array<VkDescriptorPoolSize, 2> sizes{};
	sizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT};
	sizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				MAX_FRAMES_IN_FLIGHT * B_COUNT + BLOOM_MIPS + 1};
	VkDescriptorPoolCreateInfo pi{};
	pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pi.poolSizeCount = static_cast<uint32_t>(sizes.size());
	pi.pPoolSizes = sizes.data();
	pi.maxSets = MAX_FRAMES_IN_FLIGHT + BLOOM_MIPS + 1;
	if(vkCreateDescriptorPool(device, &pi, nullptr, &m_descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool");

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		m_uniforms[i] = vkutil::createHostBuffer(m_context, sizeof(FrameUniforms),
												 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_globalLayout);
	VkDescriptorSetAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = m_descriptorPool;
	ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	ai.pSetLayouts = layouts.data();
	if(vkAllocateDescriptorSets(device, &ai, m_globalSets.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate global descriptor sets");

	std::vector<VkDescriptorSetLayout> texLayouts(BLOOM_MIPS + 1, m_textureLayout);
	std::vector<VkDescriptorSet> texSets(BLOOM_MIPS + 1);
	ai.descriptorSetCount = BLOOM_MIPS + 1;
	ai.pSetLayouts = texLayouts.data();
	if(vkAllocateDescriptorSets(device, &ai, texSets.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate texture descriptor sets");
	m_setHdr = texSets[0];
	m_setBloomMips.assign(texSets.begin() + 1, texSets.end());
}

void Renderer::writeDescriptors()
{
	VkDevice device = m_context.getDevice();
	const auto RO = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	struct ImageBinding
	{
		uint32_t binding;
		VkSampler sampler;
		VkImageView view;
		VkImageLayout layout;
	};
	std::vector<ImageBinding> images = {
	  {B_BLOCK_ALBEDO, m_samplerBlocks, m_blockTex.albedo.view, RO},
	  {B_BLOCK_NORMAL, m_samplerBlocks, m_blockTex.normal.view, RO},
	  {B_SHADOW_MAP, m_samplerShadow, m_shadowMap.view,
	   VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
	  {B_SKY_LUT, m_samplerSky, m_skyLut.view, RO},
	  {B_SKY_ENV, m_samplerSky, m_skyEnv.view, RO},
	  {B_CLOUD_NOISE, m_samplerLinearRepeat, m_cloudNoise.view, RO},
	  {B_SCENE_COLOR, m_samplerLinearClamp, m_sceneColor.view, RO},
	  {B_LINEAR_DEPTH, m_samplerNearestClamp, m_linearDepth.view, RO},
	  {B_HDR_COLOR, m_samplerLinearClamp, m_hdrColor.view, RO},
	  {B_BLOOM, m_samplerLinearClamp, m_bloom.mipViews[0], RO},
	  {B_CLOUDS, m_samplerLinearClamp, m_cloudTex.view, RO},
	};

	for(int f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f)
		{
			std::vector<VkWriteDescriptorSet> writes;
			std::vector<VkDescriptorImageInfo> infos(images.size());
			VkDescriptorBufferInfo ubo{m_uniforms[f].buffer, 0, sizeof(FrameUniforms)};

			VkWriteDescriptorSet w{};
			w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w.dstSet = m_globalSets[f];
			w.dstBinding = B_UBO;
			w.descriptorCount = 1;
			w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			w.pBufferInfo = &ubo;
			writes.push_back(w);

			for(size_t i = 0; i < images.size(); ++i)
				{
					infos[i] = {images[i].sampler, images[i].view, images[i].layout};
					VkWriteDescriptorSet iw{};
					iw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					iw.dstSet = m_globalSets[f];
					iw.dstBinding = images[i].binding;
					iw.descriptorCount = 1;
					iw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					iw.pImageInfo = &infos[i];
					writes.push_back(iw);
				}
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
								   writes.data(), 0, nullptr);
		}

	// Sources des passes de bloom
	std::vector<VkDescriptorImageInfo> infos;
	infos.push_back({m_samplerLinearClamp, m_hdrColor.view, RO});
	for(uint32_t m = 0; m < BLOOM_MIPS; ++m)
		infos.push_back({m_samplerLinearClamp, m_bloom.mipViews[m], RO});
	std::vector<VkWriteDescriptorSet> writes;
	for(size_t i = 0; i < infos.size(); ++i)
		{
			VkWriteDescriptorSet w{};
			w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w.dstSet = i == 0 ? m_setHdr : m_setBloomMips[i - 1];
			w.dstBinding = 0;
			w.descriptorCount = 1;
			w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			w.pImageInfo = &infos[i];
			writes.push_back(w);
		}
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(),
						   0, nullptr);
}

void Renderer::createCommandsAndSync()
{
	VkDevice device = m_context.getDevice();
	VkCommandPoolCreateInfo cpci{};
	cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpci.queueFamilyIndex
	  = m_context.findQueueFamilies(m_context.getPhysicalDevice()).graphicsFamily.value();
	if(vkCreateCommandPool(device, &cpci, nullptr, &m_commandPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render command pool");

	m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = m_commandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	if(vkAllocateCommandBuffers(device, &ai, m_commandBuffers.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffers");

	m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	VkSemaphoreCreateInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fi{};
	fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		if(vkCreateSemaphore(device, &si, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS
		   || vkCreateFence(device, &fi, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create sync objects");
	createRenderFinishedSemaphores();

	// Chronométrage GPU (timestamps entre les passes)
	if(m_context.getProperties().limits.timestampComputeAndGraphics)
		{
			m_timestampPeriod = m_context.getProperties().limits.timestampPeriod;
			VkQueryPoolCreateInfo qi{};
			qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
			qi.queryCount = PASS_COUNT + 1;
			for(auto &pool : m_queryPools)
				vkCreateQueryPool(device, &qi, nullptr, &pool);
		}
}

const char *Renderer::passName(int pass)
{
	static const char *names[] = {"sky", "shadows", "scene", "water", "bloom", "final"};
	return names[pass];
}

void Renderer::readTimestamps()
{
	if(!m_timestampPeriod || !m_queryValid[m_currentFrame])
		return;
	uint64_t ts[PASS_COUNT + 1];
	if(vkGetQueryPoolResults(m_context.getDevice(), m_queryPools[m_currentFrame], 0,
							 PASS_COUNT + 1, sizeof(ts), ts, sizeof(uint64_t),
							 VK_QUERY_RESULT_64_BIT)
	   != VK_SUCCESS)
		return;
	float total = 0.0f;
	m_stats.gpuFrameMs = m_stats.gpuFrameMs * 0.9f
						 + static_cast<float>(ts[PASS_COUNT] - ts[0]) * m_timestampPeriod * 1e-6f * 0.1f;
	for(int i = 0; i < PASS_COUNT; ++i)
		{
			float ms = static_cast<float>(ts[i + 1] - ts[i]) * m_timestampPeriod * 1e-6f;
			// Lissage exponentiel pour l'affichage
			m_stats.passMs[i] = m_stats.passMs[i] * 0.9f + ms * 0.1f;
			total += m_stats.passMs[i];
		}
	m_stats.gpuMs = total;
}

void Renderer::createRenderFinishedSemaphores()
{
	VkSemaphoreCreateInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	m_renderFinishedSemaphores.resize(m_swapchain.getImageCount());
	for(auto &sem : m_renderFinishedSemaphores)
		if(vkCreateSemaphore(m_context.getDevice(), &si, nullptr, &sem) != VK_SUCCESS)
			throw std::runtime_error("Failed to create sync objects");
}

void Renderer::destroyRenderFinishedSemaphores()
{
	for(auto sem : m_renderFinishedSemaphores)
		vkDestroySemaphore(m_context.getDevice(), sem, nullptr);
	m_renderFinishedSemaphores.clear();
}

void Renderer::initImGui()
{
	VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}};
	VkDescriptorPoolCreateInfo pi{};
	pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pi.maxSets = 1000;
	pi.poolSizeCount = 1;
	pi.pPoolSizes = poolSizes;
	vkCreateDescriptorPool(m_context.getDevice(), &pi, nullptr, &m_imguiPool);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 6.0f;
	style.FrameRounding = 4.0f;

	ImGui_ImplGlfw_InitForVulkan(m_window, true);
	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance = m_context.getInstance();
	info.PhysicalDevice = m_context.getPhysicalDevice();
	info.Device = m_context.getDevice();
	info.QueueFamily
	  = m_context.findQueueFamilies(m_context.getPhysicalDevice()).graphicsFamily.value();
	info.Queue = m_context.getGraphicsQueue();
	info.DescriptorPool = m_imguiPool;
	info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
	info.ImageCount = m_swapchain.getImageCount();
	info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.PipelineInfoMain.RenderPass = m_rpFinal;
	ImGui_ImplVulkan_Init(&info);

	m_panoramaId = ImGui_ImplVulkan_AddTexture(m_samplerLinearRepeat, m_panorama.view,
											   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_logoId = ImGui_ImplVulkan_AddTexture(m_samplerLinearClamp, m_logo.view,
										   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// ═════════════════════════════════════════════════════════════════
// Réglages & redimensionnement
// ═════════════════════════════════════════════════════════════════
void Renderer::recreateSwapchain()
{
	vkDeviceWaitIdle(m_context.getDevice());
	m_swapchain.recreate(m_context, m_window, m_rpFinal, m_vsync);
	destroyRenderFinishedSemaphores();
	createRenderFinishedSemaphores();
	destroyRenderTargets();
	createRenderTargets();
	writeDescriptors();
}

void Renderer::applySettings(const RenderSettings &settings)
{
	if(settings.vsync != m_vsync)
		{
			m_vsync = settings.vsync;
			recreateSwapchain();
		}
	if(settings.shadowResolution != m_shadowRes)
		{
			vkDeviceWaitIdle(m_context.getDevice());
			destroyShadowMap();
			createShadowMap(settings.shadowResolution);
			writeDescriptors();
			m_shadowDirty = true;
		}
	if(std::abs(settings.renderScale - m_renderScale) > 1e-3f)
		{
			m_renderScale = std::clamp(settings.renderScale, 0.25f, 2.0f);
			vkDeviceWaitIdle(m_context.getDevice());
			destroyRenderTargets();
			createRenderTargets();
			writeDescriptors();
		}
}

// ═════════════════════════════════════════════════════════════════
// Chunks
// ═════════════════════════════════════════════════════════════════
void Renderer::setChunkMesh(const ChunkMeshData &mesh)
{
	removeChunkMesh(mesh.chunkPos);
	if(mesh.empty())
		return;

	VkDeviceSize vBytes = mesh.vertices.size() * sizeof(Vertex);
	VkDeviceSize iBytes = mesh.indices.size() * sizeof(uint32_t);
	GpuChunk c;
	c.buffer = vkutil::createHostBuffer(m_context, vBytes + iBytes,
										VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
										  | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	auto *dst = static_cast<uint8_t *>(c.buffer.mapped);
	std::memcpy(dst, mesh.vertices.data(), vBytes);
	std::memcpy(dst + vBytes, mesh.indices.data(), iBytes);
	vkUnmapMemory(m_context.getDevice(), c.buffer.memory);
	c.buffer.mapped = nullptr;

	c.indexOffset = vBytes;
	c.opaqueCount = mesh.opaqueIndexCount;
	c.cutoutCount = mesh.cutoutIndexCount;
	c.waterCount = mesh.waterIndexCount;
	c.pos = mesh.chunkPos;
	c.minY = mesh.minY;
	c.maxY = mesh.maxY;
	m_chunkMemory += c.buffer.size;
	m_chunks[mesh.chunkPos] = c;
	glm::dvec2 d(mesh.chunkPos.x * 16.0 + 8.0 - m_shadowOrigin.x,
				 mesh.chunkPos.y * 16.0 + 8.0 - m_shadowOrigin.z);
	if(glm::length(d) < m_shadowDistUsed * 2.5 + 32.0)
		m_shadowDirty = true;
}

void Renderer::removeChunkMesh(const glm::ivec2 &pos)
{
	auto it = m_chunks.find(pos);
	if(it == m_chunks.end())
		return;
	m_chunkMemory -= it->second.buffer.size;
	// Peut encore être utilisé par les frames en vol : destruction différée
	m_deletionQueue.push_back({m_frameNumber + 1, it->second.buffer});
	m_chunks.erase(it);
}

void Renderer::flushDeletionQueue(bool all)
{
	auto it = std::remove_if(m_deletionQueue.begin(), m_deletionQueue.end(),
							 [&](auto &entry) {
								 if(!all && entry.first + MAX_FRAMES_IN_FLIGHT > m_frameNumber)
									 return false;
								 vkutil::destroyBuffer(m_context.getDevice(), entry.second);
								 return true;
							 });
	m_deletionQueue.erase(it, m_deletionQueue.end());
}

// ═════════════════════════════════════════════════════════════════
// Frame
// ═════════════════════════════════════════════════════════════════
void Renderer::beginImGuiFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void Renderer::fillUniforms(FrameUniforms &u, const FrameParams &params,
							const RenderSettings &settings)
{
	const Camera &cam = *params.camera;
	u.view = cam.getViewMatrix();
	u.proj = cam.getProjectionMatrix();
	u.viewProj = u.proj * u.view;
	u.invView = glm::inverse(u.view);
	u.invProj = glm::inverse(u.proj);
	u.invViewProj = glm::inverse(u.viewProj);

	glm::vec3 sunDir = atmosphere::sunDirection(settings.sunElevation, settings.sunAzimuth);
	glm::vec3 trans = atmosphere::sunTransmittance(sunDir);

	// ── Matrice de la shadow map (relative à la caméra, recalée texel) ──
	float dist = settings.shadowDistance;
	double texel = 2.0 * dist / m_shadowRes;
	glm::dvec3 camPos = glm::dvec3(cam.getPosition());
	m_shadowRefresh = settings.shadows
					  && (m_shadowDirty || glm::length(camPos - m_shadowOrigin) > 4.0
						  || glm::length(sunDir - m_shadowSun) > 1e-4f
						  || dist != m_shadowDistUsed);
	if(m_shadowRefresh)
		{
			glm::vec3 up = std::abs(sunDir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
			glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), -sunDir, up);
			glm::dmat3 rot = glm::dmat3(glm::mat3(lightView));
			glm::dvec3 camLS = rot * camPos;
			glm::dvec3 snapped = glm::floor(camLS / texel) * texel;
			glm::vec3 off = glm::vec3(snapped - camLS);
			glm::mat4 lightProj
			  = glm::orthoRH_ZO(off.x - dist, off.x + dist, off.y - dist, off.y + dist,
								-SHADOW_DEPTH_RANGE, SHADOW_DEPTH_RANGE);
			m_shadowMatrix = lightProj * lightView;
			m_shadowOrigin = camPos;
			m_shadowSun = sunDir;
			m_shadowDistUsed = dist;
			m_shadowDirty = false;
		}
	// Positions relatives à la caméra -> relatives à l'origine de la shadow map
	u.lightViewProj
	  = m_shadowMatrix * glm::translate(glm::mat4(1.0f), glm::vec3(camPos - m_shadowOrigin));

	u.cameraPos = glm::vec4(cam.getPosition(), params.time);
	u.sunDir = glm::vec4(sunDir, 1.0f);
	u.sunColor = glm::vec4(trans * SUN_IRRADIANCE, 1.0f);
	u.fogParams = glm::vec4(0.0011f * settings.fogDensity, 0.012f,
							static_cast<float>(settings.renderDistance * 16),
							params.underwater ? 1.0f : 0.0f);
	u.waterFog = glm::vec4(0.02f, 0.10f, 0.12f, 0.06f);
	u.screen = glm::vec4(m_renderExtent.width, m_renderExtent.height,
						 1.0f / m_renderExtent.width, 1.0f / m_renderExtent.height);
	u.post = glm::vec4(settings.exposure, settings.bloom ? settings.bloomStrength : 0.0f,
					   settings.godRays ? 1.0f : 0.0f, settings.saturation);
	u.clouds = glm::vec4(settings.cloudCoverage, 0.045f, 4.0f, settings.clouds ? 1.0f : 0.0f);
	u.shadow = glm::vec4(1.0f / m_shadowRes, dist, static_cast<float>(texel) * 1.5f,
						 settings.shadows ? 1.0f : 0.0f);
	u.misc = glm::vec4(m_swapchain.isSrgb() ? 1.0f : 0.0f, cam.getNear(), cam.getFar(),
					   static_cast<float>(m_frameNumber % 1024));
	if(!settings.waterReflections)
		u.misc.y = -cam.getNear(); // signe négatif = SSR désactivé
}

void Renderer::beginPass(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb,
						 VkExtent2D extent, const std::vector<VkClearValue> &clears)
{
	VkRenderPassBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	bi.renderPass = rp;
	bi.framebuffer = fb;
	bi.renderArea.extent = extent;
	bi.clearValueCount = static_cast<uint32_t>(clears.size());
	bi.pClearValues = clears.data();
	vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport vp{};
	vp.width = static_cast<float>(extent.width);
	vp.height = static_cast<float>(extent.height);
	vp.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &vp);
	VkRect2D scissor{{0, 0}, extent};
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Renderer::drawFullscreen(VkCommandBuffer cmd, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdDraw(cmd, 3, 1, 0, 0);
}

void Renderer::recordFrame(VkCommandBuffer cmd, uint32_t imageIndex,
						   const FrameParams &params, const RenderSettings &settings,
						   const FrameUniforms &u, ImDrawData *drawData)
{
	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if(vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
		throw std::runtime_error("Failed to begin command buffer");

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
							&m_globalSets[m_currentFrame], 0, nullptr);

	VkQueryPool queries = m_queryPools[m_currentFrame];
	auto stamp = [&](uint32_t index) {
		if(queries)
			vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, index);
	};
	if(queries)
		vkCmdResetQueryPool(cmd, queries, 0, PASS_COUNT + 1);
	stamp(0);

	const glm::dvec3 camPos = glm::dvec3(params.camera->getPosition());
	auto pushChunk = [&](const GpuChunk &c) {
		PushConstants pc{};
		pc.a = glm::vec4(static_cast<float>(c.pos.x * 16.0 - camPos.x),
						 static_cast<float>(-camPos.y),
						 static_cast<float>(c.pos.y * 16.0 - camPos.z), 0.0f);
		vkCmdPushConstants(cmd, m_pipelineLayout,
						   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						   sizeof(PushConstants), &pc);
		VkDeviceSize zero = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &c.buffer.buffer, &zero);
		vkCmdBindIndexBuffer(cmd, c.buffer.buffer, c.indexOffset, VK_INDEX_TYPE_UINT32);
	};

	// ── Sélection des chunks visibles (frustum) ─────────────────
	auto planes = params.camera->getFrustumPlanes();
	struct Visible
	{
		const GpuChunk *chunk;
		float dist2;
	};
	std::vector<Visible> visible;
	std::vector<const GpuChunk *> shadowCasters;
	visible.reserve(m_chunks.size());

	glm::mat3 lightRot = glm::mat3(glm::lookAt(
	  glm::vec3(0.0f), -glm::vec3(u.sunDir),
	  std::abs(u.sunDir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0)));
	float shadowDist = settings.shadowDistance;

	for(auto &[pos, c] : m_chunks)
		{
			glm::vec3 mn(static_cast<float>(pos.x * 16.0 - camPos.x),
						 static_cast<float>(c.minY - camPos.y),
						 static_cast<float>(pos.y * 16.0 - camPos.z));
			glm::vec3 mx = mn + glm::vec3(16.0f, c.maxY - c.minY, 16.0f);

			bool inside = true;
			for(const auto &p : planes)
				{
					glm::vec3 pv(p.x >= 0 ? mx.x : mn.x, p.y >= 0 ? mx.y : mn.y,
								 p.z >= 0 ? mx.z : mn.z);
					if(glm::dot(glm::vec3(p), pv) + p.w < 0.0f)
						{
							inside = false;
							break;
						}
				}
			glm::vec3 center = (mn + mx) * 0.5f;
			if(inside)
				visible.push_back({&c, glm::dot(center, center)});

			if(m_shadowRefresh && (c.opaqueCount > 0 || c.cutoutCount > 0))
				{
					// Boîte englobante du chunk dans l'espace de la lumière
					glm::vec3 lsMin(1e9f), lsMax(-1e9f);
					for(int k = 0; k < 8; ++k)
						{
							glm::vec3 corner((k & 1) ? mx.x : mn.x, (k & 2) ? mx.y : mn.y,
											 (k & 4) ? mx.z : mn.z);
							glm::vec3 ls = lightRot * corner;
							lsMin = glm::min(lsMin, ls);
							lsMax = glm::max(lsMax, ls);
						}
					if(lsMax.x >= -shadowDist && lsMin.x <= shadowDist && lsMax.y >= -shadowDist
					   && lsMin.y <= shadowDist && lsMax.z >= -SHADOW_DEPTH_RANGE
					   && lsMin.z <= SHADOW_DEPTH_RANGE)
						shadowCasters.push_back(&c);
				}
		}
	std::sort(visible.begin(), visible.end(),
			  [](const Visible &a, const Visible &b) { return a.dist2 < b.dist2; });
	m_stats.chunks = static_cast<int>(m_chunks.size());
	m_stats.drawnChunks = static_cast<int>(visible.size());
	m_stats.shadowChunks = static_cast<int>(shadowCasters.size());
	m_stats.gpuMemoryMB = static_cast<size_t>(m_chunkMemory / (1024 * 1024));

	// ── 1. Ciel : LUT d'atmosphère puis environnement + nuages ──
	beginPass(cmd, m_rpSky, m_fbSkyLut, {SKY_LUT_W, SKY_LUT_H}, {});
	drawFullscreen(cmd, m_pipe.skyLut);
	vkCmdEndRenderPass(cmd);
	beginPass(cmd, m_rpSky, m_fbSkyEnv, {SKY_ENV_W, SKY_ENV_H}, {});
	drawFullscreen(cmd, m_pipe.skyEnv);
	vkCmdEndRenderPass(cmd);
	beginPass(cmd, m_rpSky, m_fbClouds,
			  {std::max(1u, m_renderExtent.width / 2), std::max(1u, m_renderExtent.height / 2)},
			  {});
	drawFullscreen(cmd, m_pipe.clouds);
	vkCmdEndRenderPass(cmd);
	stamp(1);

	// ── 2. Ombres ───────────────────────────────────────────────
	VkExtent2D shadowExtent{static_cast<uint32_t>(m_shadowRes),
							static_cast<uint32_t>(m_shadowRes)};
	bool renderShadows = m_shadowRefresh || !settings.shadows;
	if(!settings.shadows)
		m_shadowDirty = true; // à réactivation : recalcul
	if(renderShadows)
		beginPass(cmd, m_rpShadow, m_fbShadow, shadowExtent, {clearDepth(1.0f)});
	if(m_shadowRefresh)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe.shadow);
			for(const GpuChunk *c : shadowCasters)
				if(c->opaqueCount > 0)
					{
						pushChunk(*c);
						vkCmdDrawIndexed(cmd, c->opaqueCount, 1, 0, 0, 0);
					}
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe.shadowCutout);
			for(const GpuChunk *c : shadowCasters)
				if(c->cutoutCount > 0)
					{
						pushChunk(*c);
						vkCmdDrawIndexed(cmd, c->cutoutCount, 1, c->opaqueCount, 0, 0);
					}
		}
	if(renderShadows)
		vkCmdEndRenderPass(cmd);
	stamp(2);

	// ── 3. Scène opaque + ciel ──────────────────────────────────
	beginPass(cmd, m_rpScene, m_fbScene, m_renderExtent,
			  {clearColor(0, 0, 0, 0), clearColor(1e6f, 0, 0, 0), clearDepth(0.0f)});
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe.terrain);
	for(const Visible &v : visible)
		{
			if(v.chunk->opaqueCount == 0)
				continue;
			pushChunk(*v.chunk);
			vkCmdDrawIndexed(cmd, v.chunk->opaqueCount, 1, 0, 0, 0);
		}
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe.terrainCutout);
	for(const Visible &v : visible)
		{
			if(v.chunk->cutoutCount == 0)
				continue;
			pushChunk(*v.chunk);
			vkCmdDrawIndexed(cmd, v.chunk->cutoutCount, 1, v.chunk->opaqueCount, 0, 0);
		}
	drawFullscreen(cmd, m_pipe.sky);
	vkCmdEndRenderPass(cmd);
	stamp(3);

	// ── 4. Eau (copie de la scène puis surfaces d'eau) ──────────
	beginPass(cmd, m_rpWater, m_fbWater, m_renderExtent, {});
	drawFullscreen(cmd, m_pipe.blit);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe.water);
	for(const Visible &v : visible)
		{
			if(v.chunk->waterCount == 0)
				continue;
			pushChunk(*v.chunk);
			vkCmdDrawIndexed(cmd, v.chunk->waterCount, 1,
							 v.chunk->opaqueCount + v.chunk->cutoutCount, 0, 0);
		}
	vkCmdEndRenderPass(cmd);
	stamp(4);

	// ── 5. Bloom : réductions puis agrandissements additifs ─────
	VkExtent2D bloomBase{std::max(1u, m_renderExtent.width / 2),
						 std::max(1u, m_renderExtent.height / 2)};
	auto mipExtent = [&](uint32_t m) {
		return VkExtent2D{std::max(1u, bloomBase.width >> m),
						  std::max(1u, bloomBase.height >> m)};
	};
	for(uint32_t m = 0; m < BLOOM_MIPS && settings.bloom; ++m)
		{
			VkExtent2D src = m == 0 ? m_renderExtent : mipExtent(m - 1);
			beginPass(cmd, m_rpBloomDown, m_fbBloom[m], mipExtent(m), {});
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe.bloomDown);
			VkDescriptorSet set = m == 0 ? m_setHdr : m_setBloomMips[m - 1];
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
									1, 1, &set, 0, nullptr);
			PushConstants pc{};
			pc.a = glm::vec4(1.0f / src.width, 1.0f / src.height, m == 0 ? 1.0f : 0.0f, 0.0f);
			vkCmdPushConstants(cmd, m_pipelineLayout,
							   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
							   sizeof(pc), &pc);
			vkCmdDraw(cmd, 3, 1, 0, 0);
			vkCmdEndRenderPass(cmd);
		}
	for(int m = static_cast<int>(BLOOM_MIPS) - 2; m >= 0 && settings.bloom; --m)
		{
			VkExtent2D src = mipExtent(static_cast<uint32_t>(m + 1));
			beginPass(cmd, m_rpBloomUp, m_fbBloom[m], mipExtent(static_cast<uint32_t>(m)), {});
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe.bloomUp);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
									1, 1, &m_setBloomMips[m + 1], 0, nullptr);
			PushConstants pc{};
			pc.a = glm::vec4(1.0f / src.width, 1.0f / src.height, 1.0f, 0.0f);
			vkCmdPushConstants(cmd, m_pipelineLayout,
							   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
							   sizeof(pc), &pc);
			vkCmdDraw(cmd, 3, 1, 0, 0);
			vkCmdEndRenderPass(cmd);
		}

	stamp(5);

	// ── 6. Composition finale + interface ───────────────────────
	beginPass(cmd, m_rpFinal, m_swapchain.getFramebuffers()[imageIndex],
			  m_swapchain.getExtent(), {clearColor(0, 0, 0, 1)});
	drawFullscreen(cmd, m_pipe.composite);
	ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
	vkCmdEndRenderPass(cmd);
	stamp(6);
	m_queryValid[m_currentFrame] = queries != VK_NULL_HANDLE;

	if(!m_screenshotPath.empty())
		recordScreenshotCopy(cmd, imageIndex);

	if(vkEndCommandBuffer(cmd) != VK_SUCCESS)
		throw std::runtime_error("Failed to record command buffer");
}

void Renderer::drawFrame(const FrameParams &params, const RenderSettings &settings)
{
	auto cpuStart = std::chrono::steady_clock::now();
	ImGui::Render();
	ImDrawData *drawData = ImGui::GetDrawData();
	VkDevice device = m_context.getDevice();

	applySettings(settings);

	auto waitStart = std::chrono::steady_clock::now();
	vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
	m_stats.fenceWaitMs = m_stats.fenceWaitMs * 0.9f
						  + std::chrono::duration<float, std::milli>(
							  std::chrono::steady_clock::now() - waitStart).count() * 0.1f;
	m_frameNumber++;
	flushDeletionQueue(false);
	readTimestamps();

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device, m_swapchain.getSwapchain(), UINT64_MAX,
											m_imageAvailableSemaphores[m_currentFrame],
											VK_NULL_HANDLE, &imageIndex);
	if(result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapchain();
			return;
		}
	if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw std::runtime_error("Failed to acquire swap chain image");

	vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

	FrameUniforms u{};
	fillUniforms(u, params, settings);
	std::memcpy(m_uniforms[m_currentFrame].mapped, &u, sizeof(u));

	VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
	vkResetCommandBuffer(cmd, 0);
	auto recStart = std::chrono::steady_clock::now();
	recordFrame(cmd, imageIndex, params, settings, u, drawData);
	m_stats.recordMs = m_stats.recordMs * 0.9f
					   + std::chrono::duration<float, std::milli>(
						   std::chrono::steady_clock::now() - recStart).count() * 0.1f;

	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore waitSems[] = {m_imageAvailableSemaphores[m_currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = waitSems;
	si.pWaitDstStageMask = waitStages;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	VkSemaphore signalSems[] = {m_renderFinishedSemaphores[imageIndex]};
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = signalSems;
	if(vkQueueSubmit(m_context.getGraphicsQueue(), 1, &si, m_inFlightFences[m_currentFrame])
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to submit draw command buffer");

	if(m_screenshotBuffer.buffer != VK_NULL_HANDLE)
		{
			vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
			saveScreenshot();
		}

	VkPresentInfoKHR pi{};
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.waitSemaphoreCount = 1;
	pi.pWaitSemaphores = signalSems;
	VkSwapchainKHR swapChains[] = {m_swapchain.getSwapchain()};
	pi.swapchainCount = 1;
	pi.pSwapchains = swapChains;
	pi.pImageIndices = &imageIndex;
	result = vkQueuePresentKHR(m_context.getPresentQueue(), &pi);
	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
		{
			framebufferResized = false;
			recreateSwapchain();
		}
	else if(result != VK_SUCCESS)
		throw std::runtime_error("Failed to present swap chain image");
	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	m_stats.drawFrameMs = m_stats.drawFrameMs * 0.9f
						  + std::chrono::duration<float, std::milli>(
							  std::chrono::steady_clock::now() - cpuStart).count() * 0.1f;
}

void Renderer::waitIdle() { vkDeviceWaitIdle(m_context.getDevice()); }

// ═════════════════════════════════════════════════════════════════
// Nettoyage
// ═════════════════════════════════════════════════════════════════
void Renderer::cleanup()
{
	VkDevice device = m_context.getDevice();
	vkDeviceWaitIdle(device);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	vkDestroyDescriptorPool(device, m_imguiPool, nullptr);

	for(auto &[pos, c] : m_chunks)
		vkutil::destroyBuffer(device, c.buffer);
	m_chunks.clear();
	flushDeletionQueue(true);

	destroyRenderTargets();
	destroyShadowMap();
	vkDestroyFramebuffer(device, m_fbSkyLut, nullptr);
	vkDestroyFramebuffer(device, m_fbSkyEnv, nullptr);
	for(GpuImage *img : {&m_skyLut, &m_skyEnv, &m_cloudNoise, &m_panorama, &m_logo,
						 &m_blockTex.albedo, &m_blockTex.normal})
		vkutil::destroyImage(device, *img);

	for(VkSampler s : {m_samplerBlocks, m_samplerLinearClamp, m_samplerLinearRepeat,
					   m_samplerSky, m_samplerNearestClamp, m_samplerShadow})
		vkDestroySampler(device, s, nullptr);

	for(auto &ubo : m_uniforms)
		vkutil::destroyBuffer(device, ubo);
	vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

	for(VkPipeline p : {m_pipe.skyLut, m_pipe.skyEnv, m_pipe.clouds, m_pipe.shadow, m_pipe.shadowCutout,
						m_pipe.terrain, m_pipe.terrainCutout,
						m_pipe.sky, m_pipe.blit, m_pipe.water, m_pipe.bloomDown,
						m_pipe.bloomUp, m_pipe.composite})
		vkDestroyPipeline(device, p, nullptr);
	vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, m_globalLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, m_textureLayout, nullptr);
	for(VkRenderPass rp : {m_rpSky, m_rpShadow, m_rpScene, m_rpWater, m_rpBloomDown,
						   m_rpBloomUp, m_rpFinal})
		vkDestroyRenderPass(device, rp, nullptr);

	m_swapchain.cleanup(device);
	destroyRenderFinishedSemaphores();
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, m_inFlightFences[i], nullptr);
		}
	for(auto pool : m_queryPools)
		if(pool)
			vkDestroyQueryPool(device, pool, nullptr);
	vkDestroyCommandPool(device, m_commandPool, nullptr);
	m_context.cleanup();
}

// ═════════════════════════════════════════════════════════════════
// Capture d'écran
// ═════════════════════════════════════════════════════════════════
void Renderer::requestScreenshot(const std::string &path)
{
	if(!m_swapchain.supportsTransferSrc())
		{
			LOG_WARNING("Screenshots not supported by this swapchain.");
			return;
		}
	m_screenshotPath = path;
}

void Renderer::recordScreenshotCopy(VkCommandBuffer cmd, uint32_t imageIndex)
{
	VkExtent2D extent = m_swapchain.getExtent();
	VkDeviceSize size = static_cast<VkDeviceSize>(extent.width) * extent.height * 4;
	m_screenshotBuffer
	  = vkutil::createHostBuffer(m_context, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	VkImage image = m_swapchain.getImage(imageIndex);
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
						 &barrier);

	VkBufferImageCopy region{};
	region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	region.imageExtent = {extent.width, extent.height, 1};
	vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   m_screenshotBuffer.buffer, 1, &region);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = 0;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
						 1, &barrier);
}

void Renderer::saveScreenshot()
{
	VkExtent2D extent = m_swapchain.getExtent();
	size_t pixelCount = static_cast<size_t>(extent.width) * extent.height;
	std::vector<uint8_t> pixels(pixelCount * 4);
	std::memcpy(pixels.data(), m_screenshotBuffer.mapped, pixels.size());

	VkFormat fmt = m_swapchain.getImageFormat();
	bool bgra = fmt == VK_FORMAT_B8G8R8A8_SRGB || fmt == VK_FORMAT_B8G8R8A8_UNORM;
	for(size_t i = 0; i < pixelCount; ++i)
		{
			if(bgra)
				std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);
			pixels[i * 4 + 3] = 255;
		}

	std::filesystem::path out(m_screenshotPath);
	if(out.has_parent_path())
		std::filesystem::create_directories(out.parent_path());
	if(stbi_write_png(m_screenshotPath.c_str(), static_cast<int>(extent.width),
					  static_cast<int>(extent.height), 4, pixels.data(),
					  static_cast<int>(extent.width) * 4))
		LOG_SUCCESS("Screenshot saved: " << m_screenshotPath);
	else
		LOG_ERROR("Failed to write screenshot: " << m_screenshotPath);

	vkutil::destroyBuffer(m_context.getDevice(), m_screenshotBuffer);
	m_screenshotPath.clear();
}
