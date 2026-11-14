#pragma once

#include "Effect.h"

class ScrollText : public Effect
{

public:
	ScrollText(Adafruit_GFX &matrix, String text, int16_t y=0, int16_t dx=1, uint16_t color = 0xFFFF, uint8_t font=0);
	void setText(String text);
	void render();

protected:
	String m_text;
	uint16_t m_width, m_height;
	int16_t m_x, m_y;
	int16_t m_dx;
	uint16_t m_color;
	uint8_t m_font;
};