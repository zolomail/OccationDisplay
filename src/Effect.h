#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#define NORMAL_FONT			0
#define TINY_WIDE_FONT 		1
#include <TinyWide.h>

static constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

class Effect
{
public:
	Effect(Adafruit_GFX &matrix) : m_matrix(matrix) {};
	virtual void render() {
		m_matrix.setTextColor(0xFFFF);
		m_matrix.setCursor(0,0);
		m_matrix.print("ERROR!");
	};

	void setFont(uint8_t font)
	{
		switch(font)
		{
			case TINY_WIDE_FONT:
				m_matrix.setFont(&TinyWide);
				break;
			default:
				m_matrix.setFont();
				break;
		}
	};

protected:
	Adafruit_GFX &m_matrix;
};