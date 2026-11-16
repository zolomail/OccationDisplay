#pragma once

#include "Effect.h"

class BubbleInt
{
public:
	BubbleInt();
	BubbleInt(const BubbleInt &f);
	BubbleInt(int32_t _x, int32_t _y, int32_t _r = 0);
	void reset(int32_t max_x, int32_t max_y);
	void move();
	void render(Adafruit_GFX &matrix);
	inline void drawCircleAntialized(Adafruit_GFX &matrix);
	inline void drawFragment(Adafruit_GFX &matrix, int32_t x1, int32_t x2, int16_t pixel_y, const int32_t yy);
	void fillCircleAntialized(Adafruit_GFX &matrix, int32_t cx, int32_t cy, int32_t cr);
	int32_t m_dx;
	int32_t m_x;
	int32_t m_y;
	int32_t m_r;
};

class BubblesInt : public Effect
{
public:
	BubblesInt(Adafruit_GFX &matrix, int16_t count=20);
	void render() override;

private:
	std::vector<BubbleInt> m_bubbles;
};
