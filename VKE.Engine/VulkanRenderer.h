#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace VKE
{
	struct VulkanSwapchainSupport
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentationModes;
	};
	
	class Platform;
	
	class VulkanRenderer
	{
	public:
		VulkanRenderer(Platform* platform);
		~VulkanRenderer();

	private:
		VkPhysicalDevice SelectPhysicalDevice();
		static bool PhysicalDeviceMeetsRequirements(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
		static void DetectQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, int32_t* graphicsQueueIndex, int32_t* presentationQueueIndex);
		static VulkanSwapchainSupport QuerySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
		
		Platform* _platform;
		
		VkInstance _instance{};
		VkDebugUtilsMessengerEXT _debugMessenger{};
		VkPhysicalDevice _physicalDevice{};
		VkDevice _device{};
		VkSurfaceKHR _surface{};
	};
}

#include "vke_assert.h"
#define VK_CHECK(expr) do { \
	ASSERT(expr == VK_SUCCESS); \
} while(0)