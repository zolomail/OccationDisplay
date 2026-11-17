#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "fonts/TinyWide.h"
#include "fonts/SmallWide.h"
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

// Position anchors in int16_t space.
// Each anchor is spaced by 0x1000 (4096), giving a +/- 2048 pixel offset window around every anchor
// so expressions like (YPOS_BOTTOM - 10) or (XPOS_CENTER + 8) work without colliding with negative coordinates.
enum PosAnchor : int16_t {
	XPOS_CENTER  = 0x2000, //  8192: Centered horizontally
	XPOS_1_QUART = 0x3000, // 12288: Centered in the left half (1/4 width)
	XPOS_3_QUART = 0x4000, // 16384: Centered in the right half (3/4 width)
	XPOS_RIGHT   = 0x5000, // 20480: Right-aligned to (width - 1)

	YPOS_CENTER  = 0x2000, //  8192: Centered vertically
	YPOS_1_QUART = 0x3000, // 12288: Centered in the top half (1/4 height)
	YPOS_3_QUART = 0x4000, // 16384: Centered in the bottom half (3/4 height)
	YPOS_BOTTOM  = 0x5000  // 20480: Bottom-aligned to (height - 1)
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

	int16_t resolveX(int16_t x_spec, uint16_t item_width = 0) const
	{
		return resolveCoord(x_spec, m_matrix.width(), item_width);
	}

	int16_t resolveY(int16_t y_spec, uint16_t item_height = 0) const
	{
		return resolveCoord(y_spec, m_matrix.height(), item_height);
	}

protected:
	int16_t resolveCoord(int16_t spec, int16_t display_size, uint16_t item_size) const
	{
		const int16_t w = static_cast<int16_t>(item_size);
		if (spec >= (XPOS_CENTER - 2048) && spec < (XPOS_CENTER + 2048)) {
			return ((display_size - w) / 2) + (spec - XPOS_CENTER);
		}
		if (spec >= (XPOS_1_QUART - 2048) && spec < (XPOS_1_QUART + 2048)) {
			return ((display_size / 4) - (w / 2)) + (spec - XPOS_1_QUART);
		}
		if (spec >= (XPOS_3_QUART - 2048) && spec < (XPOS_3_QUART + 2048)) {
			return (((display_size * 3) / 4) - (w / 2)) + (spec - XPOS_3_QUART);
		}
		if (spec >= (XPOS_RIGHT - 2048) && spec < (XPOS_RIGHT + 2048)) {
			const int16_t align = (w > 0) ? w : 1;
			return (display_size - align) + (spec - XPOS_RIGHT);
		}
		return spec;
	}

	Adafruit_GFX &m_matrix;
};