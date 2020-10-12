#pragma once

#include "vke_types.h"

namespace VKE {
	class Platform;
	class VulkanRenderer;
	
	class Engine
	{
	public:
		Engine(const char* applicationName);
		~Engine();
		
		void Run();

		void OnLoop(const float32_t deltaTime);
	private:
		Platform* _platform;
		VulkanRenderer* _renderer;
	};
}
