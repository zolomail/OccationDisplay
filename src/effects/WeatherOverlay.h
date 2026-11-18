#pragma once

#include "Effect.h"
#include "Weather.h"
#include "PngEffect.h"
#include <memory>

class WeatherOverlay : public Effect
{
public:
	static constexpr int16_t ICON_WIDTH  = 16;
	static constexpr int16_t ICON_HEIGHT = 16;

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

	std::unique_ptr<PngEffect> m_cached_png;
	String                     m_cached_png_path;
	int16_t                    m_cached_x{-999};
	int16_t                    m_cached_y{-999};
};
