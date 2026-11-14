#pragma once

#include "Effect.h"

class ShootingStar : public Effect
{
public:
	ShootingStar(Adafruit_GFX &matrix, int16_t max_delay_next_launch=-1000);
	void reset();
	void render();

private:
	int16_t m_max_delay_next_launch;
	int16_t m_launch_x;
	double	m_angle;
	int16_t	m_pos;
	int16_t m_speed;
	uint8_t m_r, m_g, m_b;
};