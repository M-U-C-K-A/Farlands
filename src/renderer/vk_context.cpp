#include "vk_context.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

// ── Validation Layers ───────────────────────────────────────────
static const std::vector<const char *> validationLayers
  = {"VK_LAYER_KHRONOS_validation"};

static bool checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> available(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, available.data());
	for(auto layerName : validationLayers)
		{
			bool found = false;
			for(auto &prop : available)
				{
					if(strcmp(layerName, prop.layerName) == 0)
						{
							found = true;
							break;
						}
				}
			if(!found)
				return false;
		}
	return true;
}

#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = checkValidationLayerSupport();
#endif

static const std::vector<const char *> deviceExtensions
  = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};

// ── Debug Callback ──────────────────────────────────────────────
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT,
  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
{
	if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		std::cerr << "Validation: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

// ── Init ────────────────────────────────────────────────────────
void VkContext::init(GLFWwindow *window)
{
	m_window = window;
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
}

// ── Instance ────────────────────────────────────────────────────
void VkContext::createInstance()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Minecraft Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Farlands";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	uint32_t glfwExtCount = 0;
	const char **glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
	std::vector<const char *> extensions(glfwExts, glfwExts + glfwExtCount);
	extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	if(enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	createInfo.enabledExtensionCount
	  = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	if(enableValidationLayers)
		{
			createInfo.enabledLayerCount
			  = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}

	if(vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vulkan instance");
}

// ── Debug Messenger ─────────────────────────────────────────────
void VkContext::setupDebugMessenger()
{
	if(!enableValidationLayers)
		return;
	VkDebugUtilsMessengerCreateInfoEXT ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
						 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
					 | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	ci.pfnUserCallback = debugCallback;
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
	  m_instance, "vkCreateDebugUtilsMessengerEXT");
	if(func)
		func(m_instance, &ci, nullptr, &m_debugMessenger);
}

// ── Surface ─────────────────────────────────────────────────────
void VkContext::createSurface()
{
	if(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface)
	   != VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface");
}

// ── Queue Families ──────────────────────────────────────────────
QueueFamilyIndices VkContext::findQueueFamilies(VkPhysicalDevice device) const
{
	QueueFamilyIndices indices;
	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
	for(uint32_t i = 0; i < count; i++)
		{
			if(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;
			VkBool32 present = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface,
												 &present);
			if(present)
				indices.presentFamily = i;
			if(indices.isComplete())
				break;
		}
	return indices;
}

// ── Swap Chain Support ──────────────────────────────────────────
SwapChainSupportDetails
VkContext::querySwapChainSupport(VkPhysicalDevice device) const
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface,
											  &details.capabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount,
										 nullptr);
	if(formatCount)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(
			  device, m_surface, &formatCount, details.formats.data());
		}
	uint32_t modeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount,
											  nullptr);
	if(modeCount)
		{
			details.presentModes.resize(modeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(
			  device, m_surface, &modeCount, details.presentModes.data());
		}
	return details;
}

// ── Device Suitability ──────────────────────────────────────────
bool VkContext::isDeviceSuitable(VkPhysicalDevice device)
{
	auto indices = findQueueFamilies(device);
	uint32_t extCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
	std::vector<VkExtensionProperties> available(extCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount,
										 available.data());
	std::set<std::string> required(deviceExtensions.begin(),
								   deviceExtensions.end());
	for(auto &ext : available)
		required.erase(ext.extensionName);
	bool swapChainOk = false;
	if(required.empty())
		{
			auto s = querySwapChainSupport(device);
			swapChainOk = !s.formats.empty() && !s.presentModes.empty();
		}
	return indices.isComplete() && required.empty() && swapChainOk;
}

// ── Physical Device ─────────────────────────────────────────────
void VkContext::pickPhysicalDevice()
{
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
	if(count == 0)
		throw std::runtime_error("No GPUs with Vulkan support");
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
	for(auto &d : devices)
		{
			if(isDeviceSuitable(d))
				{
					m_physicalDevice = d;
					break;
				}
		}
	if(m_physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("No suitable GPU found");
}

// ── Logical Device ──────────────────────────────────────────────
void VkContext::createLogicalDevice()
{
	auto indices = findQueueFamilies(m_physicalDevice);
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	std::set<uint32_t> uniqueFamilies
	  = {indices.graphicsFamily.value(), indices.presentFamily.value()};
	float priority = 1.0f;
	for(auto family : uniqueFamilies)
		{
			VkDeviceQueueCreateInfo qi{};
			qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qi.queueFamilyIndex = family;
			qi.queueCount = 1;
			qi.pQueuePriorities = &priority;
			queueInfos.push_back(qi);
		}
	VkPhysicalDeviceFeatures features{};
	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	ci.pQueueCreateInfos = queueInfos.data();
	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	ci.ppEnabledExtensionNames = deviceExtensions.data();
	if(vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device) != VK_SUCCESS)
		throw std::runtime_error("Failed to create logical device");
	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0,
					 &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0,
					 &m_presentQueue);
}

// ── Memory Type ─────────────────────────────────────────────────
uint32_t VkContext::findMemoryType(uint32_t typeFilter,
								   VkMemoryPropertyFlags props) const
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
	for(uint32_t i = 0; i < memProps.memoryTypeCount; i++)
		if((typeFilter & (1 << i))
		   && (memProps.memoryTypes[i].propertyFlags & props) == props)
			return i;
	throw std::runtime_error("Failed to find suitable memory type");
}

// ── Cleanup ─────────────────────────────────────────────────────
void VkContext::cleanup()
{
	vkDestroyDevice(m_device, nullptr);
	if(enableValidationLayers)
		{
			auto func
			  = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
				m_instance, "vkDestroyDebugUtilsMessengerEXT");
			if(func)
				func(m_instance, m_debugMessenger, nullptr);
		}
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}
