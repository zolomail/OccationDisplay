#pragma once

#include "Effect.h"

enum class BorderPattern : uint8_t {
	Solid = 0, // Solid color1 (or alternating concentric rings of color1/color2 when thickness > 1)
	Dashed,    // Alternating stripes of color1 and color2 (marquee / candy-cane border)
	Chase,     // Smooth gradient wave / comet chasing between color1 and color2 around the perimeter
	Twinkle,   // Sparkling lights randomly shimmering between color1 and color2
	Rainbow    // Smooth HSV rainbow cycling around the perimeter
};

class Border : public Effect
{
public:
	// thickness:  Border width in pixels from the outer edge inward (default 1)
	// color1:     Primary RGB565 color
	// color2:     Secondary RGB565 color (used by Dashed, Chase, Twinkle, or multi-ring Solid)
	// pattern:    BorderPattern style (Solid, Dashed, Chase, Twinkle, Rainbow)
	// stripe_len: Length in pixels of each stripe/wave cycle along the perimeter (default 4)
	// speed:      Animation speed in 1/4-pixels per frame (0 = static, >0 = clockwise, <0 = counter-clockwise)
	Border(
		Adafruit_GFX &matrix,
		uint8_t thickness = 1,
		uint16_t color1 = 0xFFFF,
		uint16_t color2 = 0x0000,
		BorderPattern pattern = BorderPattern::Solid,
		uint8_t stripe_len = 4,
		int8_t speed = 2
	);

	void render() override;

private:
	uint16_t evalColor(int32_t perim_pos, uint8_t ring, int32_t perim_len);

	uint8_t       m_thickness;
	uint16_t      m_color1;
	uint16_t      m_color2;
	BorderPattern m_pattern;
	uint8_t       m_stripe_len;
	int8_t        m_speed;
	int32_t       m_phase     = 0;
	uint32_t      m_rng_state = 0xA53C9E17u;

	uint8_t m_r1, m_g1, m_b1;
	uint8_t m_r2, m_g2, m_b2;
};
