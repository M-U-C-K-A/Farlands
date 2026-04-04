#include "renderer.h"

#include <array>
#include <stdexcept>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "../world/block.h"

// ── Init ────────────────────────────────────────────────────────
void Renderer::init(GLFWwindow *window)
{
	m_window = window;

	m_context.init(window);

	// 1. Swapchain first (we need the image format)
	m_swapchain.create(m_context, window);

	// 2. Pipeline (needs swapchain format for render pass)
	m_pipeline.create(m_context, m_swapchain);

	// 3. Framebuffers (need the render pass from pipeline)
	m_swapchain.createFramebuffers(m_context.getDevice(),
								   m_pipeline.getRenderPass());

	// 4. Textures Array & UI Resources
	std::vector<std::string> texturePaths = BlockDatabase::GetTexturePaths();
	std::vector<std::string> absPaths;
	for (const auto& p : texturePaths) {
	    if (!p.empty()) absPaths.push_back(std::string(ASSETS_DIR) + "/" + p);
	    else absPaths.push_back("");
	}
	m_texture.initArray(m_context, absPaths);
	m_panoramaTexture.init(m_context, std::string(ASSETS_DIR) + "/textures/gui/title/background/panorama_0.png");
	m_logoTexture.init(m_context, std::string(ASSETS_DIR) + "/Farlands.png");

	m_buffers.init(m_context, m_pipeline.getDescriptorSetLayout(), m_texture.getImageView(), m_texture.getSampler());
	createCommandBuffers();
	createSyncObjects();
	initImGui();
}

// ── ImGui ───────────────────────────────────────────────────────
void Renderer::initImGui()
{
	VkDescriptorPoolSize pool_sizes[]
	  = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = pool_sizes;
	vkCreateDescriptorPool(m_context.getDevice(), &pool_info, nullptr,
						   &m_imguiPool);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForVulkan(m_window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_context.getInstance();
	init_info.PhysicalDevice = m_context.getPhysicalDevice();
	init_info.Device = m_context.getDevice();
	init_info.QueueFamily
	  = m_context.findQueueFamilies(m_context.getPhysicalDevice())
		  .graphicsFamily.value();
	init_info.Queue = m_context.getGraphicsQueue();
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = m_imguiPool;
	init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
	init_info.ImageCount = m_swapchain.getImageCount();
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.PipelineInfoMain.RenderPass = m_pipeline.getRenderPass();
	ImGui_ImplVulkan_Init(&init_info);

	// Load panorama texture for ImGui
	m_panoramaTextureId = ImGui_ImplVulkan_AddTexture(
		m_panoramaTexture.getSampler(),
		m_panoramaTexture.getImageView(),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	m_logoTextureId = ImGui_ImplVulkan_AddTexture(
		m_logoTexture.getSampler(),
		m_logoTexture.getImageView(),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// ── Command Buffers ─────────────────────────────────────────────
void Renderer::createCommandBuffers()
{
	m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	// Create command pool for rendering commands
	auto indices = m_context.findQueueFamilies(m_context.getPhysicalDevice());
	VkCommandPoolCreateInfo cpci{};
	cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpci.queueFamilyIndex = indices.graphicsFamily.value();
	if(vkCreateCommandPool(m_context.getDevice(), &cpci, nullptr,
						   &m_renderCommandPool)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create render command pool");

	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = m_renderCommandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	if(vkAllocateCommandBuffers(m_context.getDevice(), &ai,
								m_commandBuffers.data())
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffers");
}

// ── Sync Objects ────────────────────────────────────────────────
void Renderer::createSyncObjects()
{
	m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	VkSemaphoreCreateInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fi{};
	fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if(vkCreateSemaphore(m_context.getDevice(), &si, nullptr,
								 &m_imageAvailableSemaphores[i])
				 != VK_SUCCESS
			   || vkCreateSemaphore(m_context.getDevice(), &si, nullptr,
									&m_renderFinishedSemaphores[i])
					!= VK_SUCCESS
			   || vkCreateFence(m_context.getDevice(), &fi, nullptr,
								&m_inFlightFences[i])
					!= VK_SUCCESS)
				throw std::runtime_error("Failed to create sync objects");
		}
}

// ── Record Command Buffer ───────────────────────────────────────
void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
								   ImDrawData *draw_data, bool inMenu, float time)
{
	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if(vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
		throw std::runtime_error("Failed to begin command buffer");

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Backplate
	clearValues[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo rpbi{};
	rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpbi.renderPass = m_pipeline.getRenderPass();
	rpbi.framebuffer = m_swapchain.getFramebuffers()[imageIndex];
	rpbi.renderArea.extent = m_swapchain.getExtent();
	rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size());
	rpbi.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

	if(!inMenu)
		{
			// --- SKY DRAW ---
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getSkyPipeline());

			VkViewport viewport{};
			viewport.width = (float)m_swapchain.getExtent().width;
			viewport.height = (float)m_swapchain.getExtent().height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			
			VkRect2D scissor{};
			scissor.extent = m_swapchain.getExtent();
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			auto ds = m_buffers.getDescriptorSet(m_currentFrame);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipelineLayout(), 0, 1, &ds, 0, nullptr);
			vkCmdDraw(cmd, 3, 1, 0, 0);

			// --- WORLD DRAW ---
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							  m_pipeline.getPipeline());

			if(m_buffers.getIndexCount() > 0
			   && m_buffers.getVertexBuffer() != VK_NULL_HANDLE)
				{
					VkBuffer vertBufs[] = {m_buffers.getVertexBuffer()};
					VkDeviceSize offsets[] = {0};
					vkCmdBindVertexBuffers(cmd, 0, 1, vertBufs, offsets);
					vkCmdBindIndexBuffer(cmd, m_buffers.getIndexBuffer(), 0,
										 VK_INDEX_TYPE_UINT32);
					auto ds = m_buffers.getDescriptorSet(m_currentFrame);
					vkCmdBindDescriptorSets(
					  cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					  m_pipeline.getPipelineLayout(), 0, 1, &ds, 0, nullptr);
					vkCmdDrawIndexed(cmd, m_buffers.getIndexCount(), 1, 0, 0,
									 0);
				}
		}

	ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);

	vkCmdEndRenderPass(cmd);
	if(vkEndCommandBuffer(cmd) != VK_SUCCESS)
		throw std::runtime_error("Failed to record command buffer");
}

// ── ImGui Frame ─────────────────────────────────────────────────
void Renderer::beginImGuiFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

// ── Draw Frame ──────────────────────────────────────────────────
void Renderer::drawFrame(const UniformBufferObject &ubo, bool inMenu)
{
	// Caller must have called beginImGuiFrame() + drawn UI before this
	ImGui::Render();
	ImDrawData *draw_data = ImGui::GetDrawData();

	vkWaitForFences(m_context.getDevice(), 1,
					&m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(
	  m_context.getDevice(), m_swapchain.getSwapchain(), UINT64_MAX,
	  m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
	if(result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_swapchain.recreate(m_context, m_window,
								 m_pipeline.getRenderPass());
			return;
		}
	if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw std::runtime_error("Failed to acquire swap chain image");

	vkResetFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame]);
	m_buffers.updateUniform(m_currentFrame, ubo);

	vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
	recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex,
						draw_data, inMenu, ubo.time);

	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore waitSems[] = {m_imageAvailableSemaphores[m_currentFrame]};
	VkPipelineStageFlags waitStages[]
	  = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = waitSems;
	si.pWaitDstStageMask = waitStages;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &m_commandBuffers[m_currentFrame];
	VkSemaphore signalSems[] = {m_renderFinishedSemaphores[m_currentFrame]};
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = signalSems;
	if(vkQueueSubmit(m_context.getGraphicsQueue(), 1, &si,
					 m_inFlightFences[m_currentFrame])
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to submit draw command buffer");

	VkPresentInfoKHR pi{};
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.waitSemaphoreCount = 1;
	pi.pWaitSemaphores = signalSems;
	VkSwapchainKHR swapChains[] = {m_swapchain.getSwapchain()};
	pi.swapchainCount = 1;
	pi.pSwapchains = swapChains;
	pi.pImageIndices = &imageIndex;
	result = vkQueuePresentKHR(m_context.getPresentQueue(), &pi);
	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
	   || framebufferResized)
		{
			framebufferResized = false;
			m_swapchain.recreate(m_context, m_window,
								 m_pipeline.getRenderPass());
		}
	else if(result != VK_SUCCESS)
		throw std::runtime_error("Failed to present swap chain image");
	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ── Update Buffers ──────────────────────────────────────────────
void Renderer::updateBuffers(const std::vector<Vertex> &vertices,
							 const std::vector<uint32_t> &indices)
{
	m_buffers.updateMeshBuffers(m_context, vertices, indices);
}

// ── Wait Idle ───────────────────────────────────────────────────
void Renderer::waitIdle() { vkDeviceWaitIdle(m_context.getDevice()); }

// ── Cleanup ─────────────────────────────────────────────────────
void Renderer::cleanup()
{
	vkDeviceWaitIdle(m_context.getDevice());

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	vkDestroyDescriptorPool(m_context.getDevice(), m_imguiPool, nullptr);

	m_swapchain.cleanup(m_context.getDevice());
	m_buffers.cleanup(m_context.getDevice());
	m_texture.cleanup(m_context.getDevice());
	m_panoramaTexture.cleanup(m_context.getDevice());
	m_logoTexture.cleanup(m_context.getDevice());
	m_pipeline.cleanup(m_context.getDevice());

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(m_context.getDevice(),
							   m_renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_context.getDevice(),
							   m_imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(m_context.getDevice(), m_inFlightFences[i],
						   nullptr);
		}

	vkDestroyCommandPool(m_context.getDevice(), m_renderCommandPool, nullptr);
	m_context.cleanup();
}
