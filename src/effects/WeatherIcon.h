#pragma once

#include "Effect.h"
#include "Weather.h"
#include "FlyingPng.h"
#include <memory>

// Renders only the 16x16 weather condition icon from LittleFS (/weather/*.png).
class WeatherIcon : public Effect
{
public:
	static constexpr int16_t ICON_WIDTH  = 16;
	static constexpr int16_t ICON_HEIGHT = 16;

	WeatherIcon(
		Adafruit_GFX &matrix,
		int16_t x = 0,
		int16_t y = 0
	);

	void render() override;

	static const char* iconPath(WeatherCondition cond, bool is_day, uint8_t moon_phase);
	void renderAt(int16_t draw_x, int16_t draw_y);

private:
	int16_t m_x;
	int16_t m_y;

	std::unique_ptr<FlyingPng> m_cached_png;
	String                     m_cached_png_path;
	int16_t                    m_cached_x{-999};
	int16_t                    m_cached_y{-999};
};
