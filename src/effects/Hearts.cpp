#include "Hearts.h"
#include <algorithm>
#include <cmath>

namespace {

inline int32_t fastSqrt(int32_t val)
{
	return static_cast<int32_t>(sqrtf(static_cast<float>(val)));
}

// Coordinate range of the normalized 64x64 SDF lookup table:
//   u in [0.0, 1.35]  (horizontal |dx| / r)
//   v in [-1.25, 1.25] (vertical dy / r)
constexpr float SDF_U_MAX = 1.35f;
constexpr float SDF_V_MIN = -1.25f;
constexpr float SDF_V_MAX =  1.25f;

} // namespace

HeartParticle::HeartParticle() :
	m_dx(0),
	m_x(0),
	m_y(0),
	m_r(0),
	m_dr(0)
{
}

HeartParticle::HeartParticle(const HeartParticle &f) :
	m_dx(random(-48, 49)),
	m_x(random(0, f.m_x) << 8),
	m_y(random(0, f.m_y) << 8),
	m_r(random(3 << 8, 7 << 8)),
	m_dr((random(0, 2) == 0) ? random(3, 8) : 0)
{
}

HeartParticle::HeartParticle(int32_t x, int32_t y, int32_t r) :
	m_dx(0),
	m_x(x),
	m_y(y),
	m_r(r),
	m_dr(0)
{
}

void HeartParticle::reset(int32_t max_x, int32_t max_y)
{
	m_dr = (random(0, 2) == 0) ? random(3, 8) : 0;
	m_r  = (m_dr > 0) ? random(3 << 8, 5 << 8) : random(4 << 8, 8 << 8);
	m_dx = random(-48, 49);
	m_x  = random(0, max_x << 8);
	m_y  = (max_y << 8) + ((m_r * 5) >> 2);
}

void HeartParticle::move()
{
	m_dx = std::min<int32_t>(std::max<int32_t>(m_dx + static_cast<int32_t>(random(-3, 4)), -48), 48);
	m_x += m_dx;
	m_y -= 22 + (m_r >> 5);
	if (m_dr > 0 && m_r < (11 << 8)) {
		m_r += m_dr;
	}
}

void HeartParticle::render(Adafruit_GFX &matrix, const uint16_t sdf[64][64])
{
	drawHeartAntialized(matrix, sdf);

	// Glossy white/rose highlight inside the upper-left circular lobe
	const int32_t offset_x = (m_r * 11) / 24;
	const int32_t offset_y = (m_r * 9) / 24;
	fillHighlightAntialized(matrix, m_x - offset_x, m_y - offset_y, std::max<int32_t>(m_r / 4, 180));
}

void HeartParticle::drawHeartAntialized(Adafruit_GFX &matrix, const uint16_t sdf[64][64])
{
	const int32_t margin = m_r + 384; // m_r + 1.5 px margin
	const int16_t x_min  = static_cast<int16_t>(std::max<int32_t>((m_x - margin) >> 8, 0));
	const int16_t x_max  = static_cast<int16_t>(std::min<int32_t>((m_x + margin) >> 8, matrix.width() - 1));
	const int16_t y_min  = static_cast<int16_t>(std::max<int32_t>((m_y - margin) >> 8, 0));
	const int16_t y_max  = static_cast<int16_t>(std::min<int32_t>((m_y + margin) >> 8, matrix.height() - 1));

	// Fixed-point conversion factors from world offset (in 24.8) to 64x64 LUT coordinates (in 16.8):
	//   u_idx = (|dx| / m_r) * (63 / SDF_U_MAX)
	//   v_idx = ((dy / m_r) - SDF_V_MIN) * (63 / (SDF_V_MAX - SDF_V_MIN))
	// Precompute (1 / m_r) scale per heart once per frame:
	const int32_t u_step = (static_cast<int32_t>(63.0f / SDF_U_MAX * 65536.0f)) / m_r;
	const int32_t v_step = (static_cast<int32_t>(63.0f / (SDF_V_MAX - SDF_V_MIN) * 65536.0f)) / m_r;
	const int32_t v_zero = static_cast<int32_t>((-SDF_V_MIN / (SDF_V_MAX - SDF_V_MIN)) * 63.0f * 256.0f);

	// Stroke half-thickness in 24.8 fixed-point (~1.25 pixels so lines are bold and never flicker)
	constexpr int32_t HALF_STROKE = 320;

	for (int16_t py = y_min; py <= y_max; ++py) {
		const int32_t dy   = (static_cast<int32_t>(py) << 8) + 128 - m_y;
		const int32_t v_fp = v_zero + ((dy * v_step) >> 8);
		if (v_fp < 0 || v_fp >= (63 << 8)) {
			continue;
		}
		const int32_t vi = v_fp >> 8;
		const int32_t vf = v_fp & 0xFF;

		for (int16_t px = x_min; px <= x_max; ++px) {
			const int32_t dx   = std::abs((static_cast<int32_t>(px) << 8) + 128 - m_x);
			const int32_t u_fp = (dx * u_step) >> 8;
			if (u_fp >= (63 << 8)) {
				continue;
			}
			const int32_t ui = u_fp >> 8;
			const int32_t uf = u_fp & 0xFF;

			// Bilinearly interpolate normalized distance from the 64x64 SDF LUT
			const int32_t d00 = sdf[vi][ui];
			const int32_t d10 = sdf[vi][ui + 1];
			const int32_t d01 = sdf[vi + 1][ui];
			const int32_t d11 = sdf[vi + 1][ui + 1];

			const int32_t d0 = d00 + (((d10 - d00) * uf) >> 8);
			const int32_t d1 = d01 + (((d11 - d01) * uf) >> 8);
			const int32_t norm_dist = d0 + (((d1 - d0) * vf) >> 8); // 8.8 fixed-point normalized distance

			// Convert normalized distance to pixel distance in 24.8 fixed-point
			const int32_t pixel_dist = (norm_dist * m_r) >> 8;
			if (pixel_dist < HALF_STROKE) {
				// Smooth tent filter: 255 at exact heart curve (pixel_dist == 0), fading to 0 at HALF_STROKE
				const uint8_t alpha = static_cast<uint8_t>(((HALF_STROKE - pixel_dist) * 255) / HALF_STROKE);
				matrix.drawPixel(px, py, color565(alpha, alpha >> 4, alpha >> 3));
			}
		}
	}
}

void HeartParticle::fillHighlightAntialized(Adafruit_GFX &matrix, int32_t cx, int32_t cy, int32_t cr)
{
	const int32_t rr = (cr + 128) * (cr + 128);
	int32_t pixel_y  = std::max<int32_t>((cy - cr) >> 8, 0) << 8;
	int32_t yy       = pixel_y + 128 - cy;
	int32_t yyy      = (yy + 0x100) * (yy + 0x100);
	yy *= yy;
	yyy -= yy;
	const int32_t yyyy = 2 * 0x100 * 0x100;

	for (; pixel_y <= std::min<int32_t>((cy + cr) >> 8, matrix.height() - 1) << 8; pixel_y += 0x100)
	{
		const int32_t delta_x = fastSqrt(rr - yy);
		const int32_t x_l     = cx - delta_x;
		const int32_t x_r     = cx + delta_x;

		if (x_r >= 0 && (x_l >> 8) < matrix.width())
		{
			int32_t pixel_x = std::max<int32_t>(x_l >> 8, 0) << 8;
			int32_t xx      = pixel_x + 128 - cx;
			int32_t xxx     = (xx + 0x100) * (xx + 0x100);
			xx *= xx;
			xxx -= xx;
			const int32_t xxxx = 2 * 0x100 * 0x100;

			for (; pixel_x <= std::min<int32_t>(x_r >> 8, matrix.width() - 1) << 8; pixel_x += 0x100)
			{
				const int32_t dist = ((fastSqrt(xx + yy) - cr) * 300) >> 8;
				if (dist < 0) {
					matrix.drawPixel(pixel_x >> 8, pixel_y >> 8, color565(255, 255, 255));
				} else if (dist <= 255) {
					const uint8_t a = static_cast<uint8_t>(255 - dist);
					matrix.drawPixel(pixel_x >> 8, pixel_y >> 8, color565(a, (a * 3) >> 2, (a * 3) >> 2));
				}
				xx  += xxx;
				xxx += xxxx;
			}
		}
		yy  += yyy;
		yyy += yyyy;
	}
}

Hearts::Hearts(Adafruit_GFX &matrix, int16_t count) :
	Effect(matrix),
	m_hearts(count, {m_matrix.width(), m_matrix.height()})
{
	// Generate 160 sample points along a plump, rounded half-heart curve:
	//   1. Upper circular lobe centered at (0.48, -0.32) with radius 0.56
	//      from top cleft angle (-149 deg) around to outer cheek (+22 deg)
	//   2. Smooth outward-curved belly Bezier from outer cheek to bottom tip (0.0, +0.95)
	constexpr int NUM_SAMPLES = 160;
	float curve_x[NUM_SAMPLES];
	float curve_y[NUM_SAMPLES];

	constexpr int ARC_SAMPLES = 96;
	constexpr float LOBE_CX =  0.48f;
	constexpr float LOBE_CY = -0.32f;
	constexpr float LOBE_R  =  0.56f;
	// At x = 0: cos(theta) = -0.48 / 0.56 => theta_start = -2.601 rad (-149 deg)
	constexpr float THETA_START = -2.60117f;
	constexpr float THETA_END   =  0.38000f; // Outer right cheek (~ +21.8 deg)

	for (int i = 0; i < ARC_SAMPLES; ++i) {
		const float t     = static_cast<float>(i) / static_cast<float>(ARC_SAMPLES - 1);
		const float theta = THETA_START + t * (THETA_END - THETA_START);
		curve_x[i] = std::max(0.0f, LOBE_CX + LOBE_R * cosf(theta));
		curve_y[i] = LOBE_CY + LOBE_R * sinf(theta);
	}

	// Quadratic Bezier from cheek point P0 to bottom point P2=(0.0, 0.95)
	// with control point P1 chosen tangent to the lobe circle for a smooth, rounded belly
	const float p0x = curve_x[ARC_SAMPLES - 1];
	const float p0y = curve_y[ARC_SAMPLES - 1];
	const float p1x = 0.78f;
	const float p1y = 0.58f;
	const float p2x = 0.00f;
	const float p2y = 0.95f;

	constexpr int BEZ_SAMPLES = NUM_SAMPLES - ARC_SAMPLES;
	for (int i = 0; i < BEZ_SAMPLES; ++i) {
		const float t  = static_cast<float>(i + 1) / static_cast<float>(BEZ_SAMPLES);
		const float mt = 1.0f - t;
		curve_x[ARC_SAMPLES + i] = mt * mt * p0x + 2.0f * mt * t * p1x + t * t * p2x;
		curve_y[ARC_SAMPLES + i] = mt * mt * p0y + 2.0f * mt * t * p1y + t * t * p2y;
	}

	// Populate the 64x64 Euclidean distance LUT (8.8 fixed-point)
	for (int vi = 0; vi < 64; ++vi) {
		const float v = SDF_V_MIN + (static_cast<float>(vi) / 63.0f) * (SDF_V_MAX - SDF_V_MIN);
		for (int ui = 0; ui < 64; ++ui) {
			const float u = (static_cast<float>(ui) / 63.0f) * SDF_U_MAX;

			float min_d2 = 100.0f;
			for (int i = 0; i < NUM_SAMPLES; ++i) {
				const float dx = u - curve_x[i];
				const float dy = v - curve_y[i];
				const float d2 = dx * dx + dy * dy;
				if (d2 < min_d2) {
					min_d2 = d2;
				}
			}
			const int32_t dist_fp = static_cast<int32_t>(sqrtf(min_d2) * 256.0f + 0.5f);
			m_sdf[vi][ui] = static_cast<uint16_t>(std::min<int32_t>(dist_fp, 0xFFFF));
		}
	}
}

void Hearts::render()
{
	for (auto &heart : m_hearts)
	{
		heart.move();
		if (heart.m_y + heart.m_r < 0)
		{
			heart.reset(m_matrix.width(), m_matrix.height());
		}
		heart.render(m_matrix, m_sdf);
	}
}
