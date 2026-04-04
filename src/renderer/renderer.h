// -----------------------------------------------------------------------------
// Fichier : renderer.h
// Rôle : Orchestrateur du moteur de rendu Vulkan.
// Gère le swapchain, le pipeline, l'interface graphique via ImGui et les
// buffers (VK).
// -----------------------------------------------------------------------------
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../core/types.h"
#include "vk_buffers.h"
#include "vk_context.h"
#include "vk_pipeline.h"
#include "vk_swapchain.h"
#include "vk_texture.h"

#include "imgui.h"

#include <vector>

/// Orchestrateur de rendu Vulkan.
/// Coordonne VkContext, VkSwapchain, VkPipelineManager et VkBufferManager.
class Renderer
{
  public:
	// Initialise l'ensemble du pipeline Vulkan (Device, RenderPass, Swapchain,
	// ImgUI)
	void init(GLFWwindow *window);
	// Démarre une nouvelle frame ImGui pour construire l'UI
	void beginImGuiFrame();
	// Commande à Vulkan de dessiner le frame-buffer courant (modèles +
	// interface)
	void drawFrame(const UniformBufferObject &ubo, bool inMenu);
	// Transfère un nouveau mesh de données vers les vertex/index buffers en GPU
	void updateBuffers(const std::vector<Vertex> &vertices,
					   const std::vector<uint32_t> &indices);
	// Détruit et libère l'ensembes des outils et contextes Vulkan créés
	void cleanup();
	// Fonction bloquante, attend que le GPU termine son exécution courante
	void waitIdle();

	bool framebufferResized = false;

	// Expose for ImGui init in menu
	GLFWwindow *getWindow() const { return m_window; }
	VkDescriptorSet getPanoramaTextureId() const { return m_panoramaTextureId; }
	VkDescriptorSet getLogoTextureId() const { return m_logoTextureId; }

  private:
	GLFWwindow *m_window = nullptr;

	VkContext m_context;
	VkSwapchain m_swapchain;
	VkTexture m_texture;
	VkTexture m_panoramaTexture;
	VkTexture m_logoTexture;
	VkPipelineManager m_pipeline;
	VkBufferManager m_buffers;

	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;
	uint32_t m_currentFrame = 0;

	VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
	VkCommandPool m_renderCommandPool = VK_NULL_HANDLE;
	VkDescriptorSet m_panoramaTextureId = VK_NULL_HANDLE;
	VkDescriptorSet m_logoTextureId = VK_NULL_HANDLE;

	// Prépare les command buffers nécessaires selon le nombre d'images de la
	// swapchain
	void createCommandBuffers();
	// Crée les objets de synchronisation Vulkan (sémaphores, fences) utilent
	// au timing
	void createSyncObjects();
	// Initialise ImGui avec le backend Vulkan et le render-pass du swapchain
	void initImGui();
	// Écrit les instructions de rendu dans un vkCommandBuffer donné (bind
	// pipeline, buffers, draw command)
	void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
							 ImDrawData *draw_data, bool inMenu, float time);
};
