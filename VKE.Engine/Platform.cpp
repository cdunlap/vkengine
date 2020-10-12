#include "Platform.h"
#include "Engine.h"
#include "Logger.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "VulkanRenderer.h"

namespace VKE
{
	Platform::Platform(Engine* engine, const char* applicationName)
		: _window(nullptr), _engine(engine)
	{
		Logger::Trace("Init platform layer");

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window = glfwCreateWindow(1280, 720, applicationName, nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
	}

	Platform::~Platform()
	{
		if(_window)
		{
			glfwDestroyWindow(_window);
			_window = nullptr;
		}
		glfwTerminate();
	}

	void Platform::GetRequiredExtensions(uint32_t* extensionCount, const char*** extensionNames)
	{
		*extensionNames = glfwGetRequiredInstanceExtensions(extensionCount);
	}


	bool Platform::StartGameLoop() const
	{
		while(!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
			_engine->OnLoop(0.0f);
		}

		return true;
	}

	void Platform::CreateSurface(VkInstance instance, VkSurfaceKHR* surface) const
	{
		VK_CHECK(glfwCreateWindowSurface(instance, _window, nullptr, surface));
	}

}
