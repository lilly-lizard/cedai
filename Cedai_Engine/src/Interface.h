#pragma once

#include "tools/Inputs.h"

#include <SDL.h>
#include <map>

class Interface {
public:
	void init(int screen_width, int screen_height);

	void processEvents();
	inline uint32_t getKeyInputs() { return inputs; }
	inline void getMouseChange(long &mouseX, long &mouseY) { mouseX = this->mouseX; mouseY = this->mouseY; }

	void draw(float *pixels);

	void cleanUp();

private:

	void mapKeys();

	int screen_width;
	int screen_height;

	SDL_Window *win;
	SDL_Renderer *ren;
	SDL_Texture *tex;
	uint8_t *rgba8_pixels;

	std::map<SDL_Keycode, CD_INPUTS> keyBindings;
	uint32_t inputs;
	long mouseX = 0;
	long mouseY = 0;
};