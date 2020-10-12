#pragma once

#include <vulkan/vulkan_core.h>

#include "vke_types.h"

struct GLFWwindow;

namespace VKE {
	class Engine;
	
	class Platform
	{
	public:
		Platform(Engine * engine, const char * applicationName);
		~Platform();

		GLFWwindow* GetWindow() const { return _window; }
		static void GetRequiredExtensions(uint32_t* extensionCount, const char*** extensionNames);
		
		bool StartGameLoop() const;

		void CreateSurface(VkInstance instance, VkSurfaceKHR* surface) const;

	private:
		GLFWwindow* _window;
		Engine* _engine;
	};
}