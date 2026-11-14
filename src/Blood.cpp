#include "Blood.h"

Drop::Drop()
{
}

// We abuse the copy constructor to initialize each flake at a random position
Drop::Drop(const Drop &f) :
	y(random(0, 7)), falling(false)
{
}

Drop::Drop(uint8_t _y) :
	y(_y), falling(false)
{
}

Blood::Blood(Adafruit_GFX &matrix) :
	Effect(matrix),
	m_drips(m_matrix.width(), {0})
{
}

void Blood::render()
{
	// Move drips
	int x = 0;
	for(auto &drip: m_drips)
	{
		if(drip.falling){
			drip.y += 2;
			if(drip.y > m_matrix.height()){
				drip.falling = false;
				drip.y = 0;
			}else{
				m_matrix.drawFastVLine(x, drip.y, 2, color565(255, 0, 0));
			}
		}else{
			if(random(0, 20) == 0)
			{
				drip.y++;
				if(drip.y > 8)
					drip.falling = true;
			}
			m_matrix.drawFastVLine(x, 0, drip.y, color565(255, 0, 0));
		}
		x++;
	}
}
