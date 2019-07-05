#pragma once

#include "Interface.h"
#include "Renderer.h"

class Cedai {
public:
	void Run();

private:
	void init();
	void loop();
	void cleanUp();

	Interface interface;
	Renderer renderer;
};