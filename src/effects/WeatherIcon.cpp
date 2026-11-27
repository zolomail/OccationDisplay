#include "WeatherIcon.h"

WeatherIcon::WeatherIcon(Adafruit_GFX &matrix, int16_t x, int16_t y)
	: Effect(matrix), m_x(x), m_y(y)
{
}

const char* WeatherIcon::iconPath(WeatherCondition cond, bool is_day, uint8_t moon_phase)
{
	if (cond == WeatherCondition::Sunny) {
		if (is_day) return "/weather/sunny_day.png";
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
		return MOON_FILES[moon_phase & 7];
	}

	switch (cond) {
		case WeatherCondition::Cloudy:
			return is_day ? "/weather/cloudy_day.png" : "/weather/cloudy_night.png";
		case WeatherCondition::Rainy:
			return is_day ? "/weather/rainy_day.png" : "/weather/rainy_night.png";
		case WeatherCondition::Snow:
			return is_day ? "/weather/snow_day.png" : "/weather/snow_night.png";
		case WeatherCondition::Hail:
			return is_day ? "/weather/hail_day.png" : "/weather/hail_night.png";
		default:
			return is_day ? "/weather/cloudy_day.png" : "/weather/cloudy_night.png";
	}
}

void WeatherIcon::renderAt(int16_t draw_x, int16_t draw_y)
{
	const char* png_path = iconPath(Weather::condition(), Weather::isDay(), Weather::moonPhase());
	if (!png_path) return;

	if (!m_cached_png || m_cached_png_path != png_path || m_cached_x != draw_x || m_cached_y != draw_y) {
		m_cached_png = std::make_unique<FlyingPng>(m_matrix, png_path, draw_x, draw_y);
		m_cached_png_path = png_path;
		m_cached_x = draw_x;
		m_cached_y = draw_y;
	}

	if (m_cached_png && m_cached_png->isValid()) {
		m_cached_png->render();
	}
}

void WeatherIcon::render()
{
	const int16_t draw_x = resolveX(m_x, ICON_WIDTH);
	const int16_t draw_y = resolveY(m_y, ICON_HEIGHT);
	renderAt(draw_x, draw_y);
}
