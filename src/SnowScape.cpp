#include "SnowScape.h"

Flake::Flake()
{
}

// We abuse the copy constructor to initialize each flake at a random position
Flake::Flake(const Flake &f) :
	x(random(0, f.x)),
	y(random(0, f.y))
{
}

Flake::Flake(uint8_t _x, uint8_t _y) :
	x(_x),
	y(_y)
{
}

void Flake::reset(uint8_t max_x, uint8_t max_y)
{
	x = random(0, max_x);
	y = random(0, max_y);
}

SnowScape::SnowScape(Adafruit_GFX &matrix, uint8_t flakes, int16_t snow_max_height) :
	Effect(matrix),
	m_ground(m_matrix.width(), (m_matrix.height()-1)<<1),
	m_flakes(flakes, {(uint8_t)m_matrix.width()-1, (uint8_t)m_matrix.height()-1}),	// Copy constructor abuse
	myWHITE(color565(255, 255, 255)),
	m_snow_max_height(snow_max_height)
{
}

void SnowScape::render()
{
	// Move flakes
	for(auto &flake: m_flakes)
	{
		int16_t x = (flake.x + random(0, 3)-1) % m_matrix.width();
		if(flake.y < (m_ground[x]>>1))
		{
			flake.x = x;
		}
		flake.y += 1;
		if(flake.y >= (m_ground[flake.x]>>1) ){
			if(flake.x < m_matrix.width()-1 &&  flake.y < (m_ground[flake.x+1]>>1) ){	// Slide right?
				flake.x += 1;
			}else if(flake.x > 1 && flake.y < (m_ground[flake.x-1]>>1)){	// Slide left?
				flake.x -= 1;
			}else{	// land
				m_ground[flake.x]--;
				flake.reset(m_matrix.width()-1);
			}
		}
	}

	// Render snow scape
	int x = 0;
	int16_t min_y = m_matrix.height();
	for(auto &gnd: m_ground)
	{
		if(gnd & 0x1){
			m_matrix.drawFastVLine(x, (gnd>>1)+1, m_matrix.height() - (gnd>>1), myWHITE);
			m_matrix.drawPixel(x++, (gnd>>1), color565(0, 64, 128));
		}else{
			m_matrix.drawFastVLine(x++, (gnd>>1), m_matrix.height() - (gnd>>1), myWHITE);
		}
		min_y = min((int16_t)(gnd>>1), min_y);
	}

	if(min_y < m_matrix.height()-m_snow_max_height)
	{
		for(auto &gnd: m_ground)
		{
			gnd = min(gnd + 1, (m_matrix.height() - 1)<<1);
		}
	}

	// Render snow flakes
	for(const auto &flake: m_flakes)
	{
		m_matrix.drawPixel(flake.x, flake.y, myWHITE);
	}
}
