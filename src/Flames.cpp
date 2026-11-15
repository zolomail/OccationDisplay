#include "Flames.h"

namespace {

constexpr uint8_t PALETTE_SIZE = 37;
constexpr uint8_t MAX_HEAT     = PALETTE_SIZE - 1;

// Authentic 37-color PSX/DOOM fire palette in RGB565
constexpr uint16_t DOOM_PALETTE[PALETTE_SIZE] = {
	color565(  0,   0,   0), //  0: Black / Transparent
	color565( 31,   7,   7), //  1
	color565( 47,  15,   7), //  2
	color565( 71,  15,   7), //  3
	color565( 87,  23,   7), //  4
	color565(103,  31,   7), //  5
	color565(119,  31,   7), //  6
	color565(143,  39,   7), //  7
	color565(159,  47,   7), //  8
	color565(175,  63,   7), //  9
	color565(191,  71,   7), // 10
	color565(199,  71,   7), // 11
	color565(223,  79,   7), // 12
	color565(223,  87,   7), // 13
	color565(223,  87,   7), // 14
	color565(215,  95,   7), // 15
	color565(215,  95,   7), // 16
	color565(215, 103,  15), // 17
	color565(207, 111,  15), // 18
	color565(207, 119,  15), // 19
	color565(207, 127,  15), // 20
	color565(207, 135,  23), // 21
	color565(199, 135,  23), // 22
	color565(199, 143,  23), // 23
	color565(199, 151,  31), // 24
	color565(191, 159,  31), // 25
	color565(191, 159,  31), // 26
	color565(191, 167,  39), // 27
	color565(191, 167,  39), // 28
	color565(191, 175,  47), // 29
	color565(183, 175,  47), // 30
	color565(183, 183,  47), // 31
	color565(183, 183,  55), // 32
	color565(207, 207, 111), // 33
	color565(223, 223, 159), // 34
	color565(239, 239, 199), // 35
	color565(255, 255, 255)  // 36: White-hot source
};

// Ultra-fast inline Xorshift32 PRNG (avoids ESP32 hardware TRNG entropy stalls)
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

Flames::Flames(Adafruit_GFX &matrix, uint8_t max_decay) :
	Effect(matrix),
	m_width(m_matrix.width()),
	m_height(m_matrix.height()),
	m_max_decay(max_decay < 2 ? 2 : max_decay),
	m_rng_state(micros() | 1u),
	m_heat(m_width * m_height, 0)
{
	// Seed bottom row with maximum heat (fire source)
	const size_t bottom_row = static_cast<size_t>(m_height - 1) * m_width;
	for (uint16_t x = 0; x < m_width; ++x) {
		m_heat[bottom_row + x] = MAX_HEAT;
	}
}

void Flames::render()
{
	uint32_t rng = m_rng_state;

	// Propagate heat upward with fast PRNG cooling and horizontal flicker
	for (uint16_t y = 1; y < m_height; ++y) {
		const size_t src_row = static_cast<size_t>(y) * m_width;
		const size_t dst_row = static_cast<size_t>(y - 1) * m_width;

		for (uint16_t x = 0; x < m_width; ++x) {
			const uint8_t src_heat = m_heat[src_row + x];
			if (src_heat == 0) {
				m_heat[dst_row + x] = 0;
			} else {
				const uint32_t r     = fastRand(rng);
				const uint8_t  decay = static_cast<uint8_t>((r & 0xFFu) * m_max_decay >> 8);
				const int16_t  wind  = static_cast<int16_t>(((r >> 8) & 0xFFu) * 3 >> 8) - 1; // -1, 0, or +1

				int16_t dst_x = static_cast<int16_t>(x) + wind;
				if (dst_x < 0) {
					dst_x = m_width - 1;
				} else if (dst_x >= m_width) {
					dst_x = 0;
				}

				const uint8_t new_heat = (src_heat > decay) ? (src_heat - decay) : 0;
				m_heat[dst_row + dst_x] = new_heat;
				if (new_heat > 0) {
					m_matrix.drawPixel(dst_x, y - 1, DOOM_PALETTE[new_heat]);
				}
			}
		}
	}

	m_rng_state = rng;

	// Draw bottom source row in a single fast horizontal line
	m_matrix.drawFastHLine(0, m_height - 1, m_width, DOOM_PALETTE[MAX_HEAT]);
}
