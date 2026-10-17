#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <functional>
#include <optional>
#include <vector>

// ── Queue Family Indices ────────────────────────────────────────
struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	bool isComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

// ── Swap Chain Support Details ──────────────────────────────────
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

/// Gère l'initialisation bas niveau de Vulkan :
/// Instance, Debug Messenger, Surface, Physical/Logical Device.
class VkContext
{
  public:
	void init(GLFWwindow *window);
	void cleanup();

	// ── Accessors ─────────────────────────────────────────────────
	VkInstance getInstance() const { return m_instance; }
	VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
	VkDevice getDevice() const { return m_device; }
	VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
	VkQueue getPresentQueue() const { return m_presentQueue; }
	VkSurfaceKHR getSurface() const { return m_surface; }

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
	SwapChainSupportDetails
	querySwapChainSupport(VkPhysicalDevice device) const;
	uint32_t findMemoryType(uint32_t typeFilter,
							VkMemoryPropertyFlags properties) const;
	/// Comme findMemoryType, sans exception (nullopt si introuvable)
	std::optional<uint32_t>
	tryFindMemoryType(uint32_t typeFilter,
					  VkMemoryPropertyFlags properties) const;

	/// Enregistre et exécute immédiatement des commandes (bloquant)
	void immediateSubmit(const std::function<void(VkCommandBuffer)> &record);

	/// Le format supporte-t-il ces fonctionnalités (tiling optimal) ?
	bool formatSupports(VkFormat format, VkFormatFeatureFlags features) const;

	bool hasAnisotropy() const { return m_anisotropy; }

	const VkPhysicalDeviceProperties &getProperties() const
	{
		return m_properties;
	}

  private:
	GLFWwindow *m_window = nullptr;

	VkInstance m_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	VkQueue m_presentQueue = VK_NULL_HANDLE;
	VkCommandPool m_transientPool = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties m_properties{};
	bool m_anisotropy = false;

	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	bool isDeviceSuitable(VkPhysicalDevice device);
};
