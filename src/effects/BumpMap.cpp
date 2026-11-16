#include "BumpMap.h"
#include <cmath>

namespace {

inline uint32_t fastRand(uint32_t& state)
{
	uint32_t x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	state = x;
	return x;
}

void applyFont(Adafruit_GFX &gfx, uint8_t font)
{
	switch (font) {
		case TINY_WIDE_FONT:  gfx.setFont(&TinyWide);  break;
		case SMALL_WIDE_FONT: gfx.setFont(&SmallWide); break;
		case ORG_01_FONT:     gfx.setFont(&Org_01);    break;
		case TOM_THUMB_FONT:  gfx.setFont(&TomThumb);  break;
		case PICOPIXEL_FONT:  gfx.setFont(&Picopixel); break;
		default:              gfx.setFont();           break;
	}
}

} // namespace

BumpMap::BumpMap(
	Adafruit_GFX &matrix,
	uint16_t base_color,
	const String &text,
	bool text_only,
	uint8_t font,
	uint8_t text_size
) :
	Effect(matrix),
	m_width(m_matrix.width()),
	m_height(m_matrix.height()),
	m_nx(static_cast<size_t>(m_width) * m_height, 0),
	m_ny(static_cast<size_t>(m_width) * m_height, 0)
{
	// 1. Unpack RGB565 base_color to 8-bit R, G, B
	uint8_t base_r = static_cast<uint8_t>((base_color >> 11) & 0x1Fu);
	uint8_t base_g = static_cast<uint8_t>((base_color >> 5)  & 0x3Fu);
	uint8_t base_b = static_cast<uint8_t>( base_color        & 0x1Fu);
	base_r = (base_r << 3) | (base_r >> 2);
	base_g = (base_g << 2) | (base_g >> 4);
	base_b = (base_b << 3) | (base_b >> 2);

	// 2. Precompute 256-entry Phong lighting LUT indexed by squared normal-deflected distance:
	//    d2 = (dx*dx + dy*dy) >> 3  (0 = direct specular reflection, 255 = dark ambient shadow)
	constexpr float MAX_DIST = 45.2548f; // sqrt(256 * 8)
	for (int d2 = 0; d2 < 256; ++d2) {
		const float dist = sqrtf(static_cast<float>(d2 * 8));
		float norm = 1.0f - (dist / MAX_DIST);
		if (norm < 0.0f) norm = 0.0f;

		// Diffuse component (smooth quadratic falloff) + Specular highlight (norm^6)
		const float diffuse  = norm * norm;
		const float norm2    = diffuse;
		const float specular = norm2 * norm2 * norm2; // norm^6

		int r = static_cast<int>(base_r * diffuse + 255.0f * specular);
		int g = static_cast<int>(base_g * diffuse + 255.0f * specular);
		int b = static_cast<int>(base_b * diffuse + 255.0f * specular);

		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;

		m_palette[d2] = color565(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
	}

	// 3. Build procedural heightmap with a 1-pixel guard border ((width + 2) x (height + 2))
	//    so central-difference gradients at the top/bottom/left/right edges are seamless
	const uint16_t stride = m_width + 2;
	const uint16_t rows   = m_height + 2;
	std::vector<int16_t> height(static_cast<size_t>(stride) * rows, 0);
	uint32_t rng = 0x13579BDFu;

	const float cx1 = m_width  * 0.28f;
	const float cy1 = m_height * 0.50f;
	const float cx2 = m_width  * 0.72f;
	const float cy2 = m_height * 0.50f;

	for (uint16_t py = 0; py < rows; ++py) {
		const float fy = static_cast<float>(static_cast<int16_t>(py) - 1);
		for (uint16_t px = 0; px < stride; ++px) {
			int16_t h_val = 0;
			if (!text_only) {
				const float fx = static_cast<float>(static_cast<int16_t>(px) - 1);

				// Egg-crate sinusoidal hills + dual interference rings
				const float hills = sinf(fx * 0.30f) * cosf(fy * 0.30f) * 14.0f;
				const float d1    = sqrtf((fx - cx1) * (fx - cx1) + (fy - cy1) * (fy - cy1));
				const float d2    = sqrtf((fx - cx2) * (fx - cx2) + (fy - cy2) * (fy - cy2));
				const float rings = (sinf(d1 * 0.55f) + sinf(d2 * 0.55f)) * 9.0f;
				h_val = static_cast<int16_t>(hills + rings);
			}

			// Subtle micro-grain for metallic surface texture
			const int16_t grain = static_cast<int16_t>(fastRand(rng) & 0x03u) - 1;
			height[static_cast<size_t>(py) * stride + px] = h_val + grain;
		}
	}

	// 4. Optional embossed text (rasterized into a 1-bit canvas and beveled into the heightmap)
	if (text.length() > 0) {
		GFXcanvas1 canvas(m_width, m_height);
		canvas.fillScreen(0);
		applyFont(canvas, font);
		canvas.setTextWrap(false);

		uint8_t sz = (text_size < 1) ? 1 : text_size;
		canvas.setTextSize(sz);

		int16_t x1 = 0, y1 = 0;
		uint16_t tw = 0, th = 0;
		canvas.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);

		// Auto-shrink to size 1 if text is wider than the display at size > 1
		if (tw > m_width - 2 && sz > 1) {
			sz = 1;
			canvas.setTextSize(1);
			canvas.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
		}

		const int16_t cx = (static_cast<int16_t>(m_width)  - static_cast<int16_t>(tw)) / 2 - x1;
		const int16_t cy = (static_cast<int16_t>(m_height) - static_cast<int16_t>(th)) / 2 - y1;
		canvas.setCursor(cx, cy);
		canvas.setTextColor(1);
		canvas.print(text);

		// Apply a 3x3 beveled emboss kernel around every lit text pixel (offset by +1 for guard border)
		for (int16_t y = 0; y < static_cast<int16_t>(m_height); ++y) {
			for (int16_t x = 0; x < static_cast<int16_t>(m_width); ++x) {
				if (!canvas.getPixel(x, y)) continue;

				for (int16_t dy = -1; dy <= 1; ++dy) {
					const size_t py = static_cast<size_t>(y + 1 + dy);
					for (int16_t dx = -1; dx <= 1; ++dx) {
						const size_t px = static_cast<size_t>(x + 1 + dx);
						const int16_t weight = (dx == 0 && dy == 0) ? 16 : ((dx == 0 || dy == 0) ? 8 : 4);
						height[py * stride + px] += weight;
					}
				}
			}
		}
	}

	// 5. Finite-difference surface gradients using the 1-pixel guard border
	for (uint16_t y = 0; y < m_height; ++y) {
		const size_t py = static_cast<size_t>(y + 1);

		for (uint16_t x = 0; x < m_width; ++x) {
			const size_t px = static_cast<size_t>(x + 1);

			int16_t gx = height[py * stride + (px - 1)] -
			             height[py * stride + (px + 1)];
			int16_t gy = height[(py - 1) * stride + px] -
			             height[(py + 1) * stride + px];

			if (gx > 127)       gx = 127;
			else if (gx < -127) gx = -127;
			if (gy > 127)       gy = 127;
			else if (gy < -127) gy = -127;

			const size_t idx = static_cast<size_t>(y) * m_width + x;
			m_nx[idx] = static_cast<int8_t>(gx);
			m_ny[idx] = static_cast<int8_t>(gy);
		}
	}
}

void BumpMap::render()
{
	// Move point light along a smooth Lissajous figure-8 trajectory
	m_light_phase += 0.045f;
	if (m_light_phase > 628.3185f) {
		m_light_phase -= 628.3185f;
	}

	const float half_w = m_width  * 0.5f;
	const float half_h = m_height * 0.5f;
	const int32_t lx = static_cast<int32_t>(half_w + cosf(m_light_phase) * (half_w - 6.0f));
	const int32_t ly = static_cast<int32_t>(half_h + sinf(m_light_phase * 2.1f) * (half_h - 3.0f));

	const uint16_t w  = m_width;
	const uint16_t h  = m_height;
	const int8_t*  nx = m_nx.data();
	const int8_t*  ny = m_ny.data();

	size_t idx = 0;
	for (uint16_t y = 0; y < h; ++y) {
		const int32_t vy = static_cast<int32_t>(y) - ly;

		for (uint16_t x = 0; x < w; ++x, ++idx) {
			const int32_t vx = static_cast<int32_t>(x) - lx;

			const int32_t dx = vx + nx[idx];
			const int32_t dy = vy + ny[idx];

			int32_t d2 = (dx * dx + dy * dy) >> 3;
			if (d2 > 255) {
				d2 = 255;
			}

			m_matrix.drawPixel(x, y, m_palette[d2]);
		}
	}
}
