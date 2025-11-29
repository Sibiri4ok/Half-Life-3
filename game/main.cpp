#include "core/engine.h"
#include "loops/menu_loop.h"
#include <memory>

#ifdef __linux__
#include <X11/Xlib.h>
#endif

int main() {
#ifdef __linux__
	// Initialize X11 threads for multi-threaded SFML usage
	// This is required when using SFML with multiple threads on Linux/X11
	XInitThreads();
#endif

	auto loop = std::make_unique<MenuLoop>();
	engine::Engine *e = engine::Engine::withLoop(std::move(loop));
	e->run();

	return 0;
}
