#pragma once

#include <vector>
#include "Effect.h"

class BumpMap : public Effect
{
public:
	// base_color: RGB565 color used to build the diffuse-to-specular lighting gradient
	// text:       Optional short text to emboss into the surface ("" = no text)
	// text_only:  If true, embosses the text on a flat surface without the background hills/rings
	// font:       FontId to use when rendering embossed text (default NORMAL_FONT)
	// text_size:  Scale multiplier for embossed text (default 2 for bold 3D bevels)
	BumpMap(
		Adafruit_GFX &matrix,
		uint16_t base_color = color565(255, 110, 0),
		const String &text = "",
		bool text_only = false,
		uint8_t font = NORMAL_FONT,
		uint8_t text_size = 2
	);
	void render() override;

private:
	uint16_t            m_width;
	uint16_t            m_height;
	float               m_light_phase = 0.0f;
	std::vector<int8_t> m_nx;
	std::vector<int8_t> m_ny;
	uint16_t            m_palette[256];
};
