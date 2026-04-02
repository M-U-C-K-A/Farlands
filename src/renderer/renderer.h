#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../core/types.h"
#include "vk_buffers.h"
#include "vk_context.h"
#include "vk_pipeline.h"
#include "vk_swapchain.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include <vector>

/// Orchestrateur de rendu Vulkan.
/// Coordonne VkContext, VkSwapchain, VkPipelineManager et VkBufferManager.
class Renderer {
public:
  void init(GLFWwindow *window);
  void beginImGuiFrame();
  void drawFrame(const UniformBufferObject &ubo, bool inMenu);
  void updateBuffers(const std::vector<Vertex> &vertices,
                     const std::vector<uint32_t> &indices);
  void cleanup();
  void waitIdle();

  bool framebufferResized = false;

  // Expose for ImGui init in menu
  GLFWwindow *getWindow() const { return m_window; }

private:
  GLFWwindow *m_window = nullptr;

  VkContext m_context;
  VkSwapchain m_swapchain;
  VkPipelineManager m_pipeline;
  VkBufferManager m_buffers;

  std::vector<VkCommandBuffer> m_commandBuffers;
  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  std::vector<VkFence> m_inFlightFences;
  uint32_t m_currentFrame = 0;

  VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
  VkCommandPool m_renderCommandPool = VK_NULL_HANDLE;

  void createCommandBuffers();
  void createSyncObjects();
  void initImGui();
  void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                           ImDrawData *draw_data, bool inMenu);
};
