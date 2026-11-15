#pragma once

#include "Effect.h"

class FireWorks : public Effect
{
public:
	FireWorks(Adafruit_GFX &matrix, int16_t max_delay_next_launch=-1000);
	void reset();
	void render() override;

private:
	int16_t m_launch_x;
	int16_t	m_max_alt;
	int16_t	m_delta_x;
	int16_t	m_rocket_pos;
	int16_t m_rocket_speed;
	int16_t m_explosion_size;
	int16_t m_max_delay_next_launch;
	int16_t c_r,c_g,c_b;
};
