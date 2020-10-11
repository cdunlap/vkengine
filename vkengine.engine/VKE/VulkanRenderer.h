#pragma once

#include <vulkan/vulkan.h>

namespace VKE
{
	class Platform;
	
	class VulkanRenderer
	{
	public:
		VulkanRenderer(Platform* platform);
		~VulkanRenderer();

	private:
		Platform* _platform;
		
		VkInstance _instance;
		VkDebugUtilsMessengerEXT _debugMessenger;
		VkPhysicalDevice _physicalDevice;
		VkDevice _device;
	};
}

#include "assert.h"
#define VK_CHECK(expr) do { \
	ASSERT(expr == VK_SUCCESS); \
} while(0)