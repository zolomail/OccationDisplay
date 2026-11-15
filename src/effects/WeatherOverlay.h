#pragma once

#include "Effect.h"
#include "Weather.h"

class WeatherOverlay : public Effect
{
public:
	static constexpr int16_t ICON_WIDTH  = 16;
	static constexpr int16_t ICON_HEIGHT = 8;

	WeatherOverlay(
		Adafruit_GFX &matrix,
		int16_t x = 0,
		int16_t y = 0,
		uint16_t text_color = color565(255, 255, 255),
		uint8_t font = TINY_WIDE_FONT
	);

	void render() override;

private:
	void drawIcon(int16_t x, int16_t y, WeatherCondition cond);

	int16_t  m_x;
	int16_t  m_y;
	uint16_t m_color;
	uint8_t  m_font;
};
