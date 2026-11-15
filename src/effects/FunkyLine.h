#pragma once

#include "Effect.h"

class FunkyLine : public Effect
{
public:
	FunkyLine(Adafruit_GFX &matrix, int16_t angle=0, int16_t da=1, uint16_t color = 0xFFFF);
	void render();
private:
	int16_t m_angle, m_da;
	uint16_t m_color;
};
