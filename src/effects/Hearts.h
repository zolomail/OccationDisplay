#pragma once

#include <vector>
#include "Effect.h"

class HeartParticle
{
public:
	HeartParticle();
	HeartParticle(const HeartParticle &f);
	HeartParticle(int32_t x, int32_t y, int32_t r = 0);

	void reset(int32_t max_x, int32_t max_y);
	void move();
	void render(Adafruit_GFX &matrix, const uint16_t sdf[64][64]);

private:
	void drawHeartAntialized(Adafruit_GFX &matrix, const uint16_t sdf[64][64]);
	void fillHighlightAntialized(Adafruit_GFX &matrix, int32_t cx, int32_t cy, int32_t cr);

public:
	int32_t m_dx;
	int32_t m_x;
	int32_t m_y;
	int32_t m_r;
	int32_t m_dr;
};

class Hearts : public Effect
{
public:
	Hearts(Adafruit_GFX &matrix, int16_t count = 14);
	void render() override;

private:
	std::vector<HeartParticle> m_hearts;
	// Normalized 64x64 Euclidean distance LUT for a plump, rounded half-heart (in 8.8 fixed-point)
	uint16_t m_sdf[64][64];
};
