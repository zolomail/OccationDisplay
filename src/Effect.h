#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "TinyWide.h"
#include "SmallWide.h"
#include <Fonts/Org_01.h>
#include <Fonts/TomThumb.h>
#include <Fonts/Picopixel.h>

enum FontId : uint8_t {
	NORMAL_FONT     = 0, // Built-in 5x7 monospace (CP437)
	TINY_WIDE_FONT  = 1, // Custom 6x5 wide pixel font (+ 0x80/0x81 icons)
	SMALL_WIDE_FONT = 2, // Custom 6x8 caps / 5x5 lowercase wide font (+ 0x80/0x81 icons)
	ORG_01_FONT     = 3, // Adafruit 5x5 retro wide font
	TOM_THUMB_FONT  = 4, // Adafruit 3x5 compact pixel font
	PICOPIXEL_FONT  = 5  // Adafruit 3x5 proportional micro font
};

static constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

class Effect
{
public:
	Effect(Adafruit_GFX &matrix) : m_matrix(matrix) {}
	virtual ~Effect() = default;

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
			case SMALL_WIDE_FONT:
				m_matrix.setFont(&SmallWide);
				break;
			case ORG_01_FONT:
				m_matrix.setFont(&Org_01);
				break;
			case TOM_THUMB_FONT:
				m_matrix.setFont(&TomThumb);
				break;
			case PICOPIXEL_FONT:
				m_matrix.setFont(&Picopixel);
				break;
			default:
				m_matrix.setFont();
				break;
		}
	};

protected:
	Adafruit_GFX &m_matrix;
};