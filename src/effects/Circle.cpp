#include "Circle.h"

Circle::Circle(Adafruit_GFX &matrix, int16_t x, int16_t y, int16_t r, uint16_t color) :
	Effect(matrix),
	m_x(x),
	m_y(y),
	m_r(r),
	m_dx(2),
	m_dy(1),
	m_color(color)
{
}

void Circle::render()
{
	m_matrix.drawCircle(m_x, m_y, m_r,m_color);

	if( m_x + m_dx + m_r > m_matrix.width() || m_x + m_dx - m_r < 0)
	{
		m_dx = -m_dx;
	}
	m_x += m_dx;
	if( m_y + m_dy + m_r > m_matrix.height() || m_y + m_dy - m_r < 0)
	{
		m_dy = -m_dy;
	}
	m_y += m_dy;
}