#include "Border.h"
#include <algorithm>

namespace {

inline void unpack565(uint16_t c, uint8_t &r, uint8_t &g, uint8_t &b)
{
	r = static_cast<uint8_t>((c >> 11) & 0x1Fu);
	g = static_cast<uint8_t>((c >> 5)  & 0x3Fu);
	b = static_cast<uint8_t>( c        & 0x1Fu);
	r = (r << 3) | (r >> 2);
	g = (g << 2) | (g >> 4);
	b = (b << 3) | (b >> 2);
}

inline uint16_t lerpColor(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2, uint8_t t)
{
	const uint8_t r = static_cast<uint8_t>(r1 + (((r2 - r1) * t) >> 8));
	const uint8_t g = static_cast<uint8_t>(g1 + (((g2 - g1) * t) >> 8));
	const uint8_t b = static_cast<uint8_t>(b1 + (((b2 - b1) * t) >> 8));
	return color565(r, g, b);
}

// Fast integer HSV hue (0..255) to RGB565
inline uint16_t hueTo565(uint8_t hue)
{
	const uint8_t region    = hue / 43;
	const uint8_t remainder = (hue - (region * 43)) * 6;
	const uint8_t q         = 255 - remainder;
	const uint8_t t         = remainder;

	switch (region) {
		case 0:  return color565(255,   t,   0);
		case 1:  return color565(  q, 255,   0);
		case 2:  return color565(  0, 255,   t);
		case 3:  return color565(  0,   q, 255);
		case 4:  return color565(  t,   0, 255);
		default: return color565(255,   0,   q);
	}
}

inline uint32_t hashPixel(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

} // namespace

Border::Border(
	Adafruit_GFX &matrix,
	uint8_t thickness,
	uint16_t color1,
	uint16_t color2,
	BorderPattern pattern,
	uint8_t stripe_len,
	int8_t speed
) :
	Effect(matrix),
	m_thickness(thickness < 1 ? 1 : thickness),
	m_color1(color1),
	m_color2(color2),
	m_pattern(pattern),
	m_stripe_len(stripe_len < 1 ? 1 : stripe_len),
	m_speed(speed)
{
	unpack565(m_color1, m_r1, m_g1, m_b1);
	unpack565(m_color2, m_r2, m_g2, m_b2);
}

uint16_t Border::evalColor(int32_t perim_pos, uint8_t ring, int32_t perim_len)
{
	// Shift position by animation phase (in 1/4-pixel units) and diagonal ring offset
	const int32_t shifted = perim_pos - (m_phase >> 2) + ring;

	switch (m_pattern) {
		case BorderPattern::Solid:
			return ((ring & 1u) == 0 || m_color1 == m_color2) ? m_color1 : m_color2;

		case BorderPattern::Dashed: {
			const int32_t period = static_cast<int32_t>(m_stripe_len) * 2;
			int32_t m = shifted % period;
			if (m < 0) m += period;
			return (m < m_stripe_len) ? m_color1 : m_color2;
		}

		case BorderPattern::Chase: {
			const int32_t period = static_cast<int32_t>(m_stripe_len) * 2;
			int32_t m = shifted % period;
			if (m < 0) m += period;
			// Triangle wave 0..255..0 across period
			const int32_t half = m_stripe_len;
			const int32_t dist = (m < half) ? m : (period - m);
			const uint8_t t    = static_cast<uint8_t>((dist * 255) / half);
			return lerpColor(m_r1, m_g1, m_b1, m_r2, m_g2, m_b2, t);
		}

		case BorderPattern::Twinkle: {
			const uint32_t slot = static_cast<uint32_t>(perim_pos / m_stripe_len) + (ring * 131u);
			const uint32_t tick = static_cast<uint32_t>(m_phase >> 3);
			const uint32_t h1   = hashPixel(slot * 0x9E3779B9u + tick);
			const uint8_t  t    = static_cast<uint8_t>(h1 & 0xFFu);
			return lerpColor(m_r1, m_g1, m_b1, m_r2, m_g2, m_b2, t);
		}

		case BorderPattern::Rainbow: {
			if (perim_len <= 0) perim_len = 1;
			int32_t m = shifted % perim_len;
			if (m < 0) m += perim_len;
			const uint8_t hue = static_cast<uint8_t>((m * 256) / perim_len);
			return hueTo565(hue);
		}
	}
	return m_color1;
}

void Border::render()
{
	const int16_t w = m_matrix.width();
	const int16_t h = m_matrix.height();
	m_phase += m_speed;

	// Fast path for Solid border using hardware rectangle primitives
	if (m_pattern == BorderPattern::Solid) {
		for (uint8_t t = 0; t < m_thickness; ++t) {
			const int16_t rw = w - 2 * t;
			const int16_t rh = h - 2 * t;
			if (rw <= 0 || rh <= 0) break;
			const uint16_t c = ((t & 1u) == 0 || m_color1 == m_color2) ? m_color1 : m_color2;
			m_matrix.drawRect(t, t, rw, rh, c);
		}
		return;
	}

	// Perimeter-walk for patterned/animated borders
	for (uint8_t t = 0; t < m_thickness; ++t) {
		const int16_t x0 = t;
		const int16_t y0 = t;
		const int16_t x1 = w - 1 - t;
		const int16_t y1 = h - 1 - t;
		if (x0 > x1 || y0 > y1) break;

		const int32_t span_w    = x1 - x0;
		const int32_t span_h    = y1 - y0;
		const int32_t perim_len = 2 * (span_w + span_h);
		int32_t pos = 0;

		// Top edge (left to right)
		for (int16_t x = x0; x <= x1; ++x, ++pos) {
			m_matrix.drawPixel(x, y0, evalColor(pos, t, perim_len));
		}
		// Right edge (top+1 to bottom-1)
		for (int16_t y = y0 + 1; y < y1; ++y, ++pos) {
			m_matrix.drawPixel(x1, y, evalColor(pos, t, perim_len));
		}
		// Bottom edge (right to left)
		if (y1 > y0) {
			for (int16_t x = x1; x >= x0; --x, ++pos) {
				m_matrix.drawPixel(x, y1, evalColor(pos, t, perim_len));
			}
		}
		// Left edge (bottom-1 to top+1)
		if (x1 > x0) {
			for (int16_t y = y1 - 1; y > y0; --y, ++pos) {
				m_matrix.drawPixel(x0, y, evalColor(pos, t, perim_len));
			}
		}
	}
}
