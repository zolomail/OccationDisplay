#pragma once

#include "Effect.h"

class Star
{
public:
	Star();
	Star(const Star &f);
	Star(int16_t max_x, int16_t max_y);
	void reset(int16_t max_x, int16_t max_y=0);
	int16_t x;
	int16_t y;
	int16_t s;
	int16_t f;
};


class Stars : public Effect
{
public:
	Stars(Adafruit_GFX &matrix, int16_t stars=20);
	void render();
private:
	std::vector<Star> m_stars;
};