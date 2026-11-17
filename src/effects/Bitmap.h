#pragma once

#include "Effect.h"

class Bitmap : public Effect
{

public:
	Bitmap(Adafruit_GFX &matrix, const uint16_t &bitmap, int16_t width, int16_t height, int16_t x=XPOS_CENTER, int16_t y=YPOS_CENTER, int16_t dx=0, int16_t dy=0, const unsigned char *mask = NULL);
	void render();

protected:
	const uint16_t &m_bitmap;
	uint16_t m_width, m_height;
	int16_t m_x, m_y, m_dx, m_dy;
	const unsigned char *m_mask;
};