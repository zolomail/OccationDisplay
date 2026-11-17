#include "FlyingText.h"

FlyingText::FlyingText(Adafruit_GFX &matrix, String text, int16_t x, int16_t y, int16_t dx, int16_t dy, uint16_t color, uint8_t font) :
	Effect(matrix),
	m_text(text),
	m_x(x),
	m_y(y),
	m_dx(dx),
	m_dy(dy),
	m_color(color),
	m_font(font)
{
	int16_t x1 = 0, y1 = 0;
	setFont(m_font);
	m_matrix.getTextBounds(m_text, 0, 0, &x1, &y1, &m_width, &m_height);
	m_height -= 2;
	m_x = resolveX(m_x, m_width) - x1;
	m_y = resolveY(m_y, m_height) - y1;
}

void FlyingText::render()
{
	setFont(m_font);
	m_matrix.setTextWrap(false);
	m_matrix.setTextColor(m_color);
	m_matrix.setCursor(m_x, m_y);
	m_matrix.print(m_text.c_str());

	if(m_x + m_dx + m_width > m_matrix.width() || m_x + m_dx < 0)
	{
		m_dx = -m_dx;
	}
	if(m_y + m_dy + m_height > m_matrix.height() || m_y + m_dy < 0)
	{
		m_dy = -m_dy;
	}
	m_x += m_dx;
	m_y += m_dy;
}