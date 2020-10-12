#include "Engine.h"
#include "Platform.h"
#include "VulkanRenderer.h"

namespace VKE
{
	Engine::Engine(const char* applicationName)
	{
		_platform = new Platform(this, applicationName);
		_renderer = new VulkanRenderer(_platform);
	}

	Engine::~Engine()
	{
		
	}

	void Engine::Run()
	{
		_platform->StartGameLoop();
	}

	void Engine::OnLoop(const float32_t deltaTime)
	{
		
	}

}