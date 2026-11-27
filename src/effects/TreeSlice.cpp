#include "TreeSlice.h"
#include "FastHub75.h"
#include <algorithm>
#include <cmath>

namespace {

inline uint16_t scaleRgb565(uint16_t c, uint8_t scale)
{
	if (scale >= 255) return c;
	uint32_t r = (((c >> 11) & 0x1F) * scale + 128) >> 8;
	uint32_t g = (((c >> 5) & 0x3F) * scale + 128) >> 8;
	uint32_t b = ((c & 0x1F) * scale + 128) >> 8;
	return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

inline uint32_t hashU32(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

constexpr uint16_t BAUBLE_COLORS[] = {
	color565(255, 35, 45),   // Ruby red
	color565(255, 210, 30),  // Gold
	color565(40, 215, 255),  // Cyan
	color565(255, 65, 220),  // Magenta
	color565(255, 135, 25),  // Orange
	color565(90, 255, 110),  // Emerald
};

constexpr float STAR_HEIGHT_FRAC  = 0.13f;
constexpr float TRUNK_HEIGHT_FRAC = 0.10f;

} // namespace

TreeSlice::TreeSlice(Adafruit_GFX &matrix, float pan_speed, float rot_speed, int16_t tree_height)
	: Effect(matrix),
	  m_pan_speed(pan_speed),
	  m_rot_speed(rot_speed),
	  m_tree_height(std::max<int16_t>(64, tree_height))
{
	initTree();
}

void TreeSlice::initTree()
{
	m_branches.clear();
	m_baubles.clear();

	const float star_h = m_tree_height * STAR_HEIGHT_FRAC;
	const float body_h = m_tree_height * (1.0f - STAR_HEIGHT_FRAC - TRUNK_HEIGHT_FRAC);

	constexpr int whorls = 9;
	constexpr int per_whorl = 7;
	m_branches.reserve(whorls * per_whorl);
	m_vis.reserve(28);

	for (int w = 0; w < whorls; ++w) {
		const float u = 0.04f + 0.90f * (static_cast<float>(w) / static_cast<float>(whorls - 1));
		for (int b = 0; b < per_whorl; ++b) {
			const uint32_t h = hashU32(static_cast<uint32_t>(w * 31 + b * 17 + 7));
			const float jitter_a = ((static_cast<float>(h & 0xFF) / 255.0f) - 0.5f) * 0.35f;
			const float jitter_y = ((static_cast<float>((h >> 8) & 0xFF) / 255.0f) - 0.5f) * (body_h * 0.04f);
			const float theta = (static_cast<float>(b) / static_cast<float>(per_whorl)) * 6.2831853f
			                    + static_cast<float>(w) * 0.52f + jitter_a;

			Branch br;
			br.y_base    = star_h + u * body_h + jitter_y;
			br.droop     = (4.0f + 10.0f * u) * (0.85f + 0.30f * ((h >> 16) & 1));
			br.cos_theta = cosf(theta);
			br.sin_theta = sinf(theta);
			br.len_frac  = 0.86f + 0.22f * (static_cast<float>((h >> 17) & 0xFF) / 255.0f);
			br.tuft_r    = 4.5f + 5.5f * u;
			br.seed      = static_cast<uint8_t>(h >> 24);
			m_branches.push_back(br);
		}
	}

	constexpr int bauble_count = 34;
	m_baubles.reserve(bauble_count);
	for (int i = 0; i < bauble_count; ++i) {
		const uint32_t h1 = hashU32(static_cast<uint32_t>(i) * 37U + 11U);
		const uint32_t h2 = hashU32(static_cast<uint32_t>(i) * 97U + 53U);
		const float u = 0.10f + 0.86f * (static_cast<float>(i) / static_cast<float>(bauble_count - 1));
		const float theta = (static_cast<float>(h1 & 0xFFFF) / 65535.0f) * 6.2831853f;
		Bauble b;
		b.y         = star_h + u * body_h + 3.0f;
		b.cos_theta = cosf(theta);
		b.sin_theta = sinf(theta);
		b.r_frac    = 0.72f + 0.22f * (static_cast<float>((h2 >> 8) & 0xFF) / 255.0f);
		b.radius    = (u > 0.45f) ? 3 : 2;
		b.color     = BAUBLE_COLORS[h2 % (sizeof(BAUBLE_COLORS) / sizeof(BAUBLE_COLORS[0]))];
		m_baubles.push_back(b);
	}
}

float TreeSlice::coneRadiusAt(float tree_y, float body_top, float body_h, float max_r) const
{
	const float u = std::max(0.0f, std::min(1.0f, (tree_y - body_top) / body_h));
	return max_r * (0.09f + 0.91f * u);
}

void TreeSlice::drawStarRow(int fb_y, float local_y, float star_h, int cx, float cos_rot)
{
	const int w = m_matrix.width();
	const float cy = star_h * 0.52f;
	const float dy = local_y - cy;
	const float r_outer = star_h * 0.48f;
	if (fabsf(dy) > r_outer) return;

	const float x_scale = 0.25f + 0.75f * fabsf(cos_rot);
	const float inv_x_scale = 1.0f / x_scale;
	const int half_span = static_cast<int>(r_outer * x_scale) + 1;

	uint16_t *row = m_fb.data() + static_cast<size_t>(fb_y) * w;
	for (int dx = -half_span; dx <= half_span; ++dx) {
		const int px = cx + dx;
		if (px < 0 || px >= w) continue;

		const float sx = static_cast<float>(dx) * inv_x_scale;
		const float dist = sqrtf(sx * sx + dy * dy);
		if (dist > r_outer) continue;

		const float ang = atan2f(dy, sx) + 1.5707963f;
		const float star_wave = 0.5f + 0.5f * cosf(ang * 5.0f);
		const float edge_r = r_outer * (0.40f + 0.60f * star_wave * star_wave);
		if (dist <= edge_r) {
			const float core = 1.0f - (dist / edge_r);
			const uint8_t white_mix = static_cast<uint8_t>(core * 180.0f);
			row[px] = color565(255, std::min(255, 210 + (white_mix >> 2)), std::min(255, 40 + white_mix));
		}
	}
}

void TreeSlice::drawTrunkRow(int fb_y, float trunk_r, int cx)
{
	const int w = m_matrix.width();
	const int half_w = static_cast<int>(trunk_r);
	const float inv_r = 1.0f / trunk_r;
	uint16_t *row = m_fb.data() + static_cast<size_t>(fb_y) * w;
	for (int dx = -half_w; dx <= half_w; ++dx) {
		const int px = cx + dx;
		if (px < 0 || px >= w) continue;
		const float nx = static_cast<float>(dx) * inv_r;
		const float nz = sqrtf(std::max(0.0f, 1.0f - nx * nx));
		const uint8_t shade = static_cast<uint8_t>(70.0f + 185.0f * nz);
		row[px] = scaleRgb565(color565(135, 72, 28), shade);
	}
}

void TreeSlice::drawBranch(const VisibleBranch &vb, float pan_y, float body_top, float body_h, float max_r, int cx, int w, int h)
{
	const Branch &br = *vb.br;
	const float reach = coneRadiusAt(br.y_base, body_top, body_h, max_r) * br.len_frac;
	const float dx_tip = reach * vb.sin_p;
	const float y0 = br.y_base - pan_y;

	const float depth_lit = 0.35f + 0.65f * ((vb.depth + 1.0f) * 0.5f);
	const int pan_yi = static_cast<int>(pan_y);

	// Well-spaced tufts (~6..11 per branch) minimize redundant pixel overdraw
	const int steps = std::max(5, static_cast<int>((fabsf(dx_tip) + br.droop) / (br.tuft_r * 0.9f)));
	const float inv_steps = 1.0f / static_cast<float>(steps);
	const float rx_scale = 0.75f + 0.55f * fabsf(vb.sin_p);

	for (int s = 0; s <= steps; ++s) {
		const float t = static_cast<float>(s) * inv_steps;
		const float by = y0 + br.droop * (t * t);
		const float puff = 4.0f * t * (1.0f - t); // fast parabolic swell in [0..1]
		const float r_y = std::max(1.5f, br.tuft_r * (0.35f + 0.65f * puff));

		const int sy0 = std::max(0, static_cast<int>(by - r_y));
		const int sy1 = std::min(h - 1, static_cast<int>(by + r_y));
		if (sy0 > sy1) continue;

		const float bx = static_cast<float>(cx) + dx_tip * t;
		const float r_x = r_y * rx_scale;
		const int sx0 = std::max(0, static_cast<int>(bx - r_x));
		const int sx1 = std::min(w - 1, static_cast<int>(bx + r_x));
		if (sx0 > sx1) continue;

		const float inv_ry = 1.0f / r_y;
		const float inv_rx = 1.0f / r_x;
		const float tip_lit = depth_lit * (0.85f + 0.25f * t);
		const uint8_t tip_blue = (t > 0.75f) ? 10 : 0;

		for (int py = sy0; py <= sy1; ++py) {
			const float dy = (static_cast<float>(py) - by) * inv_ry;
			const float dy2 = dy * dy;
			const float row_lit = tip_lit * (0.55f + 0.45f * (0.5f - 0.5f * dy));
			const uint32_t row_seed = (static_cast<uint32_t>(py + pan_yi) * 83U) ^ static_cast<uint32_t>(br.seed);
			uint16_t *row = m_fb.data() + static_cast<size_t>(py) * w;

			for (int px = sx0; px <= sx1; ++px) {
				const float dx = (static_cast<float>(px) - bx) * inv_rx;
				const float dist2 = dx * dx + dy2;
				if (dist2 > 1.16f) continue;

				const uint32_t nh = hashU32((static_cast<uint32_t>(px) * 193U) ^ row_seed);
				if (dist2 > 0.68f) {
					const float spike = 0.68f + 0.48f * (static_cast<float>(nh & 0xFF) * (1.0f / 255.0f));
					if (dist2 > spike) continue;
				}

				const float needle_var = 0.72f + 0.28f * (static_cast<float>((nh >> 8) & 0xFF) * (1.0f / 255.0f));
				const uint8_t g = static_cast<uint8_t>(std::min(255.0f, 35.0f + 215.0f * row_lit * needle_var));
				row[px] = color565(static_cast<uint8_t>(g >> 3), g, static_cast<uint8_t>((g >> 2) + tip_blue));
			}
		}
	}
}

void TreeSlice::drawBauble(int bx, int by, int r, uint16_t base_col, int w, int h)
{
	const float r2 = static_cast<float>(r * r) + 0.4f;
	const float inv_r2 = 1.0f / r2;
	for (int dy = -r; dy <= r; ++dy) {
		const int py = by + dy;
		if (py < 0 || py >= h) continue;
		uint16_t *row = m_fb.data() + static_cast<size_t>(py) * w;
		for (int dx = -r; dx <= r; ++dx) {
			const int px = bx + dx;
			if (px < 0 || px >= w) continue;
			const float dist2 = static_cast<float>(dx * dx + dy * dy);
			if (dist2 > r2) continue;

			if (dx == -r / 2 && dy == -r / 2) {
				row[px] = color565(255, 255, 255);
			} else {
				const float nz = sqrtf(std::max(0.0f, 1.0f - dist2 * inv_r2));
				const uint8_t shade = static_cast<uint8_t>(110.0f + 145.0f * nz);
				row[px] = scaleRgb565(base_col, shade);
			}
		}
	}
}

void TreeSlice::render()
{
	const int w = m_matrix.width();
	const int h = m_matrix.height();
	if (w <= 0 || h <= 0) return;

	if (static_cast<int>(m_fb.size()) != w * h) {
		m_fb.resize(static_cast<size_t>(w) * h);
	}
	std::fill(m_fb.begin(), m_fb.end(), color565(0, 2, 10));

	m_pan_phase += m_pan_speed;
	m_rot_angle += m_rot_speed;
	if (m_rot_angle > 6.2831853f) m_rot_angle -= 6.2831853f;

	const float cos_rot = cosf(m_rot_angle);
	const float sin_rot = sinf(m_rot_angle);

	const float max_pan = static_cast<float>(std::max<int>(0, m_tree_height - h));
	const float pan_t = 0.5f - 0.5f * cosf(m_pan_phase);
	const float pan_y = pan_t * max_pan;

	const int   cx       = w / 2;
	const float max_r    = static_cast<float>(w) * 0.44f;
	const float star_h   = m_tree_height * STAR_HEIGHT_FRAC;
	const float trunk_h  = m_tree_height * TRUNK_HEIGHT_FRAC;
	const float body_top = star_h;
	const float body_h   = static_cast<float>(m_tree_height) - star_h - trunk_h;
	const float trunk_y  = body_top + body_h;
	const float trunk_r  = std::max(5.0f, max_r * 0.11f);

	// 1. Central trunk
	for (int y = 0; y < h; ++y) {
		const float ty = pan_y + static_cast<float>(y);
		if (ty >= star_h + 6.0f && ty < static_cast<float>(m_tree_height)) {
			const float tr = (ty >= trunk_y) ? trunk_r : (trunk_r * 0.55f);
			drawTrunkRow(y, tr, cx);
		}
	}

	// 2. Visible branches sorted back-to-front (reuse m_vis buffer; no trig per branch)
	m_vis.clear();
	for (const Branch &br : m_branches) {
		const float y_min = br.y_base - br.tuft_r;
		const float y_max = br.y_base + br.droop + br.tuft_r;
		if (y_max < pan_y || y_min >= pan_y + static_cast<float>(h)) continue;
		const float depth = br.cos_theta * cos_rot - br.sin_theta * sin_rot;
		const float sin_p = br.sin_theta * cos_rot + br.cos_theta * sin_rot;
		m_vis.push_back({ &br, depth, sin_p });
	}
	std::sort(m_vis.begin(), m_vis.end(), [](const VisibleBranch &a, const VisibleBranch &b) {
		return a.depth < b.depth;
	});

	for (const VisibleBranch &vb : m_vis) {
		drawBranch(vb, pan_y, body_top, body_h, max_r, cx, w, h);
	}

	// 3. Star on top
	for (int y = 0; y < h; ++y) {
		const float ty = pan_y + static_cast<float>(y);
		if (ty >= 0.0f && ty < star_h) {
			drawStarRow(y, ty, star_h, cx, cos_rot);
		}
	}

	// 4. Front-facing baubles (zero trig per bauble)
	for (const Bauble &b : m_baubles) {
		const float rel_y = b.y - pan_y;
		if (rel_y < -b.radius || rel_y >= static_cast<float>(h + b.radius)) continue;

		const float depth = b.cos_theta * cos_rot - b.sin_theta * sin_rot;
		if (depth <= 0.12f) continue;

		const float sin_p = b.sin_theta * cos_rot + b.cos_theta * sin_rot;
		const float r_at_y = coneRadiusAt(b.y, body_top, body_h, max_r);
		const int bx = cx + static_cast<int>(r_at_y * b.r_frac * sin_p);
		const int by = static_cast<int>(rel_y);
		drawBauble(bx, by, b.radius, b.color, w, h);
	}

	auto *hub75 = static_cast<MatrixPanel_I2S_DMA *>(&m_matrix);
	FastHub75::drawRGBBitmap(*hub75, 0, 0, m_fb.data(), nullptr, w, h, false);
}
