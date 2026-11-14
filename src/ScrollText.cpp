#include "ScrollText.h"

#define SPEED_FACTOR 10

ScrollText::ScrollText(Adafruit_GFX &matrix, String text, int16_t y, int16_t dx, uint16_t color, uint8_t font) :
	Effect(matrix),
	m_text(text),
	m_x(m_matrix.width()*SPEED_FACTOR),
	m_y(y),
	m_initial_y(y),
	m_dx(dx),
	m_color(color),
	m_font(font)
{
	setText(m_text);
}

void ScrollText::setText(String text)
{
	m_text = text;
	setFont(m_font);
	m_matrix.setTextWrap(false);
	int16_t x1 = 0, y1 = 0;
	m_matrix.getTextBounds(m_text, 0, 0, &x1, &y1, &m_width, &m_height);
	m_y = (m_initial_y < 0) ? (m_matrix.height() - m_height) / 2 : m_initial_y;
	m_y -= y1;
}

void ScrollText::render()
{
	setFont(m_font);
	m_matrix.setTextWrap(false);
	m_matrix.setTextColor(m_color);
	m_matrix.setCursor(m_x/SPEED_FACTOR, m_y);
	m_matrix.print(m_text.c_str());

	if(m_x/SPEED_FACTOR + m_width < m_matrix.width())
	{
		m_matrix.setCursor(m_x/SPEED_FACTOR + m_width, m_y);
		m_matrix.print(m_text.c_str());
	}
	m_x += m_dx;
	if(m_x/SPEED_FACTOR + m_width <= 0){
		m_x += m_width*SPEED_FACTOR;
	}
}