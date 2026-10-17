#include "vk_context.h"

#include <algorithm>
#include <cstring>
#include "core/logger.h"
#include <set>
#include <string>
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
  = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// MoltenVK (macOS) exposes this extension; when present it MUST be enabled.
static const char *kPortabilitySubsetExt = "VK_KHR_portability_subset";

static bool hasInstanceExtension(const char *name)
{
	uint32_t count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> exts(count);
	vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
	for(auto &e : exts)
		if(strcmp(e.extensionName, name) == 0)
			return true;
	return false;
}

static bool hasDeviceExtension(VkPhysicalDevice device, const char *name)
{
	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> exts(count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
										 exts.data());
	for(auto &e : exts)
		if(strcmp(e.extensionName, name) == 0)
			return true;
	return false;
}

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
	LOG_INFO("Creating Vulkan instance...");
	createInstance();
	LOG_SUCCESS("Vulkan instance created.");

	setupDebugMessenger();

	LOG_INFO("Creating window surface...");
	createSurface();
	LOG_SUCCESS("Window surface created.");

	LOG_INFO("Selecting physical device (GPU)...");
	pickPhysicalDevice();

	LOG_INFO("Creating logical device and queues...");
	createLogicalDevice();
	vkGetPhysicalDeviceProperties(m_physicalDevice, &m_properties);

	VkCommandPoolCreateInfo pci{};
	pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
				| VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pci.queueFamilyIndex = findQueueFamilies(m_physicalDevice).graphicsFamily.value();
	if(vkCreateCommandPool(m_device, &pci, nullptr, &m_transientPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create transient command pool");
	LOG_SUCCESS("Logical device and queues ready.");
}

// ── Immediate submit ────────────────────────────────────────────
void VkContext::immediateSubmit(
  const std::function<void(VkCommandBuffer)> &record)
{
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = m_transientPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;
	VkCommandBuffer cmd;
	if(vkAllocateCommandBuffers(m_device, &ai, &cmd) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffer");

	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);
	record(cmd);
	vkEndCommandBuffer(cmd);

	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	vkQueueSubmit(m_graphicsQueue, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_graphicsQueue);
	vkFreeCommandBuffers(m_device, m_transientPool, 1, &cmd);
}

bool VkContext::formatSupports(VkFormat format,
							   VkFormatFeatureFlags features) const
{
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
	return (props.optimalTilingFeatures & features) == features;
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
	if(!glfwExts)
		throw std::runtime_error(
		  "GLFW: Vulkan is not available (missing loader/ICD?)");
	std::vector<const char *> extensions(glfwExts, glfwExts + glfwExtCount);

	VkInstanceCreateInfo createInfo{};

	// Portability (MoltenVK) : only when the loader exposes it
	if(hasInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
		{
			extensions.push_back(
			  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			createInfo.flags
			  |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
			// Required by VK_KHR_portability_subset on a Vulkan 1.0 instance
			if(hasInstanceExtension(
				 VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
				extensions.push_back(
				  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		}
	if(enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount
	  = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	if(enableValidationLayers)
		{
			createInfo.enabledLayerCount
			  = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}

	VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
	if(res != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vulkan instance (VkResult "
								 + std::to_string(res) + ")");
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

					VkPhysicalDeviceProperties props;
					vkGetPhysicalDeviceProperties(d, &props);
					LOG_SUCCESS("GPU Selected: " << props.deviceName);
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
	VkPhysicalDeviceFeatures supported{};
	vkGetPhysicalDeviceFeatures(m_physicalDevice, &supported);
	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = supported.samplerAnisotropy;
	m_anisotropy = supported.samplerAnisotropy == VK_TRUE;
	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	ci.pQueueCreateInfos = queueInfos.data();
	std::vector<const char *> extensions(deviceExtensions.begin(),
										 deviceExtensions.end());
	if(hasDeviceExtension(m_physicalDevice, kPortabilitySubsetExt))
		extensions.push_back(kPortabilitySubsetExt);

	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	ci.ppEnabledExtensionNames = extensions.data();
	if(vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device) != VK_SUCCESS)
		throw std::runtime_error("Failed to create logical device");
	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0,
					 &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0,
					 &m_presentQueue);
}

// ── Memory Type ─────────────────────────────────────────────────
std::optional<uint32_t>
VkContext::tryFindMemoryType(uint32_t typeFilter,
							 VkMemoryPropertyFlags props) const
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
	for(uint32_t i = 0; i < memProps.memoryTypeCount; i++)
		if((typeFilter & (1 << i))
		   && (memProps.memoryTypes[i].propertyFlags & props) == props)
			return i;
	return std::nullopt;
}

uint32_t VkContext::findMemoryType(uint32_t typeFilter,
								   VkMemoryPropertyFlags props) const
{
	auto type = tryFindMemoryType(typeFilter, props);
	if(!type)
		throw std::runtime_error("Failed to find suitable memory type");
	return *type;
}

// ── Cleanup ─────────────────────────────────────────────────────
void VkContext::cleanup()
{
	vkDestroyCommandPool(m_device, m_transientPool, nullptr);
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
