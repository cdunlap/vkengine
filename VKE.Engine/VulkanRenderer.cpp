#include "Platform.h"
#include "Logger.h"

#include "VulkanRenderer.h"

#include <vector>

namespace VKE
{	
	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRendererDebugCallback (
		VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		switch(messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			Logger::Warn(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			Logger::Info(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			Logger::Trace(pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		default:
			Logger::Error(pCallbackData->pMessage);
			break;
		}

		return VK_FALSE;
	}
	
	VulkanRenderer::VulkanRenderer(Platform* platform)
		: _platform(platform), _physicalDevice(nullptr), _device(nullptr)
	{
		Logger::Trace("VulkanRenderer()");

		VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.apiVersion = VK_API_VERSION_1_2;
		appInfo.pApplicationName = "VKE";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

		VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		instanceCreateInfo.pApplicationInfo = &appInfo;

		// Extensions
		const char** pfe = nullptr;
		uint32_t count = 0;
		Platform::GetRequiredExtensions(&count, &pfe);
		std::vector<const char*> platformExtensions;
		for (uint32_t i = 0; i < count; ++i) {
			platformExtensions.push_back(pfe[i]);
		}
		platformExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(platformExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = platformExtensions.data();

		// Validation layers
		std::vector<const char*> requiredValidationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		// Get available layers.
		uint32_t availableLayerCount = 0;
		VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr));
		std::vector<VkLayerProperties> availableLayers(availableLayerCount);
		VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data()));

		// Verify that all required layers are available.
		bool success = true;
		for (auto& requiredValidationLayer : requiredValidationLayers)
		{
			bool found = false;
			for (auto & availableLayer : availableLayers) {
				if (strcmp(requiredValidationLayer, availableLayer.layerName) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				success = false;
				Logger::Fatal("Required validation layer is missing: %s", requiredValidationLayer);
				break;
			}
		}

		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();

		// Create instance
		VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &_instance));

		// Debugger
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugCreateInfo.pfnUserCallback = VulkanRendererDebugCallback;
		debugCreateInfo.pUserData = this;

		const auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT"));
		ASSERT_MSG(fn, "Failed to create debug messenger");
		fn(_instance, &debugCreateInfo, nullptr, &_debugMessenger);

		// Surface
		_platform->CreateSurface(_instance, &_surface);

		// Physical device
		_physicalDevice = SelectPhysicalDevice();

		// Logical device
		CreateLogicalDevice(requiredValidationLayers);
	}

	VulkanRenderer::~VulkanRenderer()
	{
		if(_debugMessenger)
		{
			const auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
				_instance, "vkDestroyDebugUtilsMessengerEXT"));
			fn(_instance, _debugMessenger, nullptr);
		}
		vkDestroyInstance(_instance, nullptr);
	}

	VkPhysicalDevice VulkanRenderer::SelectPhysicalDevice() const
	{
		uint32_t deviceCount = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr));
		ASSERT_MSG(deviceCount != 0, "No supported physical devices found.");
		std::vector<VkPhysicalDevice> devices(deviceCount);
		VK_CHECK(vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data()));

		for(auto &device : devices)
		{
			if(VulkanRenderer::PhysicalDeviceMeetsRequirements(device, _surface))
			{
				return device;
			}
		}
		Logger::Fatal("No devices that meet engine requirements found");
		return nullptr;
	}

	bool VulkanRenderer::PhysicalDeviceMeetsRequirements(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
	{
		int32_t graphicsQueueIndex = -1;
		int32_t presentationQueueIndex = -1;
		DetectQueueFamilyIndices(physicalDevice, surface, &graphicsQueueIndex, &presentationQueueIndex);

		VulkanSwapchainSupport swapchainSupport = VulkanRenderer::QuerySwapchainSupport(physicalDevice, surface);

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);

		bool supportsRequiredQueueFamilies = (graphicsQueueIndex != -1) && (presentationQueueIndex != -1);

		// Extension support
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

		// Required extensions
		std::vector<const char*> requiredExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		bool success = true;
		for(auto & requiredExtension : requiredExtensions)
		{
			bool found = false;
			for(auto & availableExtension : availableExtensions)
			{
				if(strcmp(requiredExtension, availableExtension.extensionName) == 0)
				{
					found = true;
					break;
				}

				if(!found)
				{
					success = false;
					break;
				}
			}
		}

		bool swapChainMeetsReq = false;
		if(supportsRequiredQueueFamilies)
		{
			swapChainMeetsReq = !swapchainSupport.Formats.empty() &&
				!swapchainSupport.PresentationModes.empty();
		}

        // NOTE: Could also look for discrete GPU. We could score and rank them based on features and capabilities.
		return supportsRequiredQueueFamilies && swapChainMeetsReq && features.samplerAnisotropy;
	}

	void VulkanRenderer::DetectQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, int32_t* graphicsQueueIndex, int32_t* presentationQueueIndex)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> familyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, familyProperties.data());

		for(uint32_t i = 0; i < familyProperties.size(); i++)
		{
			if(familyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				*graphicsQueueIndex = i;
			}

			VkBool32 supportsPresentation = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresentation);
			if(supportsPresentation)
			{
				*presentationQueueIndex = i;
			}
		}
	}

	VulkanSwapchainSupport VulkanRenderer::QuerySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
	{
		VulkanSwapchainSupport support;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &support.Capabilities);
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
		if(formatCount != 0)
		{
			support.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, support.Formats.data());
		}

		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
		if(presentModeCount != 0)
		{
			support.PresentationModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, support.PresentationModes.data());
		}

		return support;
	}

	void VulkanRenderer::CreateLogicalDevice(std::vector<const char*>& requiredValidationLayers)
	{
		int32_t graphicsQueueIndex = -1;
		int32_t presentationQueueIndex = -1;
		VulkanRenderer::DetectQueueFamilyIndices(_physicalDevice, _surface, &graphicsQueueIndex, &presentationQueueIndex);

		const uint32_t indexCount = 2;
		uint32_t indices[indexCount] = {
			static_cast<uint32_t>(graphicsQueueIndex),
			static_cast<uint32_t>(presentationQueueIndex)
		};

		VkDeviceQueueCreateInfo queueCreateInfo[indexCount];
		for(uint32_t i = 0; i < indexCount; i++)
		{
			queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo[i].queueFamilyIndex = indices[i];
			queueCreateInfo[i].queueCount = 1;
			queueCreateInfo[i].flags = 0;
			queueCreateInfo[i].pNext = nullptr;
			float32_t queuePriority = 1.0f;
			queueCreateInfo[i].pQueuePriorities = &queuePriority;			
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		// TODO: Disable on release builds
		VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = indexCount;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        deviceCreateInfo.enabledExtensionCount = 1;
        deviceCreateInfo.pNext = nullptr;
        const char* requiredExtensions[1] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        deviceCreateInfo.ppEnabledExtensionNames = requiredExtensions;
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();

		// Create device
		VK_CHECK(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device));

		_graphicsQueueIndex = graphicsQueueIndex;
		_presentationQueueIndex = presentationQueueIndex;

		// Create queues
		vkGetDeviceQueue(_device, _graphicsQueueIndex, 0, &_graphicsQueue);
		vkGetDeviceQueue(_device, _presentationQueueIndex, 0, &_presentationQueue);
	}

}
