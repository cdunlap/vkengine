#include "Logger.h"
#include "Engine.h"

int main(int argc, const char ** argv) {
	VKE::Logger::Info("Initializing engine %d", 4);

	VKE::Engine* engine = new VKE::Engine("VKE");

	engine->Run();

	delete engine;

	return 0;
}