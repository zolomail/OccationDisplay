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
	m_snow_max_height(snow_max_height)
{
}

void SnowScape::render()
{
	const int16_t w = m_matrix.width();
	const int16_t h = m_matrix.height();

	// Move flakes
	for (auto &flake : m_flakes)
	{
		// 1. Horizontal wind drift: clamp within [0, w - 1] to prevent negative index or right-to-left wrap
		const int16_t drift = static_cast<int16_t>(random(0, 3)) - 1; // -1, 0, +1
		const int16_t next_x = constrain(flake.x + drift, 0, w - 1);

		// Only drift horizontally if the column is not already blocked by a snow pile
		if (flake.y < (m_ground[next_x] >> 1)) {
			flake.x = next_x;
		}

		flake.y += 1;

		// 2. Check if flake has landed on or settled into the snow surface
		if (flake.y >= (m_ground[flake.x] >> 1)) {
			const bool can_right = (flake.x < w - 1) && (flake.y < (m_ground[flake.x + 1] >> 1));
			const bool can_left  = (flake.x > 0)     && (flake.y < (m_ground[flake.x - 1] >> 1));

			if (can_right && can_left) {
				// Slide down the steeper slope (larger Y value = deeper valley = less snow)
				if (m_ground[flake.x + 1] > m_ground[flake.x - 1]) {
					flake.x += 1;
				} else if (m_ground[flake.x - 1] > m_ground[flake.x + 1]) {
					flake.x -= 1;
				} else {
					// Equal depth: 50/50 symmetric chance left or right
					flake.x += (random(0, 2) == 0) ? -1 : 1;
				}
			} else if (can_right) {
				flake.x += 1;
			} else if (can_left) {
				flake.x -= 1;
			} else {
				// Land and accumulate snow in this column
				if (m_ground[flake.x] > 0) {
					m_ground[flake.x]--;
				}
				flake.reset(w);
			}
		}
	}

	// Render snow scape ground
	int16_t min_y = h;
	for (int16_t x = 0; x < w; ++x)
	{
		const int16_t gnd = m_ground[x];
		const int16_t top_y = gnd >> 1;
		if (gnd & 0x1) {
			m_matrix.drawFastVLine(x, top_y + 1, h - top_y, myWHITE);
			m_matrix.drawPixel(x, top_y, color565(0, 64, 128));
		} else {
			m_matrix.drawFastVLine(x, top_y, h - top_y, myWHITE);
		}
		if (top_y < min_y) {
			min_y = top_y;
		}
	}

	// Gently melt / compact snow across the screen when highest peak exceeds threshold
	if (min_y < h - m_snow_max_height)
	{
		const int16_t ground_floor = (h - 1) << 1;
		for (auto &gnd : m_ground)
		{
			if (gnd < ground_floor) {
				gnd++;
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
