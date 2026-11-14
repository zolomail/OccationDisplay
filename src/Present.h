#pragma once

#include "Effect.h"

class Present : public Effect
{
public:
	Present(Adafruit_GFX &matrix, int16_t _y, int16_t _w=5, int16_t _h=5, uint16_t _color=0xFFFF, uint16_t _ribbon=0xFFFF);

	void render();

private:
	int16_t w, h;
	int16_t x_pos;
	int16_t y_pos;
	uint16_t color;
	uint16_t ribbon;
};