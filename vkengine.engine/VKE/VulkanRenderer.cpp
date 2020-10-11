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
		: _platform(platform)
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

		instanceCreateInfo.enabledExtensionCount = (uint32_t)platformExtensions.size();
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

		instanceCreateInfo.enabledLayerCount = (uint32_t)requiredValidationLayers.size();
		instanceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();

		// Create instance
		VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &_instance));

		// Debugger
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugCreateInfo.pfnUserCallback = VulkanRendererDebugCallback;
		debugCreateInfo.pUserData = this;

		PFN_vkCreateDebugUtilsMessengerEXT fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
		ASSERT_MSG(fn, "Failed to create debug messenger");
		fn(_instance, &debugCreateInfo, nullptr, &_debugMessenger);
	}

	VulkanRenderer::~VulkanRenderer()
	{
		if(_debugMessenger)
		{
			PFN_vkDestroyDebugUtilsMessengerEXT fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
			fn(_instance, _debugMessenger, nullptr);
		}
		vkDestroyInstance(_instance, nullptr);
	}
}
