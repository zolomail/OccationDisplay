#include "Present.h"

Present::Present(Adafruit_GFX &matrix, int16_t _y, int16_t _w, int16_t _h, uint16_t _color, uint16_t _ribbon) :
	Effect(matrix),
	w(_w),
	h(_h),
	x_pos(random(0, m_matrix.width()+w-1)-w+1),
	y_pos(_y),
	color(_color),
	ribbon(_ribbon)
{
}

void Present::render()
{
	y_pos++;
	if(y_pos>m_matrix.height()){
		x_pos=random(0, m_matrix.width()+w-1)-w+1;
		y_pos=-h-random(0, 20);
	}
	m_matrix.fillRect(x_pos, y_pos, w, h, color);
	m_matrix.drawLine(x_pos, y_pos+(h/2), x_pos+w-1, y_pos+(h/2), ribbon);
	m_matrix.drawLine(x_pos+(w/2), y_pos, x_pos+(w/2), y_pos+h-1, ribbon);
}
