#include "WeatherOverlay.h"
#include <cmath>

WeatherOverlay::WeatherOverlay(
	Adafruit_GFX &matrix,
	int16_t x,
	int16_t y,
	uint16_t text_color,
	uint8_t font
) :
	Effect(matrix),
	m_x(x),
	m_y(y),
	m_color(text_color),
	m_font(font)
{
}

void WeatherOverlay::drawIcon(int16_t x, int16_t y, WeatherCondition cond)
{
	const bool is_day = Weather::isDay();
	const char* png_path = nullptr;

	if (cond == WeatherCondition::Sunny) {
		if (is_day) {
			png_path = "/weather/sunny_day.png";
		} else {
			static const char* const MOON_FILES[8] = {
				"/weather/moon_new.png",
				"/weather/moon_waxing_crescent.png",
				"/weather/moon_first_quarter.png",
				"/weather/moon_waxing_gibbous.png",
				"/weather/moon_full.png",
				"/weather/moon_waning_gibbous.png",
				"/weather/moon_last_quarter.png",
				"/weather/moon_waning_crescent.png"
			};
			png_path = MOON_FILES[Weather::moonPhase() & 7];
		}
	} else {
		switch (cond) {
			case WeatherCondition::Cloudy:
				png_path = is_day ? "/weather/cloudy_day.png" : "/weather/cloudy_night.png";
				break;
			case WeatherCondition::Rainy:
				png_path = is_day ? "/weather/rainy_day.png" : "/weather/rainy_night.png";
				break;
			case WeatherCondition::Snow:
				png_path = is_day ? "/weather/snow_day.png" : "/weather/snow_night.png";
				break;
			case WeatherCondition::Hail:
				png_path = is_day ? "/weather/hail_day.png" : "/weather/hail_night.png";
				break;
			default:
				png_path = is_day ? "/weather/cloudy_day.png" : "/weather/cloudy_night.png";
				break;
		}
	}

	if (!png_path) return;

	// Load and cache PNG icon; reload only if icon path or position changes
	if (!m_cached_png || m_cached_png_path != png_path || m_cached_x != x || m_cached_y != y) {
		m_cached_png = std::make_unique<PngEffect>(m_matrix, png_path, x, y);
		m_cached_png_path = png_path;
		m_cached_x = x;
		m_cached_y = y;
	}

	if (m_cached_png && m_cached_png->isValid()) {
		m_cached_png->render();
	}
}

void WeatherOverlay::render()
{
	const bool valid = Weather::isValid();
	const int temp_rounded = valid ? static_cast<int>(lroundf(Weather::temperature())) : 0;
	const String temp_str  = valid ? String(temp_rounded) : String("--");

	setFont(m_font);
	m_matrix.setTextWrap(false);
	m_matrix.setTextColor(m_color);

	int16_t x1 = 0, y1 = 0;
	uint16_t tw = 0, th = 0;
	m_matrix.getTextBounds(temp_str, 0, 0, &x1, &y1, &tw, &th);

	int16_t cx1 = 0, cy1 = 0;
	uint16_t cw = 0, ch = 0;
	m_matrix.getTextBounds("C", 0, 0, &cx1, &cy1, &cw, &ch);

	// Total width = 16px icon + 2px gap + temp digits + 4px degree symbol + "C"
	const int16_t total_w = ICON_WIDTH + 2 + tw + 4 + cw;
	const int16_t draw_x  = resolveX(m_x, total_w);
	const int16_t draw_y  = resolveY(m_y, ICON_HEIGHT);

	// 1. Draw 16x16 weather icon from LittleFS (/weather/*.png)
	drawIcon(draw_x, draw_y, Weather::condition());

	// Vertically center temperature text relative to the 16px icon height
	const int16_t text_x = draw_x + ICON_WIDTH + 2;
	const int16_t text_y = draw_y + ((ICON_HEIGHT - static_cast<int16_t>(th)) / 2) - y1;

	// 2. Draw temperature number
	m_matrix.setCursor(text_x - x1, text_y);
	m_matrix.print(temp_str.c_str());

	// 3. Draw 2x2 pixel degree symbol (°) + "C"
	const int16_t deg_x = text_x + tw + 1;
	const int16_t deg_y = draw_y + ((ICON_HEIGHT - static_cast<int16_t>(th)) / 2) - 1;
	m_matrix.drawRect(deg_x, deg_y, 2, 2, m_color);

	m_matrix.setCursor(deg_x + 3 - cx1, text_y);
	m_matrix.print("C");
}
