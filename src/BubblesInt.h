#pragma once

#include "Effect.h"

class BubbleInt
{
public:
	BubbleInt();
	BubbleInt(const BubbleInt &f);
	BubbleInt(int16_t _x, int16_t _y, int16_t _r = 0);
	void reset(int16_t max_x, int16_t max_y);
	void move();
	void render(Adafruit_GFX &matrix);
	inline void drawCircleAntialized(Adafruit_GFX &matrix);
	inline void drawFragment(Adafruit_GFX &matrix, int32_t x1, int32_t x2, int16_t pixel_y, const int32_t yy);
	void fillCircleAntialized(Adafruit_GFX &matrix, int16_t cx, int16_t cy, int16_t cr);
	int16_t m_dx;
	int16_t m_x;
	int16_t m_y;
	int16_t m_r;
};

class BubblesInt : public Effect
{
public:
	BubblesInt(Adafruit_GFX &matrix, int16_t count=20);
	void render();
	void drawFragment(float x1, float x2, uint8_t y, float rr, float re, float bx, float by);
	void drawFragmentB(float x1, float x2, uint8_t y, float rr, float re, float bx, float by, float br);
	void drawCircleAntialized(float cx, float cy, float cr);
	void fillCircleAntialized(float cx, float cy, float cr);

private:
	std::vector<BubbleInt> m_bubbles;
};
