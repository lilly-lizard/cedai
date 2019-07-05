#include <iostream>
#include <SDL.h>

#include "c_sdl.h"
#include "opencl.h"

void run_sdl() {
	
	// start up sdl
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return;
	}

	// create a window with title "Hello World" at 100, 100 on the screen with w:640 h:480 and show it
	SDL_Window *win = SDL_CreateWindow("Hello World!", 100, 100, 640, 480, SDL_WINDOW_SHOWN);
	if (win == nullptr) {
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return;
	}

	// create a renderer that will draw to the window, -1 specifies that we want to load whichever
	// video driver supports the flags we're passing
	// flags: SDL_RENDERER_ACCELERATED: We want to use hardware accelerated rendering
	// SDL_RENDERER_PRESENTVSYNC: We want the renderer's present function (update screen) to be
	// synchronized with the monitor's refresh rate
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr) {
		SDL_DestroyWindow(win);
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return;
	}

	// SDL 2.0 now uses textures to draw things but SDL_LoadBMP returns a surface
	// this lets us choose when to upload or remove textures from the GPU
	SDL_Surface *bmp = SDL_LoadBMP("hello.bmp");
	if (bmp == nullptr) {
		SDL_DestroyRenderer(ren);
		SDL_DestroyWindow(win);
		std::cout << "SDL_LoadBMP Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return;
	}

	// to use a hardware accelerated texture for rendering we can create one from
	// the surface we loaded
	SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, bmp);
	SDL_FreeSurface(bmp); // we no longer need the surface
	if (tex == nullptr) {
		SDL_DestroyRenderer(ren);
		SDL_DestroyWindow(win);
		std::cout << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return;
	}

	// a sleepy rendering loop, wait for 3 seconds and render and present the screen each time
	for (int i = 0; i < 3; ++i) {
		// clear the renderer
		SDL_RenderClear(ren);
		// draw the texture
		SDL_RenderCopy(ren, tex, NULL, NULL);
		// update the screen
		SDL_RenderPresent(ren);
		// take a quick break after all that hard work
		SDL_Delay(1000);
	}

	// clean up our objects and quit
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	
	//return 0;
}