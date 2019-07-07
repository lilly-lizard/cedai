
#include "Interface.h"
#include "tools/Log.h"

#include <iostream>

void Interface::init(int screen_width, int screen_height) {
	CD_INFO("Initialising interface...");

	this->screen_width = screen_width;
	this->screen_height = screen_height;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return;
	}

	win = SDL_CreateWindow("Cedai", 100, 100, screen_width, screen_height, SDL_WINDOW_SHOWN);
	if (win == nullptr) {
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return;
	}

	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr) {
		SDL_DestroyWindow(win);
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return;
	}

	SDL_SetRelativeMouseMode(SDL_TRUE);
	mapKeys();
}

void Interface::mapKeys() {
	keyBindings[SDLK_ESCAPE] = CD_INPUTS::ESC;
	keyBindings[SDLK_w] = CD_INPUTS::FORWARD;
}

void Interface::draw(float *pixels) {
	// clear the renderer
	SDL_RenderClear(ren);
	// draw pixels
	for (int x = 0; x < screen_width; x++) {
		for (int y = 0; y < screen_height; y++) {
			SDL_SetRenderDrawColor(ren, 255 * pixels[(x + y * screen_width) * 3],
										255 * pixels[(x + y * screen_width) * 3 + 1],
										255 * pixels[(x + y * screen_width) * 3 + 2],
										255);
			SDL_RenderDrawPoint(ren, x, y);
		}
	}
	// update the screen
	SDL_RenderPresent(ren);
}

void Interface::processEvents() {
	// process pending events
	inputs = 0;
	SDL_Event e;
	bool mouse = false;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT || e.type == SDL_MOUSEBUTTONDOWN) {
			inputs |= CD_INPUTS::QUIT;
		}
		if (e.type == SDL_KEYDOWN) {
			for (auto&[key, input] : keyBindings) {
				if (e.key.keysym.sym == key)
					inputs |= input;
			}
		}
		if (e.type == SDL_MOUSEMOTION) {
			mouseX = e.motion.xrel;
			mouseY = e.motion.yrel;
			mouse = true;
		}
	}
	if (!mouse) {
		mouseX = 0;
		mouseY = 0;
	}
}

void Interface::cleanUp() {
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
}