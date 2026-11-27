#pragma once

#include "Effect.h"
#include "Weather.h"

// Renders only the current temperature (e.g. "21°C" or "--°C") at an independent position.
class WeatherTemp : public Effect
{
public:
	WeatherTemp(
		Adafruit_GFX &matrix,
		int16_t x = 0,
		int16_t y = 0,
		uint16_t text_color = color565(255, 255, 255),
		uint8_t font = TINY_WIDE_FONT
	);

	void render() override;

	void measure(String &temp_str, int16_t &x1, int16_t &y1, uint16_t &tw, uint16_t &th,
	             int16_t &cx1, int16_t &cy1, uint16_t &cw, uint16_t &ch, int16_t &total_w);
	void renderAt(int16_t text_x, int16_t text_top_y, const String &temp_str,
	              int16_t x1, int16_t y1, uint16_t tw, int16_t cx1);

private:
	int16_t  m_x;
	int16_t  m_y;
	uint16_t m_color;
	uint8_t  m_font;
};
