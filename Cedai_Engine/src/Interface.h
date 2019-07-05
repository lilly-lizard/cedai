#pragma once

#include <SDL.h>
#include <vector>

class Interface {
public:
	void init(int screen_width, int screen_height);

	bool processEvents();
	void draw(std::vector<float> &pixels);

	void cleanUp();

private:

	int screen_width;
	int screen_height;

	SDL_Window *win;
	SDL_Renderer *ren;
};