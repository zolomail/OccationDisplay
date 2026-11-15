#include "WaveSim.h"
#include <cmath>

namespace {

// Fast inline Xorshift32 PRNG
inline uint32_t fastRand(uint32_t& state)
{
	uint32_t x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	state = x;
	return x;
}

} // namespace

WaveSim::WaveSim(Adafruit_GFX &matrix, uint16_t drop_interval, bool enable_boat) :
	Effect(matrix),
	m_width(m_matrix.width()),
	m_height(m_matrix.height()),
	m_stride(m_width + 2),
	m_drop_interval(drop_interval),
	m_enable_boat(enable_boat),
	m_rng_state(micros() | 1u),
	m_buf_a(static_cast<size_t>(m_width + 2) * (m_height + 2), 0),
	m_buf_b(static_cast<size_t>(m_width + 2) * (m_height + 2), 0)
{
	// Precompute 256-entry water shading palette (index 128 = calm water)
	for (int i = 0; i < 256; ++i) {
		uint8_t r = 0;
		uint8_t g = 0;
		uint8_t b = 0;

		if (i < 128) {
			// Deep water trough (0..127 -> dark abyss blue to calm ocean blue)
			const int t = i; // 0..127
			r = 0;
			g = static_cast<uint8_t>((t * 32) / 128);
			b = static_cast<uint8_t>(20 + (t * 80) / 128);
		} else if (i < 200) {
			// Mid wave crest (128..199 -> calm ocean blue to bright cyan/azure)
			const int t = i - 128; // 0..71
			r = static_cast<uint8_t>((t * 60) / 72);
			g = static_cast<uint8_t>(32 + (t * 170) / 72);
			b = static_cast<uint8_t>(100 + (t * 155) / 72);
		} else {
			// Specular foam highlight (200..255 -> bright cyan to white)
			const int t = i - 200; // 0..55
			r = static_cast<uint8_t>(60 + (t * 195) / 55);
			g = static_cast<uint8_t>(202 + (t * 53) / 55);
			b = 255;
		}
		m_palette[i] = color565(r, g, b);
	}
}

void WaveSim::addDrop(int16_t x, int16_t y, int16_t strength)
{
	if (x < 1 || x >= static_cast<int16_t>(m_width) - 1 ||
	    y < 1 || y >= static_cast<int16_t>(m_height) - 1) {
		return;
	}

	int16_t* curr = m_flip ? m_buf_b.data() : m_buf_a.data();
	const size_t idx = static_cast<size_t>(y + 1) * m_stride + (x + 1);

	// 5-point cross kernel couples both checkerboard parity sub-lattices for smooth circular ripples
	curr[idx]            = strength;
	curr[idx - 1]        = strength >> 1;
	curr[idx + 1]        = strength >> 1;
	curr[idx - m_stride] = strength >> 1;
	curr[idx + m_stride] = strength >> 1;
}

void WaveSim::render()
{
	int16_t* curr = m_flip ? m_buf_b.data() : m_buf_a.data();
	int16_t* prev = m_flip ? m_buf_a.data() : m_buf_b.data();

	// 1. Spawn random raindrops at the configured frame interval
	if (m_drop_interval > 0) {
		if (++m_frame_count >= m_drop_interval) {
			m_frame_count = 0;
			const uint32_t r  = fastRand(m_rng_state);
			const int16_t  rx = 2 + static_cast<int16_t>((r & 0xFFFFu) * (m_width - 4) >> 16);
			const int16_t  ry = 2 + static_cast<int16_t>(((r >> 16) & 0xFFFFu) * (m_height - 4) >> 16);
			addDrop(rx, ry, 520);
		}
	}

	// 2. Update moving boat / water skier along a smooth Lissajous curve
	int16_t boat_x = -1;
	int16_t boat_y = -1;
	if (m_enable_boat) {
		m_boat_phase += 0.045f;
		if (m_boat_phase > 628.3185f) {
			m_boat_phase -= 628.3185f;
		}
		const float half_w = m_width * 0.5f;
		const float half_h = m_height * 0.5f;
		boat_x = static_cast<int16_t>(half_w + cosf(m_boat_phase) * (half_w - 4.0f));
		boat_y = static_cast<int16_t>(half_h + sinf(m_boat_phase * 2.3f) * (half_h - 4.0f));
		addDrop(boat_x, boat_y, -380);
	}

	// 3. Mirror edge cells into the 1-pixel guard border so waves reflect cleanly off display walls
	const uint16_t w      = m_width;
	const uint16_t h      = m_height;
	const uint16_t stride = m_stride;

	for (uint16_t x = 1; x <= w; ++x) {
		curr[x]                    = curr[stride + x];
		curr[(h + 1) * stride + x] = curr[h * stride + x];
	}
	for (uint16_t y = 1; y <= h; ++y) {
		const size_t row = static_cast<size_t>(y) * stride;
		curr[row]         = curr[row + 1];
		curr[row + w + 1] = curr[row + w];
	}

	// 4. Step the 2D wave equation and overdraw the entire 128x32 display in a single pass
	for (uint16_t y = 1; y <= h; ++y) {
		const size_t row = static_cast<size_t>(y) * stride;
		const int16_t py = static_cast<int16_t>(y - 1);

		for (uint16_t x = 1; x <= w; ++x) {
			const size_t idx = row + x;

			const int16_t left  = curr[idx - 1];
			const int16_t right = curr[idx + 1];
			const int16_t up    = curr[idx - stride];
			const int16_t down  = curr[idx + stride];

			int32_t val = ((left + right + up + down) >> 1) - prev[idx];
			val = (val * 31) / 32; // Smooth symmetric damping (~3% energy loss per frame)

			if (val > 2048)       val = 2048;
			else if (val < -2048) val = -2048;

			prev[idx] = static_cast<int16_t>(val);

			// Combine wave elevation and directional surface slope (top-left light source)
			const int32_t slope = (left - right) + (up - down);
			int32_t pal_idx = 128 + (val >> 2) + (slope >> 3);
			if (pal_idx < 0)        pal_idx = 0;
			else if (pal_idx > 255) pal_idx = 255;

			m_matrix.drawPixel(static_cast<int16_t>(x - 1), py, m_palette[pal_idx]);
		}
	}

	// Draw the boat / water skier highlight pixel on top of its wake
	if (m_enable_boat && boat_x >= 0 && boat_y >= 0) {
		m_matrix.drawPixel(boat_x, boat_y, 0xFFFF);
	}

	// Swap buffers for the next frame
	m_flip = !m_flip;
}
