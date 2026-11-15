#include "WeatherOverlay.h"
#include <cmath>

namespace {

constexpr uint16_t _  = 0x0000;                      // Transparent
constexpr uint16_t SY = color565(255, 220,   0);     // Sun bright yellow
constexpr uint16_t SO = color565(255, 140,   0);     // Sun warm orange ray
constexpr uint16_t CW = color565(245, 245, 255);     // Cloud white highlight
constexpr uint16_t CL = color565(170, 180, 200);     // Cloud light grey body
constexpr uint16_t CD = color565(100, 110, 130);     // Storm cloud dark grey
constexpr uint16_t RB = color565( 40, 140, 255);     // Raindrop blue
constexpr uint16_t SW = color565(255, 255, 255);     // Snowflake white
constexpr uint16_t SC = color565(140, 230, 255);     // Hail / ice cyan

// 16x8 Sunny icon (Sun with rays)
constexpr uint16_t ICON_SUNNY[16 * 8] = {
	_,  _,  _,  _, SO,  _,  _, SY, SY,  _,  _, SO,  _,  _,  _,  _,
	_,  _,  _,  _,  _, SY, SY, SY, SY, SY, SY,  _,  _,  _,  _,  _,
	_,  _, SO,  _, SY, SY, SY, SY, SY, SY, SY, SY,  _, SO,  _,  _,
	_,  _,  _, SY, SY, SY, SY, SY, SY, SY, SY, SY, SY,  _,  _,  _,
	_,  _,  _, SY, SY, SY, SY, SY, SY, SY, SY, SY, SY,  _,  _,  _,
	_,  _, SO,  _, SY, SY, SY, SY, SY, SY, SY, SY,  _, SO,  _,  _,
	_,  _,  _,  _,  _, SY, SY, SY, SY, SY, SY,  _,  _,  _,  _,  _,
	_,  _,  _,  _, SO,  _,  _, SY, SY,  _,  _, SO,  _,  _,  _,  _
};

// 16x8 Cloudy icon (Fluffy two-tone cloud)
constexpr uint16_t ICON_CLOUDY[16 * 8] = {
	_,  _,  _,  _,  _, CW, CW, CW, CW,  _,  _,  _,  _,  _,  _,  _,
	_,  _,  _,  _, CW, CW, CW, CW, CW, CW, CW, CW,  _,  _,  _,  _,
	_,  _, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW,  _,  _,  _,
	_, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW,  _,  _,
	_, CW, CW, CL, CL, CL, CL, CL, CL, CL, CL, CL, CW, CW, CW,  _,
	_, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL,  _,
	_,  _, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL,  _,  _,
	_,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _
};

// 16x8 Rainy icon (Cloud + diagonal rain streaks)
constexpr uint16_t ICON_RAINY[16 * 8] = {
	_,  _,  _,  _,  _, CW, CW, CW, CW,  _,  _,  _,  _,  _,  _,  _,
	_,  _,  _, CW, CW, CW, CW, CW, CW, CW, CW, CW,  _,  _,  _,  _,
	_,  _, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW,  _,  _,  _,
	_, CW, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CW,  _,  _,
	_,  _, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL,  _,  _,  _,
	_,  _,  _, RB,  _,  _, RB,  _,  _, RB,  _,  _, RB,  _,  _,  _,
	_,  _, RB,  _,  _, RB,  _,  _, RB,  _,  _, RB,  _,  _,  _,  _,
	_,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _,  _
};

// 16x8 Snow icon (Cloud + falling snowflakes)
constexpr uint16_t ICON_SNOW[16 * 8] = {
	_,  _,  _,  _,  _, CW, CW, CW, CW,  _,  _,  _,  _,  _,  _,  _,
	_,  _,  _, CW, CW, CW, CW, CW, CW, CW, CW, CW,  _,  _,  _,  _,
	_,  _, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW, CW,  _,  _,  _,
	_, CW, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CW,  _,  _,
	_,  _, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL, CL,  _,  _,  _,
	_,  _,  _, SW,  _,  _,  _, SC,  _,  _,  _, SW,  _,  _,  _,  _,
	_,  _,  _,  _,  _, SW,  _,  _,  _, SW,  _,  _,  _, SC,  _,  _,
	_,  _,  _, SC,  _,  _,  _, SW,  _,  _,  _, SC,  _,  _,  _,  _
};

// 16x8 Hail / Storm icon (Dark cloud + lightning bolt + hail pellets)
constexpr uint16_t ICON_HAIL[16 * 8] = {
	_,  _,  _,  _,  _, CL, CL, CL, CL,  _,  _,  _,  _,  _,  _,  _,
	_,  _,  _, CL, CL, CL, CL, CL, CL, CL, CL, CL,  _,  _,  _,  _,
	_,  _, CL, CD, CD, CD, CD, CD, CD, CD, CD, CD, CL,  _,  _,  _,
	_, CL, CD, CD, CD, CD, CD, CD, CD, CD, CD, CD, CD, CL,  _,  _,
	_,  _, CD, CD, CD, CD, CD, SY, SY, CD, CD, CD, CD,  _,  _,  _,
	_,  _,  _, SC,  _,  _, SY, SY,  _,  _, SC,  _,  _,  _,  _,  _,
	_,  _,  _,  _,  _,  _,  _, SY, SY,  _,  _,  _, SW,  _,  _,  _,
	_,  _,  _,  _, SW,  _, SY,  _,  _,  _, SC,  _,  _,  _,  _,  _
};

} // namespace

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
	const uint16_t* bmp = ICON_CLOUDY;
	switch (cond) {
		case WeatherCondition::Sunny:  bmp = ICON_SUNNY;  break;
		case WeatherCondition::Cloudy: bmp = ICON_CLOUDY; break;
		case WeatherCondition::Rainy:  bmp = ICON_RAINY;  break;
		case WeatherCondition::Snow:   bmp = ICON_SNOW;   break;
		case WeatherCondition::Hail:   bmp = ICON_HAIL;   break;
		case WeatherCondition::Unknown:
		default:                       bmp = ICON_CLOUDY; break;
	}

	for (int16_t row = 0; row < ICON_HEIGHT; ++row) {
		const int16_t py = y + row;
		if (py < 0 || py >= m_matrix.height()) continue;

		const uint16_t* row_ptr = bmp + (row * ICON_WIDTH);
		for (int16_t col = 0; col < ICON_WIDTH; ++col) {
			const uint16_t c = row_ptr[col];
			if (c != 0x0000) {
				const int16_t px = x + col;
				if (px >= 0 && px < m_matrix.width()) {
					m_matrix.drawPixel(px, py, c);
				}
			}
		}
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
	const int16_t draw_x  = (m_x < 0) ? ((m_matrix.width() - total_w) / 2) : m_x;
	const int16_t draw_y  = (m_y < 0) ? ((m_matrix.height() - ICON_HEIGHT) / 2) : m_y;

	// 1. Draw 16x8 weather icon
	drawIcon(draw_x, draw_y, Weather::condition());

	// Vertically center text relative to the 8px icon height
	const int16_t text_x = draw_x + ICON_WIDTH + 2;
	const int16_t text_y = draw_y + ((ICON_HEIGHT - static_cast<int16_t>(th)) / 2) - y1;

	// 2. Draw temperature number
	m_matrix.setCursor(text_x - x1, text_y);
	m_matrix.print(temp_str.c_str());

	// 3. Draw 2x2 pixel degree symbol (°) + "C"
	const int16_t deg_x = text_x + tw + 1;
	const int16_t deg_y = draw_y + 1;
	m_matrix.drawRect(deg_x, deg_y, 2, 2, m_color);

	m_matrix.setCursor(deg_x + 3 - cx1, text_y);
	m_matrix.print("C");
}
