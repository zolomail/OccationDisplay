#pragma once

#include "Effect.h"

class Circle : public Effect
{
public:
	Circle(Adafruit_GFX &matrix, int16_t x=50, int16_t y=15, int16_t r=15, uint16_t color = 0xFFFF);
	void render();
private:
	int16_t m_x, m_y, m_r, m_dx, m_dy;
	uint16_t m_color;
};
