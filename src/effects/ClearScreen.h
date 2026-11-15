#pragma once

#include "Effect.h"

class ClearScreen : public Effect
{
public:
	ClearScreen(Adafruit_GFX &matrix, uint8_t r=0, uint8_t g=0, uint8_t b=0);
	void render();
private:
	uint8_t m_r, m_g, m_b;
};
