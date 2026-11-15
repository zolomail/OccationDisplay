#include "Stars.h"

Star::Star()
{
}

// We abuse the copy constructor to initialize each stars at a random position
Star::Star(const Star &f)
{
	reset(f.x, f.y);
}

Star::Star(int16_t _x, int16_t _y) :
	x(_x),
	y(_y)
{
}

void Star::reset(int16_t max_x, int16_t max_y)
{
	x = random(0, max_x);
	y = random(0, max_y);
	s = random(1, 10);
	f = random(0, 511);
}

Stars::Stars(Adafruit_GFX &matrix, int16_t stars) :
	Effect(matrix),
	m_stars(stars, {static_cast<int16_t>(m_matrix.width()-1), static_cast<int16_t>(m_matrix.height()-1)})	// Copy constructor abuse
{
}

void Stars::render()
{
	for(auto &star: m_stars)
	{
		star.f += star.s;
		if(star.f>512){
			star.reset(m_matrix.width()-1, m_matrix.height()-1);
		}
		int16_t lum = (256-abs(256-star.f))>>1;
		m_matrix.drawPixel(star.x, star.y, color565((lum>>1)&0xF8, (lum>>1)&0xF8, lum&0xF8));
	}
}
