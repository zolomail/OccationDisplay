#include "ScrollText.h"

#define SPEED_FACTOR 10

ScrollText::ScrollText(Adafruit_GFX &matrix, String text, int16_t y, int16_t dx, uint16_t color, uint8_t font) :
	Effect(matrix),
	m_text(text),
	m_x(0),
	m_y(y),
	m_initial_y(y),
	m_dx(dx),
	m_color(color),
	m_font(font)
{
	setText(m_text);
	if (m_dx > 0) {
		m_x = -static_cast<int16_t>(m_width) * SPEED_FACTOR;
	} else if (m_dx < 0) {
		m_x = m_matrix.width() * SPEED_FACTOR;
	} else {
		m_x = 0;
	}
}

void ScrollText::setText(String text)
{
	m_text = text;
	setFont(m_font);
	m_matrix.setTextWrap(false);
	int16_t x1 = 0, y1 = 0;
	m_matrix.getTextBounds(m_text, 0, 0, &x1, &y1, &m_width, &m_height);
	m_y = resolveY(m_initial_y, m_height) - y1;
}

void ScrollText::render()
{
	if(m_width == 0)
	{
		return;
	}

	setFont(m_font);
	m_matrix.setTextWrap(false);
	m_matrix.setTextColor(m_color);

	int16_t draw_x = m_x / SPEED_FACTOR;
	if (m_dx > 0) {
		while (draw_x + static_cast<int16_t>(m_width) > 0) {
			if (draw_x < m_matrix.width()) {
				m_matrix.setCursor(draw_x, m_y);
				m_matrix.print(m_text.c_str());
			}
			draw_x -= m_width;
		}
		m_x += m_dx;
		if (m_x / SPEED_FACTOR >= m_matrix.width()) {
			m_x -= m_width * SPEED_FACTOR;
		}
	} else {
		while (draw_x < m_matrix.width()) {
			m_matrix.setCursor(draw_x, m_y);
			m_matrix.print(m_text.c_str());
			draw_x += m_width;
		}
		m_x += m_dx;
		if (m_x / SPEED_FACTOR + static_cast<int16_t>(m_width) <= 0) {
			m_x += m_width * SPEED_FACTOR;
		}
	}
}