#include "Bubbles.h"

Bubble::Bubble()
{
}

static int init_x = 0;

// We abuse the copy constructor to initialize each flake at a random position
Bubble::Bubble(const Bubble &f) :
//	m_x(random(0, f.m_x * 10) / 10.0),
//	m_y(random(0, f.m_y * 10) / 10.0),
//	m_r(random(2, 7))
	m_x(int((init_x*128)/5)),
	m_y(15.5),
	m_r(init_x%2?7:15)
{
	init_x++;
}

Bubble::Bubble(float x, float y, float r) :
	m_x(x),
	m_y(y),
	m_r(r)
{
}

void Bubble::reset(float max_x, float max_y)
{
	m_x = random(0, max_x * 10) / 10.0;
	m_r = random(2, 7);
	m_y = max_y + m_r;
}

void Bubble::move()
{
//	m_x += (random(0, 1000) - 500) / 1000.0;
//	m_y -= (2.0 + m_r) / 20.0;
}

void Bubble::render(Adafruit_GFX &matrix)
{
//	matrix.drawCircle(bubble.x, bubble.y, bubble.r, color565(0, 0, 32));
//	matrix.fillCircle(bubble.x-(bubble.r/2.5), bubble.y-(bubble.r/2.5), bubble.r/5, color565(255, 255, 255));
	drawCircleAntialized(matrix);
	fillCircleAntialized(matrix, m_x-(m_r/2.5), m_y-(m_r/2.5), m_r/5);
}

void Bubble::drawCircleAntialized(Adafruit_GFX &matrix)
{
	float rr = (m_r+.5)*(m_r+.5);
	float re = (m_r-.5)*(m_r-.5);
	for(float pixel_y = m_y - m_r; pixel_y <= m_y + m_r; pixel_y++)
	{
		int yy = int(pixel_y)+0.5-m_y;
		float delta_x = sqrtf(rr-yy*yy); //-0.5;	// - 0.5 Reduce the error pixels caused beacuse of 1.41 in draeFragment
		float xe = re - yy * yy;
		float x_l = m_x - delta_x;
		float x_r = m_x + delta_x;
		if(xe > 0.0){
			xe = sqrt(xe);
			drawFragment(matrix, x_l, m_x - xe, pixel_y, rr, re);
			drawFragment(matrix, m_x + xe, x_r, pixel_y, rr, re);
		}else{
			drawFragment(matrix, x_l, x_r, pixel_y, rr, re);
		}
	}
}

void Bubble::drawFragment(Adafruit_GFX &matrix, float x1, float x2, uint8_t pixel_y, const float rr, const float re)
{
	float yy = (pixel_y+0.5-m_y)*(pixel_y+0.5-m_y);
	for(uint8_t pixel_x = int(x1); pixel_x <= int(x2); pixel_x++)
	{
		float dist = (sqrtf((pixel_x-m_x+0.5)*(pixel_x-m_x+0.5)+yy)-m_r); //*1.41;	// 1.41 make lines less fat in the x direction
		if(fabs(dist) > 1.0)
		{
			//matrix.drawPixel(pixel_x, pixel_y, color565(255, 0, 0));
		}else if(dist < 0.0){
			matrix.drawPixel(pixel_x, pixel_y, color565(0, 0, (1.0 + dist) * 255));
		}else{
			matrix.drawPixel(pixel_x, pixel_y, color565(0, 0, (1.0 - dist) * 255));
		}
	}
}

void Bubble::fillCircleAntialized(Adafruit_GFX &matrix, float cx, float cy, float cr)
{
	float rr = (cr+0.5)*(cr+0.5);
	for(float pixel_y=cy - cr; pixel_y <= cy + cr; pixel_y++)
	{
		int yy = int(pixel_y) - cy;
		yy *= yy;
		float delta_x = sqrtf(rr-yy);
		for(uint8_t pixel_x = int(cx - delta_x); pixel_x <= int(cx + delta_x); pixel_x++)
		{
			float dist = sqrtf((pixel_x + 0.5 - cx) * (pixel_x + 0.5 - cx) + yy) - cr;
			if(dist < 0.0){
				matrix.drawPixel(pixel_x, pixel_y, color565(255, 255, 255));
			}else if(dist <= 1.0){
				matrix.drawPixel(pixel_x, pixel_y, color565((1.0-dist)*255, (1.0-dist)*255, (1.0-dist)*255));
			}
		}
	}
}



Bubbles::Bubbles(Adafruit_GFX &matrix, int16_t count) :
	Effect(matrix),
	m_bubbles(count, {(uint8_t)m_matrix.width(), (uint8_t)m_matrix.height()})	// Copy constructor abuse
{
}

void Bubbles::render()
{
	int time = millis();
	for(auto &bubble: m_bubbles)
	{
		bubble.move();
		if(bubble.m_y + bubble.m_r<0)
		{
			bubble.reset(m_matrix.width(), m_matrix.height());
		}
		bubble.render(m_matrix);
	}
	time = millis() - time;
	m_matrix.setCursor(0, 0);
	m_matrix.print(time);
}
