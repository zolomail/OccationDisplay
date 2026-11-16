#include "BubblesInt.h"
#include <cmath>

namespace {
inline int32_t fastSqrt(int32_t val)
{
	return static_cast<int32_t>(sqrtf(static_cast<float>(val)));
}
} // namespace

BubbleInt::BubbleInt()
{
}

// We abuse the copy constructor to initialize each bubble at a random position
BubbleInt::BubbleInt(const BubbleInt &f) :
	m_dx(random(-64, 64)),
	m_x(random(0, f.m_x)<<8),
	m_y(random(0, f.m_y)<<8),
	m_r(random(3<<8, 7<<8))
{
}

BubbleInt::BubbleInt(int32_t x, int32_t y, int32_t r) :
	m_dx(0),
	m_x(x),
	m_y(y),
	m_r(r)
{
}

void BubbleInt::reset(int32_t max_x, int32_t max_y)
{
	m_r = random(3<<8, 7<<8);
	m_dx = random(-64, 64);
	m_x = random(0, max_x<<8);
	m_y = (max_y<<8) + m_r;
}

void BubbleInt::move()
{
	m_dx = std::min<int32_t>(std::max<int32_t>(m_dx + (int32_t)random(-4, 5),-64),64);

	m_x += m_dx;
	m_y -= 25 + (m_r>>5);
//	m_r += 8;	// Bubble inflate?
}

void BubbleInt::render(Adafruit_GFX &matrix)
{
	drawCircleAntialized(matrix);
	const int32_t offset = (m_r * 2) / 5;
	fillCircleAntialized(matrix, m_x - offset, m_y - offset, m_r / 5);
}

void BubbleInt::drawCircleAntialized(Adafruit_GFX &matrix)
{
//	matrix.drawPixel(m_x>>8, m_y>>8, color565(255, 0, 0));
	int32_t rr = (m_r+128)*(m_r+128);
	int32_t re = (m_r-128)*(m_r-128);
	int32_t pixel_y = std::max<int32_t>((m_y - m_r)>>8, 0)<<8;
	int32_t yy = pixel_y+128-m_y;

	int32_t	yyy = (yy+0x100)*(yy+0x100);
	yy *= yy;
	yyy -= yy;
	int32_t	yyyy = 2*0x100*0x100;

	for(; pixel_y <= std::min<int32_t>((m_y + m_r)>>8,matrix.height()-1)<<8 ; pixel_y+=0x100)
	{
		int32_t delta_x = fastSqrt(rr - yy);
		int32_t xe = re - yy;
		int32_t x_l = m_x - delta_x;
		int32_t x_r = m_x + delta_x;

		if(xe > 0){
			xe = fastSqrt(xe);
			drawFragment(matrix, x_l, m_x - xe, pixel_y>>8, yy);
			drawFragment(matrix, m_x + xe, x_r, pixel_y>>8, yy);
		}else{
			drawFragment(matrix, x_l, x_r, pixel_y>>8, yy);
		}
		yy += yyy;
		yyy += yyyy;
	}
}

void BubbleInt::drawFragment(Adafruit_GFX &matrix, int32_t x1, int32_t x2, int16_t pixel_y, const int32_t yy)
{
	if(x2<0 || x1>>8>=matrix.width())
		return;

	int32_t	pixel_x = std::max<int32_t>(x1>>8, 0)<<8;
	int32_t xx = pixel_x+128-m_x;
	int32_t	xxx = (xx+0x100)*(xx+0x100);
	xx *= xx;
	xxx -= xx;
	int32_t	xxxx = 2*0x100*0x100;
	for(; pixel_x <= std::min<int32_t>(x2>>8,matrix.width()-1)<<8; pixel_x+= 0x100)
	{
		int32_t dist = ((fastSqrt(xx + yy) - m_r) * 361) >> 8; // 1.41 makes lines less fat in the x direction
		if(abs(dist) > 256)
	 	{
			// matrix.drawPixel(pixel_x>>8, pixel_y, color565(255, 0, 0));
		}else if(dist < 0){
			matrix.drawPixel(pixel_x>>8, pixel_y, color565((dist>>2)&0x3F, (dist>>1)&0x7F, dist&0xFF));
		}else{
			matrix.drawPixel(pixel_x>>8, pixel_y, color565(((256 - dist)>>2)&0x3F, ((256 - dist)>>1)&0x7F, (256 - dist)&0xFF));
		}
		xx += xxx;
		xxx += xxxx;
	}
}

void BubbleInt::fillCircleAntialized(Adafruit_GFX &matrix, int32_t cx, int32_t cy, int32_t cr)
{
	int32_t rr = (cr+128)*(cr+128);
	int32_t pixel_y = std::max<int32_t>((cy - cr)>>8, 0)<<8;
	int32_t yy = pixel_y+128-cy;
	int32_t yyy = (yy+0x100)*(yy+0x100);
	yy *= yy;
	yyy -= yy;
	int32_t yyyy = 2*0x100*0x100;

	for(; pixel_y <= std::min<int32_t>((cy + cr)>>8,matrix.height()-1)<<8 ; pixel_y+=0x100)
	{
		int32_t delta_x = fastSqrt(rr - yy);

		int32_t x_l = cx - delta_x;
		int32_t x_r = cx + delta_x;
		if(x_r >= 0 && (x_l >> 8) < matrix.width())
		{
			int32_t pixel_x = std::max<int32_t>(x_l>>8, 0)<<8;
			int32_t xx = pixel_x+128-cx;
			int32_t xxx = (xx+0x100)*(xx+0x100);
			xx *= xx;
			xxx -= xx;
			int32_t xxxx = 2*0x100*0x100;

			for(; pixel_x <= std::min<int32_t>(x_r>>8, matrix.width()-1)<<8; pixel_x+= 0x100)
			{
				int32_t dist = ((fastSqrt(xx + yy) - cr) * 361) >> 8;
				if(dist < 0){
					matrix.drawPixel(pixel_x>>8, pixel_y>>8, color565(255, 255, 255));
				}else if(dist <= 256){
					matrix.drawPixel(pixel_x>>8, pixel_y>>8, color565((256 - dist)&0xFF, (256 - dist)&0xFF, (256 - dist)&0xFF));
				}
				xx += xxx;
				xxx += xxxx;
			}
		}
		yy += yyy;
		yyy += yyyy;
	}
}

BubblesInt::BubblesInt(Adafruit_GFX &matrix, int16_t count) :
	Effect(matrix),
	m_bubbles(count, {m_matrix.width(), m_matrix.height()})	// Copy constructor abuse
{
}

void BubblesInt::render()
{
//	int time = millis();
	for(auto &bubble: m_bubbles)
	{
		bubble.move();
		if(bubble.m_y + bubble.m_r<0)
		{
			bubble.reset(m_matrix.width(), m_matrix.height());
		}
		bubble.render(m_matrix);
	}
//	time = millis() - time;
//	m_matrix.setCursor(0, 0);
//	m_matrix.print(time);
}
