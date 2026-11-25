#pragma once

#include "Effect.h"

class ClearScreen : public Effect
{
public:
	ClearScreen(Adafruit_GFX &matrix, uint8_t r=0, uint8_t g=0, uint8_t b=0);
	ClearScreen(Adafruit_GFX &matrix, uint16_t color);
	void render();
private:
	uint16_t m_color;
};
