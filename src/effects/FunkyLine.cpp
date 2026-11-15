#include "FunkyLine.h"

FunkyLine::FunkyLine(Adafruit_GFX &matrix, int16_t angle, int16_t da, uint16_t color) :
	Effect(matrix),
	m_angle(angle),
	m_da(da),
	m_color(color)
{
}

void FunkyLine::render()
{
	int16_t	x = sin(m_angle * M_PI / 180) * m_matrix.width();
	int16_t	y = cos(m_angle * M_PI / 180) * m_matrix.height();
	m_matrix.drawLine((m_matrix.width() / 2) - x, (m_matrix.height() / 2) - y, (m_matrix.width() / 2) + x, (m_matrix.height() / 2) + y, m_color);
	m_angle += m_da;
}