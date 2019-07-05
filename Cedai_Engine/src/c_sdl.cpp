#include <iostream>
#include <SDL.h>

#include "c_sdl.h"
#include "opencl.h"

const int SCREEN_WIDTH  = 640;
const int SCREEN_HEIGHT = 480;

void run_sdl() {

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return;
	}

	SDL_Window *win = SDL_CreateWindow("Cedai", 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (win == nullptr) {
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return;
	}

	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr) {
		SDL_DestroyWindow(win);
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return;
	}

	//Our event structure
	SDL_Event e;
	bool quit = false;
	while (!quit) {
		// clear the renderer
		SDL_RenderClear(ren);
		// draw pixels
		for (int x = 0; x < SCREEN_WIDTH; x++) {
			for (int y = 0; y < SCREEN_HEIGHT; y++) {
				SDL_SetRenderDrawColor(ren, 255, 255 * x / SCREEN_WIDTH, 255 * y / SCREEN_HEIGHT, 255);
				SDL_RenderDrawPoint(ren, x, y);
			}
		}
		// update the screen
		SDL_RenderPresent(ren);

		// process pending events
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN)
				quit = true;
		}
	}

	// clean up our objects and quit
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
}