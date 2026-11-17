#include "Bitmap.h"

Bitmap::Bitmap(Adafruit_GFX &matrix, const uint16_t &bitmap, int16_t width, int16_t height, int16_t x, int16_t y, int16_t dx, int16_t dy, const unsigned char *mask) :
	Effect(matrix),
	m_bitmap(bitmap),
	m_width(width),
	m_height(height),
	m_x(x),
	m_y(y),
	m_dx(dx),
	m_dy(dy),
	m_mask(mask)
{
	m_x = resolveX(m_x, m_width) << 4;
	m_y = resolveY(m_y, m_height) << 4;
}

void Bitmap::render()
{
	if(m_mask == NULL)
	{
		m_matrix.drawRGBBitmap(m_x>>4, m_y>>4, &m_bitmap, m_width, m_height);
	}else{
		m_matrix.drawRGBBitmap(m_x>>4, m_y>>4, &m_bitmap, m_mask, m_width, m_height);
	}

	if(((m_x + m_dx)>>4) + m_width > m_matrix.width() || (m_x + m_dx)>>4 < 0)
	{
		m_dx = -m_dx;
	}
	m_x += m_dx;
	if(((m_y + m_dy)>>4) + m_height > m_matrix.height() || (m_y + m_dy)>>4 < 0)
	{
		m_dy = -m_dy;
	}
	m_y += m_dy;
}
