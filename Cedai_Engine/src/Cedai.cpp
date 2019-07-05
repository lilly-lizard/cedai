#include "Cedai.h"

#include "Interface.h"

#include <iostream>
#include <chrono>

const int screen_width = 640;
const int screen_height = 480;

#undef main // sdl2 defines main for some reason
void main() {
	Cedai App;
	App.Run();
}

void Cedai::Run() {
	init();

	loop();

	cleanUp();
}

void Cedai::init() {
	renderer.init(screen_width, screen_height);
	interface.init(screen_width, screen_height);
}

void Cedai::loop() {
	bool quit = false;
	std::vector<float> pixels;
	std::chrono::time_point<std::chrono::system_clock> prevTime;
	std::chrono::duration<double> elapsedTime;

	while (!quit) {
		prevTime = std::chrono::system_clock::now();
		renderer.draw(pixels);
		elapsedTime = std::chrono::system_clock::now() - prevTime;
		std::cout << "render took: " << elapsedTime.count() << "s" << std::endl;
		prevTime = std::chrono::system_clock::now();
		interface.draw(pixels);
		quit = interface.processEvents();
		elapsedTime = std::chrono::system_clock::now() - prevTime;
		std::cout << "draw took: " << elapsedTime.count() << "s" << std::endl;
	}
}

void Cedai::cleanUp() {
	renderer.cleanUp();
	interface.cleanUp();
}