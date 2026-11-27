#include "Raycaster.h"
#include "FastHub75.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int MAP_W = 16;
constexpr int MAP_H = 16;
constexpr int TEX_SIZE = 16;

// 0 = empty, 1 = brick, 2 = stone, 3 = tech blue, 4 = gold/wood
constexpr uint8_t MAZE_MAP[MAP_H][MAP_W] = {
	{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
	{1,0,0,0,0,2,0,0,0,0,0,3,0,0,0,1},
	{1,0,2,2,0,2,0,3,5,3,0,3,0,4,0,1},
	{1,0,2,0,0,0,0,0,0,3,0,0,0,4,0,1},
	{1,0,0,0,4,4,0,2,0,0,0,2,2,4,0,1},
	{1,1,1,0,4,0,0,2,0,1,0,0,0,0,0,1},
	{1,0,0,0,0,0,3,2,0,1,1,1,0,3,0,1},
	{1,0,3,3,0,0,3,0,0,0,0,1,0,3,0,1},
	{1,0,3,0,0,2,0,0,4,4,0,0,0,0,0,1},
	{1,0,0,0,2,2,0,0,5,0,0,2,0,1,0,1},
	{1,2,0,0,0,0,0,1,0,0,3,2,0,1,0,1},
	{1,0,0,4,4,0,0,1,0,0,3,0,0,0,0,1},
	{1,0,4,4,0,0,2,1,0,2,0,0,4,4,0,1},
	{1,0,0,0,0,2,2,0,0,2,0,0,4,0,0,1},
	{1,0,1,0,0,0,0,0,0,0,0,3,0,0,0,1},
	{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Procedural 16x16 RGB565 texture texel (pure saturated colors + true black seams for LED contrast)
inline uint16_t sampleWallTexel(uint8_t wall_type, int tx, int ty)
{
	switch (wall_type) {
		case 1: { // Pure red brick with 16x16 staggered black mortar
			const bool mortar = ((ty & 3) == 0) ||
			                    (((ty & 4) == 0) ? (tx == 0 || tx == 8) : (tx == 4 || tx == 12));
			return mortar ? color565(0, 0, 0) : color565(255, 0, 0);
		}
		case 2: { // Crisp 16x16 silver/white stone blocks with black seams
			const bool seam = (tx == 0 || ty == 0 || (ty == 8) ||
			                   ((ty < 8) ? (tx == 9) : (tx == 5 || tx == 13)));
			return seam ? color565(0, 0, 0) : color565(210, 220, 255);
		}
		case 3: { // Wolfenstein 16x16 blue tech panel with black frame, vent & cyan rivets
			if (tx == 0 || tx == 15 || ty == 0 || ty == 15) return color565(0, 0, 0);
			if ((tx == 3 || tx == 12) && (ty == 3 || ty == 12)) return color565(0, 255, 255);
			if ((ty == 7 || ty == 8) && tx >= 4 && tx <= 11) return color565(0, 0, 0);
			return color565(0, 70, 255);
		}
		case 4: { // 16x16 Gold / amber panel with black frame & gold cross
			if (tx == 0 || tx == 15 || ty == 0 || ty == 15) return color565(0, 0, 0);
			if (tx == 7 || tx == 8 || ty == 7 || ty == 8) return color565(255, 220, 0);
			return color565(255, 110, 0);
		}
		default:{ // 16x16 
			if (tx == 0 || tx == 15 || ty == 0 || ty == 15) return color565(0, 0, 0);
			int r = (7-tx)*(7-tx)+(7-ty)*(7-ty);
			if ( r < 17) return color565(220, 0, 220);
			if ( r < 19) return color565(0, 0, 0);
			return color565(50, 50, 200);
		}
	}
}

inline uint16_t shadeRgb565(uint16_t c, uint8_t scale)
{
	if (scale == 255) return c;
	uint32_t r = (((c >> 11) & 0x1F) * scale + 128) >> 8;
	uint32_t g = (((c >> 5) & 0x3F) * scale + 128) >> 8;
	uint32_t b = ((c & 0x1F) * scale + 128) >> 8;
	return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

inline bool isBlocked(float x, float y)
{
	constexpr float margin = 0.24f;
	const int x0 = static_cast<int>(x - margin);
	const int x1 = static_cast<int>(x + margin);
	const int y0 = static_cast<int>(y - margin);
	const int y1 = static_cast<int>(y + margin);
	if (x0 < 0 || y0 < 0 || x1 >= MAP_W || y1 >= MAP_H) return true;
	return MAZE_MAP[y0][x0] || MAZE_MAP[y0][x1] || MAZE_MAP[y1][x0] || MAZE_MAP[y1][x1];
}

} // namespace

Raycaster::Raycaster(Adafruit_GFX &matrix, float move_speed)
	: Effect(matrix), m_move_speed(move_speed)
{
}

void Raycaster::rotatePlayer(float angle)
{
	const float cs = cosf(angle);
	const float sn = sinf(angle);
	const float old_dx = m_dir_x;
	m_dir_x = old_dx * cs - m_dir_y * sn;
	m_dir_y = old_dx * sn + m_dir_y * cs;
	const float old_px = m_plane_x;
	m_plane_x = old_px * cs - m_plane_y * sn;
	m_plane_y = old_px * sn + m_plane_y * cs;
}

float Raycaster::castDistance(float dir_x, float dir_y) const
{
	int map_x = static_cast<int>(m_pos_x);
	int map_y = static_cast<int>(m_pos_y);

	const float delta_x = (dir_x == 0.0f) ? 1e6f : fabsf(1.0f / dir_x);
	const float delta_y = (dir_y == 0.0f) ? 1e6f : fabsf(1.0f / dir_y);

	int step_x, step_y;
	float side_x, side_y;

	if (dir_x < 0.0f) {
		step_x = -1;
		side_x = (m_pos_x - map_x) * delta_x;
	} else {
		step_x = 1;
		side_x = (map_x + 1.0f - m_pos_x) * delta_x;
	}

	if (dir_y < 0.0f) {
		step_y = -1;
		side_y = (m_pos_y - map_y) * delta_y;
	} else {
		step_y = 1;
		side_y = (map_y + 1.0f - m_pos_y) * delta_y;
	}

	for (int step = 0; step < 24; ++step) {
		bool hit_y = false;
		if (side_x < side_y) {
			side_x += delta_x;
			map_x += step_x;
		} else {
			side_y += delta_y;
			map_y += step_y;
			hit_y = true;
		}
		if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) {
			return 0.5f;
		}
		if (MAZE_MAP[map_y][map_x] != 0) {
			return hit_y ? (side_y - delta_y) : (side_x - delta_x);
		}
	}
	return 12.0f;
}

void Raycaster::updatePlayer()
{
	if (++m_bias_timer > 180) {
		m_bias_timer = 0;
		m_turn_bias = -m_turn_bias;
	}

	// Feeler rays at 0 deg and +/-35 deg for smooth autonomous steering
	constexpr float cs35 = 0.81915f;
	constexpr float sn35 = 0.57358f;
	const float lx = m_dir_x * cs35 - m_dir_y * sn35;
	const float ly = m_dir_x * sn35 + m_dir_y * cs35;
	const float rx = m_dir_x * cs35 + m_dir_y * sn35;
	const float ry = -m_dir_x * sn35 + m_dir_y * cs35;

	const float dist_c = castDistance(m_dir_x, m_dir_y);
	const float dist_l = castDistance(lx, ly);
	const float dist_r = castDistance(rx, ry);

	float target_turn = 0.0f;
	if (dist_c < 1.55f) {
		// Turn away from obstacle toward the more open side
		const float diff = dist_r - dist_l;
		if (fabsf(diff) < 0.25f) {
			target_turn = 0.055f * m_turn_bias;
		} else {
			target_turn = (diff > 0.0f) ? -0.055f : 0.055f;
		}
	} else {
		// Gentle corridor centering
		const float balance = (dist_l - dist_r) * 0.012f;
		target_turn = fmaxf(-0.025f, fminf(0.025f, balance));
	}

	m_turn_vel = m_turn_vel * 0.82f + target_turn * 0.18f;
	rotatePlayer(m_turn_vel);

	// Slow down slightly in tight turns, slide smoothly along walls
	const float speed = m_move_speed * (dist_c < 0.85f ? 0.55f : 1.0f);
	const float nx = m_pos_x + m_dir_x * speed;
	const float ny = m_pos_y + m_dir_y * speed;

	if (!isBlocked(nx, m_pos_y)) m_pos_x = nx;
	if (!isBlocked(m_pos_x, ny)) m_pos_y = ny;

	m_walk_phase += 0.22f;
}

void Raycaster::render()
{
	const int w = m_matrix.width();
	const int h = m_matrix.height();
	if (w <= 0 || h <= 0) return;

	if (static_cast<int>(m_fb.size()) != w * h) {
		m_fb.resize(static_cast<size_t>(w) * h);
	}

	updatePlayer();

	const int bob_y = static_cast<int>(sinf(m_walk_phase) * 0.6f);
	const int horizon = (h / 2) + bob_y;

	// Fill very dark ceiling and floor gradients so walls pop with high contrast
	for (int y = 0; y < h; ++y) {
		uint16_t row_col = 0;
		if (y < horizon) {
			const int d = horizon - y;
			if (d > 4) {
				const uint8_t v = static_cast<uint8_t>(std::min(28, (d - 4) * 3));
				row_col = color565(0, 0, v);
			}
		} else {
			const int d = y - horizon;
			if (d > 4) {
				const uint8_t v = static_cast<uint8_t>(std::min(32, (d - 4) * 3));
				row_col = color565(v, 0, 0);
			}
		}
		uint16_t *row_ptr = m_fb.data() + static_cast<size_t>(y) * w;
		for (int x = 0; x < w; ++x) {
			row_ptr[x] = row_col;
		}
	}

	// Adjust projection plane width to match display aspect ratio without fisheye
	const float aspect_scale = (static_cast<float>(w) / static_cast<float>(h)) * 0.22f;
	const float plane_x = m_plane_x * aspect_scale;
	const float plane_y = m_plane_y * aspect_scale;
	const float inv_w = 2.0f / static_cast<float>(w);

	for (int x = 0; x < w; ++x) {
		const float camera_x = static_cast<float>(x) * inv_w - 1.0f;
		const float ray_dx = m_dir_x + plane_x * camera_x;
		const float ray_dy = m_dir_y + plane_y * camera_x;

		int map_x = static_cast<int>(m_pos_x);
		int map_y = static_cast<int>(m_pos_y);

		const float delta_x = (ray_dx == 0.0f) ? 1e6f : fabsf(1.0f / ray_dx);
		const float delta_y = (ray_dy == 0.0f) ? 1e6f : fabsf(1.0f / ray_dy);

		int step_x, step_y;
		float side_x, side_y;

		if (ray_dx < 0.0f) {
			step_x = -1;
			side_x = (m_pos_x - map_x) * delta_x;
		} else {
			step_x = 1;
			side_x = (map_x + 1.0f - m_pos_x) * delta_x;
		}

		if (ray_dy < 0.0f) {
			step_y = -1;
			side_y = (m_pos_y - map_y) * delta_y;
		} else {
			step_y = 1;
			side_y = (map_y + 1.0f - m_pos_y) * delta_y;
		}

		uint8_t wall_type = 0;
		bool side = false;
		for (int step = 0; step < 32; ++step) {
			if (side_x < side_y) {
				side_x += delta_x;
				map_x += step_x;
				side = false;
			} else {
				side_y += delta_y;
				map_y += step_y;
				side = true;
			}
			if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) {
				wall_type = 1;
				break;
			}
			wall_type = MAZE_MAP[map_y][map_x];
			if (wall_type != 0) break;
		}

		float perp_dist = side ? (side_y - delta_y) : (side_x - delta_x);
		if (perp_dist < 0.1f) perp_dist = 0.1f;

		const int line_h = static_cast<int>(static_cast<float>(h) * 1.25f / perp_dist);
		int draw_start = horizon - line_h / 2;
		int draw_end = horizon + line_h / 2;
		if (draw_start < 0) draw_start = 0;
		if (draw_end >= h) draw_end = h - 1;

		// Horizontal texture coordinate
		float wall_x = side ? (m_pos_x + perp_dist * ray_dx) : (m_pos_y + perp_dist * ray_dy);
		wall_x -= floorf(wall_x);
		int tx = static_cast<int>(wall_x * TEX_SIZE) & (TEX_SIZE - 1);
		if ((!side && ray_dx > 0.0f) || (side && ray_dy < 0.0f)) {
			tx = (TEX_SIZE - 1) - tx;
		}

		// Distance + side shading (fade far corridors toward black)
		int shade = (perp_dist <= 1.8f)
			? 255
			: static_cast<int>(255.0f / (1.0f + (perp_dist - 1.8f) * 0.42f));
		if (side) shade = (shade * 145) >> 8;
		if (shade < 16) shade = 16;
		const uint8_t shade_u8 = static_cast<uint8_t>(shade);

		const float tex_step = static_cast<float>(TEX_SIZE) / static_cast<float>(line_h);
		float tex_pos = static_cast<float>(draw_start - (horizon - line_h / 2)) * tex_step;

		uint16_t *col_ptr = m_fb.data() + static_cast<size_t>(draw_start) * w + x;
		for (int y = draw_start; y <= draw_end; ++y) {
			const int ty = static_cast<int>(tex_pos) & (TEX_SIZE - 1);
			tex_pos += tex_step;
			*col_ptr = shadeRgb565(sampleWallTexel(wall_type, tx, ty), shade_u8);
			col_ptr += w;
		}
	}

	auto *hub75 = static_cast<MatrixPanel_I2S_DMA *>(&m_matrix);
	FastHub75::drawRGBBitmap(*hub75, 0, 0, m_fb.data(), nullptr, w, h, false);
}
