#pragma once

#include "Effect.h"

class Flake
{
public:
	Flake();
	Flake(const Flake &f);
	Flake(uint8_t max_x, uint8_t max_y);
	void reset(uint8_t max_x, uint8_t max_y=0);
	uint8_t x;
	uint8_t y;
};

class SnowScape : public Effect
{
public:
	SnowScape(Adafruit_GFX &matrix, uint8_t flakes=20, int16_t snow_max_height=8);
	void render();
private:
	std::vector<int16_t> m_ground;
	std::vector<Flake> m_flakes;
	const uint16_t myWHITE;
	int16_t m_snow_max_height;
};