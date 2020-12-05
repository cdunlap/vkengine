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
		VkPhysicalDevice SelectPhysicalDevice() const;
		static bool PhysicalDeviceMeetsRequirements(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
		static void DetectQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, int32_t* graphicsQueueIndex, int32_t* presentationQueueIndex);
		static VulkanSwapchainSupport QuerySwapchainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
		void CreateLogicalDevice(std::vector<const char *> & requiredValidationLayers);
		char* ReadShaderFile(const char* filename, const char* shaderType, uint64_t* fileSize) const;
		void CreateShader(const char* name);
		void CreateSwapchain();
		void CreateSwapchainImagesAndViews();
		void CreateRenderPass();

		Platform* _platform;
		
		VkInstance _instance;
		VkDebugUtilsMessengerEXT _debugMessenger;
		VkPhysicalDevice _physicalDevice;
		VkDevice _device;
		VkSurfaceKHR _surface;
		VkQueue _graphicsQueue;
		int32_t _graphicsQueueIndex = -1;
		VkQueue _presentationQueue;
		int32_t _presentationQueueIndex = -1;

		uint64_t _shaderStageCount;
		std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

		VkSurfaceFormatKHR _swapchainImageFormat;
		VkExtent2D _swapchainExtent;
		VkSwapchainKHR _swapchain;
		std::vector<VkImage> _swapchainImages;
		std::vector<VkImageView> _swapchainImageViews;
		VkRenderPass _renderPass;
	};
}

#include "vke_assert.h"
#define VK_CHECK(expr) do { \
	ASSERT(expr == VK_SUCCESS); \
} while(0)