#include "Cube3D.h"
#include <algorithm>
#include <cmath>

namespace {

struct Vec3 {
	float x;
	float y;
	float z;
};

struct FaceDef {
	uint8_t idx[4]; // 4 vertex indices in counter-clockwise winding order (viewed from outside)
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

// 8 unit cube vertices in [-1, +1]^3
constexpr Vec3 UNIT_VERTS[8] = {
	{ -1.0f, -1.0f, -1.0f }, // 0: left-top-back
	{  1.0f, -1.0f, -1.0f }, // 1: right-top-back
	{  1.0f,  1.0f, -1.0f }, // 2: right-bottom-back
	{ -1.0f,  1.0f, -1.0f }, // 3: left-bottom-back
	{ -1.0f, -1.0f,  1.0f }, // 4: left-top-front
	{  1.0f, -1.0f,  1.0f }, // 5: right-top-front
	{  1.0f,  1.0f,  1.0f }, // 6: right-bottom-front
	{ -1.0f,  1.0f,  1.0f }  // 7: left-bottom-front
};

// 6 faces with distinct base RGB colors (outward normals when camera looks along -Z)
constexpr FaceDef CUBE_FACES[6] = {
	{ { 4, 5, 6, 7 }, 255,  30,  30 }, // Front  (+Z): Red
	{ { 1, 0, 3, 2 },  30, 255,  50 }, // Back   (-Z): Green
	{ { 5, 1, 2, 6 },  30, 100, 255 }, // Right  (+X): Blue
	{ { 0, 4, 7, 3 }, 255, 225,  20 }, // Left   (-X): Yellow
	{ { 0, 1, 5, 4 },  20, 240, 255 }, // Top    (-Y): Cyan
	{ { 7, 6, 2, 3 }, 235,  30, 255 }  // Bottom (+Y): Magenta
};

constexpr float TWO_PI_F = 6.2831853f;

} // namespace

Cube3D::Cube3D(Adafruit_GFX &matrix, float size, bool wireframe_edges) :
	Effect(matrix),
	m_size(size),
	m_wireframe_edges(wireframe_edges),
	m_cx(m_matrix.width()  * 0.5f),
	m_cy(m_matrix.height() * 0.5f)
{
}

void Cube3D::render()
{
	// 1. Advance 3-axis rotation angles
	m_rx += m_drx;
	m_ry += m_dry;
	m_rz += m_drz;
	if (m_rx > TWO_PI_F) m_rx -= TWO_PI_F;
	if (m_ry > TWO_PI_F) m_ry -= TWO_PI_F;
	if (m_rz > TWO_PI_F) m_rz -= TWO_PI_F;

	const float sx = sinf(m_rx), cx = cosf(m_rx);
	const float sy = sinf(m_ry), cy = cosf(m_ry);
	const float sz = sinf(m_rz), cz = cosf(m_rz);

	// 2. Rotate all 8 vertices in 3D and perspective-project relative to cube center (0, 0)
	Vec3  v3d[8];
	float px_rel[8];
	float py_rel[8];

	float min_x =  1000.0f, max_x = -1000.0f;
	float min_y =  1000.0f, max_y = -1000.0f;

	constexpr float CAM_DIST = 48.0f;
	constexpr float FOCAL    = 44.0f;

	for (int i = 0; i < 8; ++i) {
		float x = UNIT_VERTS[i].x * m_size;
		float y = UNIT_VERTS[i].y * m_size;
		float z = UNIT_VERTS[i].z * m_size;

		// Rotate around X axis
		const float y1 = y * cx - z * sx;
		const float z1 = y * sx + z * cx;

		// Rotate around Y axis
		const float x2 =  x * cy + z1 * sy;
		const float z2 = -x * sy + z1 * cy;

		// Rotate around Z axis
		const float x3 = x2 * cz - y1 * sz;
		const float y3 = x2 * sz + y1 * cz;

		v3d[i] = { x3, y3, z2 };

		// Perspective projection (camera at z = +CAM_DIST looking toward -Z)
		const float inv_z = FOCAL / (CAM_DIST - z2);
		const float rx    = x3 * inv_z;
		const float ry    = y3 * inv_z;

		px_rel[i] = rx;
		py_rel[i] = ry;

		if (rx < min_x) min_x = rx;
		if (rx > max_x) max_x = rx;
		if (ry < min_y) min_y = ry;
		if (ry > max_y) max_y = ry;
	}

	// 3. Move cube center and bounce accurately when any rotated corner touches a display wall
	m_cx += m_vx;
	m_cy += m_vy;

	const float max_w = static_cast<float>(m_matrix.width()  - 1);
	const float max_h = static_cast<float>(m_matrix.height() - 1);

	if (m_cx + min_x < 0.0f) {
		m_cx = -min_x;
		m_vx = std::abs(m_vx);
	} else if (m_cx + max_x > max_w) {
		m_cx = max_w - max_x;
		m_vx = -std::abs(m_vx);
	}

	if (m_cy + min_y < 0.0f) {
		m_cy = -min_y;
		m_vy = std::abs(m_vy);
	} else if (m_cy + max_y > max_h) {
		m_cy = max_h - max_y;
		m_vy = -std::abs(m_vy);
	}

	int16_t sx_scr[8];
	int16_t sy_scr[8];
	for (int i = 0; i < 8; ++i) {
		sx_scr[i] = static_cast<int16_t>(lroundf(m_cx + px_rel[i]));
		sy_scr[i] = static_cast<int16_t>(lroundf(m_cy + py_rel[i]));
	}

	// 4. Render visible faces using 2D screen-space backface culling + directional Lambertian shading
	// Directional light vector (normalized, from top-left-front toward origin)
	constexpr float LX = -0.38f;
	constexpr float LY = -0.48f;
	constexpr float LZ =  0.79f;

	for (const auto &face : CUBE_FACES) {
		const uint8_t i0 = face.idx[0];
		const uint8_t i1 = face.idx[1];
		const uint8_t i2 = face.idx[2];
		const uint8_t i3 = face.idx[3];

		// 2D cross product of projected edges (v0->v1) x (v0->v2)
		// In screen coords (+X right, +Y down), front-facing CCW faces have cross2d > 0
		const float dx1 = px_rel[i1] - px_rel[i0];
		const float dy1 = py_rel[i1] - py_rel[i0];
		const float dx2 = px_rel[i2] - px_rel[i0];
		const float dy2 = py_rel[i2] - py_rel[i0];
		const float cross2d = dx1 * dy2 - dy1 * dx2;

		if (cross2d <= 0.0f) {
			continue; // Back-facing face; cull it
		}

		// Compute 3D face normal (v1 - v0) x (v2 - v0) for directional shading
		const float ex1 = v3d[i1].x - v3d[i0].x;
		const float ey1 = v3d[i1].y - v3d[i0].y;
		const float ez1 = v3d[i1].z - v3d[i0].z;

		const float ex2 = v3d[i2].x - v3d[i0].x;
		const float ey2 = v3d[i2].y - v3d[i0].y;
		const float ez2 = v3d[i2].z - v3d[i0].z;

		// Outward normal for our winding order is (e2 x e1)
		float nx = ey2 * ez1 - ez2 * ey1;
		float ny = ez2 * ex1 - ex2 * ez1;
		float nz = ex2 * ey1 - ey2 * ex1;
		const float len = sqrtf(nx * nx + ny * ny + nz * nz);
		if (len > 0.0001f) {
			const float inv_len = 1.0f / len;
			nx *= inv_len;
			ny *= inv_len;
			nz *= inv_len;
		}

		float ndotl = nx * LX + ny * LY + nz * LZ;
		if (ndotl < 0.0f) ndotl = 0.0f;

		// Ambient (0.30) + Diffuse (0.70 * ndotl)
		const float shade = 0.30f + 0.70f * ndotl;
		const uint8_t r = static_cast<uint8_t>(std::min<int>(static_cast<int>(face.r * shade), 255));
		const uint8_t g = static_cast<uint8_t>(std::min<int>(static_cast<int>(face.g * shade), 255));
		const uint8_t b = static_cast<uint8_t>(std::min<int>(static_cast<int>(face.b * shade), 255));
		const uint16_t color = color565(r, g, b);

		// Fill quad as two triangles
		m_matrix.fillTriangle(sx_scr[i0], sy_scr[i0], sx_scr[i1], sy_scr[i1], sx_scr[i2], sy_scr[i2], color);
		m_matrix.fillTriangle(sx_scr[i0], sy_scr[i0], sx_scr[i2], sy_scr[i2], sx_scr[i3], sy_scr[i3], color);

		if (m_wireframe_edges) {
			const uint16_t edge_col = color565(
				static_cast<uint8_t>(std::min<int>(r + 60, 255)),
				static_cast<uint8_t>(std::min<int>(g + 60, 255)),
				static_cast<uint8_t>(std::min<int>(b + 60, 255))
			);
			m_matrix.drawLine(sx_scr[i0], sy_scr[i0], sx_scr[i1], sy_scr[i1], edge_col);
			m_matrix.drawLine(sx_scr[i1], sy_scr[i1], sx_scr[i2], sy_scr[i2], edge_col);
			m_matrix.drawLine(sx_scr[i2], sy_scr[i2], sx_scr[i3], sy_scr[i3], edge_col);
			m_matrix.drawLine(sx_scr[i3], sy_scr[i3], sx_scr[i0], sy_scr[i0], edge_col);
		}
	}
}
