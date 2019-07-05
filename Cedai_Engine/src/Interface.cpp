#include <iostream>

#include "Interface.h"

void Interface::init(int screen_width, int screen_height) {
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
}

void Interface::draw(std::vector<float> &pixels) {
	// clear the renderer
	SDL_RenderClear(ren);
	// draw pixels
	for (int x = 0; x < screen_width; x++) {
		for (int y = 0; y < screen_height; y++) {
			//SDL_SetRenderDrawColor(ren, 255, 255 * x / screen_width, 255 * y / screen_height, 255);
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

bool Interface::processEvents() {
	bool quit = false;
	SDL_Event e;
	// process pending events
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN)
			quit = true;
	}
	return quit;
}

void Interface::cleanUp() {
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
}