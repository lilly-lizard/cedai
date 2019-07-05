#pragma once

#include <SDL.h>

class Interface {
public:
	void init(int screen_width, int screen_height);

	bool processEvents();
	void draw(float *pixels);

	void cleanUp();

private:

	int screen_width;
	int screen_height;

	SDL_Window *win;
	SDL_Renderer *ren;
};