// Next: https://youtu.be/wT2ChABGLL4?t=5112

#include "Platform.h"
#include "Logger.h"

#include "VulkanRenderer.h"

#include <vector>
#include <fstream>

#include <glm/glm.hpp>

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

		// Create the basic shader
		CreateShader("main");

		CreateSwapchain();
		CreateSwapchainImagesAndViews();
		CreateRenderPass();
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

		std::vector<uint32_t> queueIndices;
		queueIndices.push_back(graphicsQueueIndex);
		if (graphicsQueueIndex != presentationQueueIndex) {
			queueIndices.push_back(presentationQueueIndex);
		}

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueIndices.size());
		for(uint32_t i = 0; i < queueIndices.size(); i++)
		{
			queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfos[i].queueFamilyIndex = queueIndices[i];
			queueCreateInfos[i].queueCount = 1;
			queueCreateInfos[i].flags = 0;
			queueCreateInfos[i].pNext = nullptr;
			float32_t queuePriority = 1.0f;
			queueCreateInfos[i].pQueuePriorities = &queuePriority;
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		// TODO: Disable on release builds
		VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
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

	char* VulkanRenderer::ReadShaderFile(const char* filename, const char* shaderType, uint64_t* fileSize) const
	{
		ASSERT_MSG(shaderType == "frag" || shaderType == "vert", "Unexpected shader type string");
		char buffer[256];
		uint32_t length = snprintf(buffer, sizeof(buffer), "shaders/%s.%s.spv", filename, shaderType);

		if (length < 0) {
			Logger::Fatal("Shader filename is too long");
		}

		std::ifstream file(buffer, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			Logger::Fatal("Unable to open shader file %s.", buffer);
		}

		*fileSize = (uint64_t)file.tellg();
		char* fb = (char*)malloc(*fileSize); // REMEMBER TO FREE THIS!
		file.seekg(0);
		file.read(fb, *fileSize);
		file.close();

		return fb;
	}

	void VulkanRenderer::CreateShader(const char* name)
	{
		// Vert shader
		uint64_t vertShaderSize;
		char* vertShaderSrc = ReadShaderFile(name, "vert", &vertShaderSize);
		VkShaderModuleCreateInfo vertShaderCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		vertShaderCreateInfo.codeSize = vertShaderSize;
		vertShaderCreateInfo.pCode = (uint32_t *)vertShaderSrc;
		VkShaderModule vertShaderModule;
		VK_CHECK(vkCreateShaderModule(_device, &vertShaderCreateInfo, nullptr, &vertShaderModule));

		// Frag Shader
		uint64_t fragShaderSize;
		char* fragShaderSrc = ReadShaderFile(name, "frag", &fragShaderSize);
		VkShaderModuleCreateInfo fragShaderCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		fragShaderCreateInfo.codeSize = fragShaderSize;
		fragShaderCreateInfo.pCode = (uint32_t*)fragShaderSrc;
		VkShaderModule fragShaderModule;
		VK_CHECK(vkCreateShaderModule(_device, &fragShaderCreateInfo, nullptr, &fragShaderModule));

		// Vert shader stage
		VkPipelineShaderStageCreateInfo vertShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		// Frag shader stage
		VkPipelineShaderStageCreateInfo fragShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		_shaderStageCount = 2;
		_shaderStages.push_back(vertShaderStageInfo);
		_shaderStages.push_back(fragShaderStageInfo);

		free(vertShaderSrc);
		free(fragShaderSrc);
	}

	void VulkanRenderer::CreateSwapchain()
	{
		VulkanSwapchainSupport swapchainSupport = VulkanRenderer::QuerySwapchainSupport(_physicalDevice, _surface);
		VkSurfaceCapabilitiesKHR capabilities = swapchainSupport.Capabilities;

		// Surface format
		bool found = false;
		for (auto format : swapchainSupport.Formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				_swapchainImageFormat = format;
				found = true;
				break;
			}
		}

		if (!found) {
			_swapchainImageFormat = swapchainSupport.Formats[0];
		}

		// Presentation Mode
		VkPresentModeKHR presentMode;
		found = false;
		for (auto mode : swapchainSupport.PresentationModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				presentMode = mode;
				found = true;
			}
		}

		if (!found) {
			presentMode = VK_PRESENT_MODE_FIFO_KHR;
		}

		// Swapchain extent
		if (capabilities.currentExtent.width != UINT32_MAX) {
			_swapchainExtent = capabilities.currentExtent;
		}
		else {
			Extent2D extent = _platform->GetFramebufferExtent();
			_swapchainExtent = { (uint32_t)extent.width, (uint32_t)extent.height };

			// Clamp to max allowed by GPU
			_swapchainExtent.width = glm::clamp(_swapchainExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			_swapchainExtent.height = glm::clamp(_swapchainExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
			imageCount = capabilities.maxImageCount;
		}

		// Swapchain create info
		VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		swapchainCreateInfo.surface = _surface;
		swapchainCreateInfo.minImageCount = imageCount;
		swapchainCreateInfo.imageFormat = _swapchainImageFormat.format;
		swapchainCreateInfo.imageColorSpace = _swapchainImageFormat.colorSpace;
		swapchainCreateInfo.imageExtent = _swapchainExtent;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// Setup queue family indices
		if (_graphicsQueueIndex != _presentationQueueIndex) {
			uint32_t queueIndices[2] = { (uint32_t)_graphicsQueueIndex, (uint32_t)_presentationQueueIndex };
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = queueIndices;
		}
		else {
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchainCreateInfo.queueFamilyIndexCount = 0;
			swapchainCreateInfo.pQueueFamilyIndices = nullptr;
		}

		swapchainCreateInfo.preTransform = capabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = presentMode;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = nullptr; // Used after window resize, no need now

		VK_CHECK(vkCreateSwapchainKHR(_device, &swapchainCreateInfo, nullptr, &_swapchain));
	}

	void VulkanRenderer::CreateSwapchainImagesAndViews()
	{
		uint32_t swapchainImageCount = 0;
		VK_CHECK(vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, nullptr));
		_swapchainImages.resize(swapchainImageCount);
		_swapchainImageViews.resize(swapchainImageCount);
		VK_CHECK(vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, _swapchainImages.data()));

		for (uint32_t i = 0; i < swapchainImageCount; i++) {
			VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewInfo.image = _swapchainImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = _swapchainImageFormat.format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &_swapchainImageViews[i]));
		}
	}

	void VulkanRenderer::CreateRenderPass()
	{
		// Color attachment
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = _swapchainImageFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Color attachment reference
		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Depth attachment
		// Find depth format
		constexpr uint64_t candidateCount = 3;
		constexpr VkFormat candidates[candidateCount] = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		};

		constexpr uint32_t flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkFormat depthFormat = VK_FORMAT_UNDEFINED;
		for (auto candidate : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(_physicalDevice, candidate, &props);
			if ((props.linearTilingFeatures & flags) == flags) {
				depthFormat = candidate;
				break;
			}
			else if ((props.optimalTilingFeatures & flags) == flags) {
				depthFormat = candidate;
				break;
			}
		}

		if (depthFormat == VK_FORMAT_UNDEFINED) {
			Logger::Fatal("Unable to find a supported depth format");
		}
		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Depth attachment reference
		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Subpass
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pDepthStencilAttachment = &depthReference;

		// Render pass dependencies
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		constexpr uint32_t attachmentCount = 2;
		VkAttachmentDescription attachments[attachmentCount] = {
			colorAttachment,
			depthAttachment
		};

		// Render pass create
		VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		renderPassCreateInfo.attachmentCount = attachmentCount;
		renderPassCreateInfo.pAttachments = attachments;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;
		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &dependency;
		VK_CHECK(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &_renderPass));
	}

	void VulkanRenderer::CreateGraphicsPipeline()
	{
		// Viewport
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(_swapchainExtent.height);
		viewport.width = static_cast<float>(_swapchainExtent.width);
		viewport.height = -static_cast<float>(_swapchainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Scissor
		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = { _swapchainExtent.width, _swapchainExtent.height };
		
		// Viewport state
		VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Because we flipped the viewport
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampleState.sampleShadingEnable = VK_FALSE;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.minSampleShading = 1.0f;
		multisampleState.pSampleMask = nullptr;
		multisampleState.alphaToCoverageEnable = VK_FALSE;
		multisampleState.alphaToOneEnable = VK_FALSE;

		// Depth and stencil
		VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		// Color attachment
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendState.logicOpEnable = VK_FALSE;
		colorBlendState.logicOp = VK_LOGIC_OP_COPY;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &colorBlendAttachment;
		colorBlendState.blendConstants[0] = 0.0f;
		colorBlendState.blendConstants[1] = 0.0f;
		colorBlendState.blendConstants[2] = 0.0f;
		colorBlendState.blendConstants[3] = 0.0f;

		// Dynamic state
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicStateCreate = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStateCreate.dynamicStateCount = 2;
		dynamicStateCreate.pDynamicStates = dynamicStates;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		
		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// Pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout));

		// Pipeline create
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineCreateInfo.stageCount = _shaderStageCount;
		pipelineCreateInfo.pStages = _shaderStages.data();
		pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pRasterizationState = &rasterizer;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pDepthStencilState = &depthStencil;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pDynamicState = nullptr; // For now?
		pipelineCreateInfo.layout = _pipelineLayout;
		pipelineCreateInfo.renderPass = _renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		VK_CHECK(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_pipeline));

		Logger::Info("Graphics pipeline created");
	}

}
