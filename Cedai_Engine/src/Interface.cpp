
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

	win = SDL_CreateWindow("Cedai", 300, 100, screen_width, screen_height, SDL_WINDOW_SHOWN);
	if (win == nullptr) {
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return;
	}

	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED); //SDL_RENDERER_PRESENTVSYNC
	if (ren == nullptr) {
		SDL_DestroyWindow(win);
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return;
	}

	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, screen_width, screen_height);

	SDL_SetRelativeMouseMode(SDL_TRUE);
	mapKeys();
}

void Interface::mapKeys() {
	keyBindings[SDLK_ESCAPE] = CD_INPUTS::ESC;

	keyBindings[SDLK_w] = CD_INPUTS::UP;
	keyBindings[SDLK_a] = CD_INPUTS::LEFT;
	keyBindings[SDLK_s] = CD_INPUTS::DOWN;
	keyBindings[SDLK_d] = CD_INPUTS::RIGHT;
	keyBindings[SDLK_q] = CD_INPUTS::ROTATEL;
	keyBindings[SDLK_e] = CD_INPUTS::ROTATER;

	//keyBindings[SDLK_UP] = CD_INPUTS::UP;
	//keyBindings[SDLK_DOWN] = CD_INPUTS::DOWN;
	//keyBindings[SDLK_LEFT] = CD_INPUTS::LEFT;
	//keyBindings[SDLK_RIGHT] = CD_INPUTS::RIGHT;
	//keyBindings[SDLK_PAGEUP] = CD_INPUTS::ROTATEL;
	//keyBindings[SDLK_PAGEDOWN] = CD_INPUTS::ROTATER;

	keyBindings[SDLK_SPACE] = CD_INPUTS::FORWARD;
	keyBindings[SDLK_LSHIFT] = CD_INPUTS::BACKWARD;

	keyBindings[SDLK_z] = CD_INPUTS::INTERACTL;
	keyBindings[SDLK_x] = CD_INPUTS::INTERACTR;
}

void Interface::draw(uint8_t* pixels) {

	// set texture pixels
	void *tex_pixels;
	int pitch;
	SDL_LockTexture(tex, NULL, &tex_pixels, &pitch);
	SDL_memcpy(tex_pixels, pixels, sizeof(uint8_t) * screen_width * screen_height * 4);
	SDL_UnlockTexture(tex);

	// copy texture to screen
	SDL_RenderClear(ren);
	SDL_RenderCopy(ren, tex, NULL, NULL);
	SDL_RenderPresent(ren);
}

void Interface::processEvents() {
	// process pending events
	inputs = 0;
	SDL_Event e;
	bool mouse = false;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) {
			inputs |= CD_INPUTS::QUIT;
		}

		if (e.type == SDL_KEYDOWN) {
			for (auto&[key, input] : keyBindings) {
				if (e.key.keysym.sym == key)
					inputs |= input;
			}
		}
		else if (e.type == SDL_KEYUP) {
			for (auto&[key, input] : keyBindings) {
				if (e.key.keysym.sym == key)
					inputs &= !input;
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
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
}

/*
notes:
sld renderer explained: https://stackoverflow.com/questions/21007329/what-is-an-sdl-renderer
opencl sdl: https://forums.libsdl.org/viewtopic.php?t=11801
opengl: https://www.gamedev.net/forums/topic/677152-textures-with-sdl2-opengl/
*/