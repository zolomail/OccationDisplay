#pragma once

#include "Effect.h"

class Bubble
{
public:
	Bubble();
	Bubble(const Bubble &f);
	Bubble(float _x, float _y, float _r = 0);
	void reset(float max_x, float max_y);
	void move();
	void render(Adafruit_GFX &matrix);
	inline void drawCircleAntialized(Adafruit_GFX &matrix);
	inline void drawFragment(Adafruit_GFX &matrix, float x1, float x2, uint8_t pixel_y, const float rr, const float re);
	void fillCircleAntialized(Adafruit_GFX &matrix, float cx, float cy, float cr);
	float m_x;
	float m_y;
	uint8_t m_r;
};

class Bubbles : public Effect
{
public:
	Bubbles(Adafruit_GFX &matrix, int16_t count=20);
	void render();
	void drawFragment(float x1, float x2, uint8_t y, float rr, float re, float bx, float by);
	void drawFragmentB(float x1, float x2, uint8_t y, float rr, float re, float bx, float by, float br);
	void drawCircleAntialized(float cx, float cy, float cr);
	void fillCircleAntialized(float cx, float cy, float cr);

private:
	std::vector<Bubble> m_bubbles;
};
