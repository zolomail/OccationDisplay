#include "ClearScreen.h"

ClearScreen::ClearScreen(Adafruit_GFX &matrix, uint8_t r, uint8_t g, uint8_t b) :
	Effect(matrix),
	m_color(color565(r, g, b))
{
}

ClearScreen::ClearScreen(Adafruit_GFX &matrix, uint16_t color) :
	Effect(matrix),
	m_color(color)
{
}

void ClearScreen::render()
{
	m_matrix.fillScreen(m_color);
}