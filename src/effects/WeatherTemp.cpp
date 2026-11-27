#include "WeatherTemp.h"
#include <cmath>

WeatherTemp::WeatherTemp(
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

void WeatherTemp::measure(
	String &temp_str, int16_t &x1, int16_t &y1, uint16_t &tw, uint16_t &th,
	int16_t &cx1, int16_t &cy1, uint16_t &cw, uint16_t &ch, int16_t &total_w)
{
	const bool valid = Weather::isValid();
	const int temp_rounded = valid ? static_cast<int>(lroundf(Weather::temperature())) : 0;
	temp_str = valid ? String(temp_rounded) : String("--");

	setFont(m_font);
	m_matrix.setTextWrap(false);
	m_matrix.setTextColor(m_color);

	m_matrix.getTextBounds(temp_str, 0, 0, &x1, &y1, &tw, &th);
	m_matrix.getTextBounds("C", 0, 0, &cx1, &cy1, &cw, &ch);
	total_w = static_cast<int16_t>(tw) + 4 + static_cast<int16_t>(cw);
}

void WeatherTemp::renderAt(
	int16_t text_x, int16_t text_top_y, const String &temp_str,
	int16_t x1, int16_t y1, uint16_t tw, int16_t cx1)
{
	const int16_t baseline_y = text_top_y - y1;
	m_matrix.setCursor(text_x - x1, baseline_y);
	m_matrix.print(temp_str.c_str());

	const int16_t deg_x = text_x + static_cast<int16_t>(tw) + 1;
	const int16_t deg_y = text_top_y - 1;
	m_matrix.drawRect(deg_x, deg_y, 2, 2, m_color);

	m_matrix.setCursor(deg_x + 3 - cx1, baseline_y);
	m_matrix.print("C");
}

void WeatherTemp::render()
{
	String temp_str;
	int16_t x1 = 0, y1 = 0, cx1 = 0, cy1 = 0, total_w = 0;
	uint16_t tw = 0, th = 0, cw = 0, ch = 0;
	measure(temp_str, x1, y1, tw, th, cx1, cy1, cw, ch, total_w);

	const int16_t draw_x = resolveX(m_x, total_w);
	const int16_t draw_y = resolveY(m_y, static_cast<int16_t>(th));
	renderAt(draw_x, draw_y, temp_str, x1, y1, tw, cx1);
}
