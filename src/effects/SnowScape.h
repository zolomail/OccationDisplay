#pragma once

#include "Effect.h"
#include <vector>

class Flake
{
public:
	Flake();
	Flake(const Flake &f);
	Flake(int16_t max_x, int16_t max_y);
	void reset(int16_t max_x, int16_t max_y = 0);
	int16_t x{0};
	int16_t y{0};
};

class SnowScape : public Effect
{
public:
	SnowScape(Adafruit_GFX &matrix, uint8_t flakes = 20, int16_t snow_max_height = 8);
	void render() override;

private:
	std::vector<int16_t> m_ground;
	std::vector<Flake>   m_flakes;
	const uint16_t       myWHITE;
	int16_t              m_snow_max_height;
};
