#include "SnowScape.h"

Flake::Flake()
{
}

Flake::Flake(const Flake &f) :
	x(f.x > 0 ? static_cast<int16_t>(random(0, f.x)) : 0),
	y(f.y > 0 ? static_cast<int16_t>(random(0, f.y)) : 0)
{
}

Flake::Flake(int16_t _x, int16_t _y) :
	x(_x),
	y(_y)
{
}

void Flake::reset(int16_t max_x, int16_t max_y)
{
	x = (max_x > 0) ? static_cast<int16_t>(random(0, max_x)) : 0;
	y = (max_y > 0) ? static_cast<int16_t>(random(0, max_y)) : 0;
}

SnowScape::SnowScape(Adafruit_GFX &matrix, uint8_t flakes, int16_t snow_max_height) :
	Effect(matrix),
	m_ground(m_matrix.width(), (m_matrix.height() - 1) << 1),
	m_flakes(flakes, Flake(static_cast<int16_t>(m_matrix.width()), static_cast<int16_t>(m_matrix.height()))),
	myWHITE(color565(255, 255, 255)),
	myBLUE(color565(0, 64, 128)),
	m_snow_max_height(snow_max_height),
	m_rng(static_cast<uint32_t>(random(1, 0x7FFFFFFF)) | 1u)
{
}

inline uint32_t SnowScape::nextRand()
{
	uint32_t x = m_rng;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	m_rng = x;
	return x;
}

void SnowScape::render()
{
	const int16_t w = m_matrix.width();
	const int16_t h = m_matrix.height();
	int16_t* const ground = m_ground.data();

	// Move flakes
	for (auto &flake : m_flakes)
	{
		// 1. Horizontal wind drift: clamp within [0, w - 1] to prevent negative index or right-to-left wrap
		const int16_t drift = static_cast<int16_t>((static_cast<uint64_t>(nextRand()) * 3) >> 32) - 1; // -1, 0, +1
		int16_t next_x = flake.x + drift;
		if (next_x < 0) next_x = 0;
		else if (next_x >= w) next_x = w - 1;

		// Only drift horizontally if the column is not already blocked by a snow pile
		if (flake.y < (ground[next_x] >> 1)) {
			flake.x = next_x;
		}

		flake.y += 1;

		// 2. Check if flake has landed on or settled into the snow surface
		const int16_t fx = flake.x;
		if (flake.y >= (ground[fx] >> 1)) {
			const bool can_right = (fx < w - 1) && (flake.y < (ground[fx + 1] >> 1));
			const bool can_left  = (fx > 0)     && (flake.y < (ground[fx - 1] >> 1));

			if (can_right && can_left) {
				// Slide down the steeper slope (larger Y value = deeper valley = less snow)
				const int16_t g_right = ground[fx + 1];
				const int16_t g_left  = ground[fx - 1];
				if (g_right > g_left) {
					flake.x = fx + 1;
				} else if (g_left > g_right) {
					flake.x = fx - 1;
				} else {
					// Equal depth: 50/50 symmetric chance left or right
					flake.x = fx + ((nextRand() & 1u) ? 1 : -1);
				}
			} else if (can_right) {
				flake.x = fx + 1;
			} else if (can_left) {
				flake.x = fx - 1;
			} else {
				// Land and accumulate snow in this column
				if (ground[fx] > 0) {
					ground[fx]--;
				}
				flake.x = static_cast<int16_t>((static_cast<uint64_t>(nextRand()) * w) >> 32);
				flake.y = 0;
			}
		}
	}

	// Render snow scape ground using horizontal spans & solid base fillRect
	// (much faster on HUB75 DMA than per-column drawFastVLine)
	int16_t min_y = h;
	int16_t min_white_y = h;
	int16_t max_white_y = 0;

	for (int16_t x = 0; x < w; ++x)
	{
		const int16_t gnd = ground[x];
		const int16_t top_y = gnd >> 1;
		const int16_t white_y = (gnd + 1) >> 1;
		if (gnd & 0x1) {
			m_matrix.drawPixel(x, top_y, myBLUE);
		}
		if (top_y < min_y) {
			min_y = top_y;
		}
		if (white_y < min_white_y) {
			min_white_y = white_y;
		}
		if (white_y > max_white_y) {
			max_white_y = white_y;
		}
	}

	// Draw partial snow rows between min_white_y and max_white_y - 1 as horizontal runs
	for (int16_t y = min_white_y; y < max_white_y; ++y)
	{
		int16_t x = 0;
		while (x < w)
		{
			while (x < w && ((ground[x] + 1) >> 1) > y) {
				++x;
			}
			if (x >= w) break;
			const int16_t run_start = x;
			while (x < w && ((ground[x] + 1) >> 1) <= y) {
				++x;
			}
			m_matrix.drawFastHLine(run_start, y, x - run_start, myWHITE);
		}
	}

	// Draw solid bottom snow base in a single fillRect
	if (max_white_y < h) {
		m_matrix.fillRect(0, max_white_y, w, h - max_white_y, myWHITE);
	}

	// Gently melt / compact snow across the screen when highest peak exceeds threshold
	if (min_y < h - m_snow_max_height)
	{
		const int16_t ground_floor = (h - 1) << 1;
		for (int16_t x = 0; x < w; ++x)
		{
			if (ground[x] < ground_floor) {
				ground[x]++;
			}
		}
	}

	// Render active falling snowflakes
	for (const auto &flake : m_flakes)
	{
		if (flake.x >= 0 && flake.x < w && flake.y >= 0 && flake.y < h) {
			m_matrix.drawPixel(flake.x, flake.y, myWHITE);
		}
	}
}
