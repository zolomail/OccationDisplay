#include "ClearScreen.h"

ClearScreen::ClearScreen(Adafruit_GFX &matrix, uint8_t r, uint8_t g, uint8_t b) :
	Effect(matrix),
	m_r(r),
	m_g(g),
	m_b(b)
{
}

void ClearScreen::render()
{
	m_matrix.fillScreen(color565(m_r, m_g, m_b));
}