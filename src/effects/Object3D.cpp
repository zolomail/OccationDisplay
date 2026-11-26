#include "Object3D.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct Vec3 {
	float x;
	float y;
	float z;
};

struct TriDef {
	uint8_t idx[3]; // Counter-clockwise winding when viewed from outside
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct MeshData {
	std::vector<Vec3>   verts;
	std::vector<TriDef> tris;
};

// Palette helper for multi-face meshes
void hslToRgb(float h_deg, float s, float l, uint8_t &r, uint8_t &g, uint8_t &b)
{
	const float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
	const float hp = fmodf(h_deg, 360.0f) / 60.0f;
	const float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
	float rf = 0, gf = 0, bf = 0;
	if      (hp < 1.0f) { rf = c; gf = x; }
	else if (hp < 2.0f) { rf = x; gf = c; }
	else if (hp < 3.0f) { gf = c; bf = x; }
	else if (hp < 4.0f) { gf = x; bf = c; }
	else if (hp < 5.0f) { rf = x; bf = c; }
	else                { rf = c; bf = x; }
	const float m = l - 0.5f * c;
	r = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>((rf + m) * 255.0f))));
	g = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>((gf + m) * 255.0f))));
	b = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>((bf + m) * 255.0f))));
}

void addQuad(std::vector<TriDef> &tris, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t r, uint8_t g, uint8_t bl)
{
	tris.push_back({ { a, b, c }, r, g, bl });
	tris.push_back({ { a, c, d }, r, g, bl });
}

MeshData buildMesh(MeshShape shape)
{
	MeshData m;
	switch (shape) {
		case MeshShape::Pyramid: {
			m.verts = {
				{  0.0f, -1.0f,  0.0f }, // 0: apex (top)
				{ -1.0f,  0.8f,  1.0f }, // 1: front-left
				{  1.0f,  0.8f,  1.0f }, // 2: front-right
				{  1.0f,  0.8f, -1.0f }, // 3: back-right
				{ -1.0f,  0.8f, -1.0f }, // 4: back-left
			};
			m.tris = {
				{ { 0, 1, 2 }, 255,  50,  40 }, // Front: Red
				{ { 0, 2, 3 },  40, 140, 255 }, // Right: Blue
				{ { 0, 3, 4 },  40, 240,  70 }, // Back:  Green
				{ { 0, 4, 1 }, 255, 220,  30 }, // Left:  Yellow
				{ { 1, 4, 3 }, 200,  40, 255 }, // Base 1: Magenta
				{ { 1, 3, 2 }, 200,  40, 255 }, // Base 2: Magenta
			};
			break;
		}
		case MeshShape::Octahedron: {
			m.verts = {
				{  0.0f, -1.1f,  0.0f }, // 0: top
				{  0.0f,  1.1f,  0.0f }, // 1: bottom
				{  0.0f,  0.0f,  1.1f }, // 2: front
				{  1.1f,  0.0f,  0.0f }, // 3: right
				{  0.0f,  0.0f, -1.1f }, // 4: back
				{ -1.1f,  0.0f,  0.0f }, // 5: left
			};
			m.tris = {
				{ { 0, 2, 3 },  30, 230, 255 },
				{ { 0, 3, 4 },  40, 120, 255 },
				{ { 0, 4, 5 }, 160,  60, 255 },
				{ { 0, 5, 2 }, 255,  60, 180 },
				{ { 1, 3, 2 }, 255,  80,  40 },
				{ { 1, 4, 3 }, 255, 190,  30 },
				{ { 1, 5, 4 },  60, 240,  90 },
				{ { 1, 2, 5 },  30, 220, 180 },
			};
			break;
		}
		case MeshShape::Diamond: {
			// Bipyramid with 6-sided equator, elongated vertically
			m.verts.push_back({ 0.0f, -1.25f, 0.0f }); // 0: top tip
			m.verts.push_back({ 0.0f,  1.25f, 0.0f }); // 1: bottom tip
			constexpr int N = 6;
			for (int i = 0; i < N; ++i) {
				const float a = (6.2831853f * i) / N;
				m.verts.push_back({ 0.9f * cosf(a), -0.15f, 0.9f * sinf(a) });
			}
			for (int i = 0; i < N; ++i) {
				const uint8_t cur = static_cast<uint8_t>(2 + i);
				const uint8_t nxt = static_cast<uint8_t>(2 + ((i + 1) % N));
				uint8_t r, g, b;
				hslToRgb(185.0f + i * 25.0f, 0.90f, 0.58f, r, g, b);
				m.tris.push_back({ { 0, nxt, cur }, r, g, b });
				hslToRgb(195.0f + i * 25.0f, 0.85f, 0.45f, r, g, b);
				m.tris.push_back({ { 1, cur, nxt }, r, g, b });
			}
			break;
		}
		case MeshShape::Icosahedron: {
			const float phi = (1.0f + sqrtf(5.0f)) * 0.5f;
			const float inv = 1.1f / sqrtf(1.0f + phi * phi);
			const float a = inv;
			const float b = phi * inv;
			m.verts = {
				{-a,  b,  0}, { a,  b,  0}, {-a, -b,  0}, { a, -b,  0},
				{ 0, -a,  b}, { 0,  a,  b}, { 0, -a, -b}, { 0,  a, -b},
				{ b,  0, -a}, { b,  0,  a}, {-b,  0, -a}, {-b,  0,  a}
			};
			// Outward-facing CCW triangles (viewed from outside in +Y-down screen coords)
			const uint8_t faces[20][3] = {
				{0, 5,11}, {0, 1, 5}, {0, 7, 1}, {0,10, 7}, {0,11,10},
				{1, 9, 5}, {5, 4,11}, {11,2,10}, {10,6, 7}, {7, 8, 1},
				{3, 4, 9}, {3, 2, 4}, {3, 6, 2}, {3, 8, 6}, {3, 9, 8},
				{4, 5, 9}, {2,11, 4}, {6,10, 2}, {8, 7, 6}, {9, 1, 8}
			};
			for (int i = 0; i < 20; ++i) {
				uint8_t r, g, bl;
				hslToRgb(i * 18.0f, 0.90f, 0.52f, r, g, bl);
				m.tris.push_back({ { faces[i][0], faces[i][1], faces[i][2] }, r, g, bl });
			}
			break;
		}
		case MeshShape::Torus: {
			constexpr int MAJ = 10;
			constexpr int MIN = 6;
			constexpr float R = 0.78f;
			constexpr float r = 0.34f;
			for (int i = 0; i < MAJ; ++i) {
				const float u = (6.2831853f * i) / MAJ;
				const float cu = cosf(u), su = sinf(u);
				for (int j = 0; j < MIN; ++j) {
					const float v = (6.2831853f * j) / MIN;
					const float cv = cosf(v), sv = sinf(v);
					const float d = R + r * cv;
					m.verts.push_back({ d * cu, d * su, r * sv });
				}
			}
			for (int i = 0; i < MAJ; ++i) {
				const int ni = (i + 1) % MAJ;
				uint8_t cr, cg, cb;
				hslToRgb((i * 360.0f) / MAJ, 0.92f, 0.52f, cr, cg, cb);
				for (int j = 0; j < MIN; ++j) {
					const int nj = (j + 1) % MIN;
					const uint8_t a = static_cast<uint8_t>(i  * MIN + j);
					const uint8_t b = static_cast<uint8_t>(ni * MIN + j);
					const uint8_t c = static_cast<uint8_t>(ni * MIN + nj);
					const uint8_t d = static_cast<uint8_t>(i  * MIN + nj);
					addQuad(m.tris, a, b, c, d, cr, cg, cb);
				}
			}
			break;
		}
		case MeshShape::Cube:
		default: {
			m.verts = {
				{ -1.0f, -1.0f, -1.0f }, // 0: left-top-back
				{  1.0f, -1.0f, -1.0f }, // 1: right-top-back
				{  1.0f,  1.0f, -1.0f }, // 2: right-bottom-back
				{ -1.0f,  1.0f, -1.0f }, // 3: left-bottom-back
				{ -1.0f, -1.0f,  1.0f }, // 4: left-top-front
				{  1.0f, -1.0f,  1.0f }, // 5: right-top-front
				{  1.0f,  1.0f,  1.0f }, // 6: right-bottom-front
				{ -1.0f,  1.0f,  1.0f }  // 7: left-bottom-front
			};
			addQuad(m.tris, 4, 5, 6, 7, 255,  30,  30); // Front  (+Z): Red
			addQuad(m.tris, 1, 0, 3, 2,  30, 255,  50); // Back   (-Z): Green
			addQuad(m.tris, 5, 1, 2, 6,  30, 100, 255); // Right  (+X): Blue
			addQuad(m.tris, 0, 4, 7, 3, 255, 225,  20); // Left   (-X): Yellow
			addQuad(m.tris, 0, 1, 5, 4,  20, 240, 255); // Top    (-Y): Cyan
			addQuad(m.tris, 7, 6, 2, 3, 235,  30, 255); // Bottom (+Y): Magenta
			break;
		}
	}
	return m;
}

constexpr float TWO_PI_F = 6.2831853f;
constexpr float CAM_DIST = 64.0f;
constexpr float FOCAL    = 64.0f; // At z = 0 (front viewing plane), 1 world unit == 1 pixel
constexpr float Z_NEAR   = 12.0f;
constexpr float Z_FAR    = -48.0f;

} // namespace

Object3D::Object3D(
	Adafruit_GFX &matrix,
	float size,
	bool wireframe_edges,
	MeshShape shape,
	bool show_box
) :
	Effect(matrix),
	m_size(size),
	m_wireframe_edges(wireframe_edges),
	m_shape(shape),
	m_show_box(show_box)
{
}

void Object3D::render()
{
	const float scr_w  = static_cast<float>(m_matrix.width());
	const float scr_h  = static_cast<float>(m_matrix.height());
	const float half_w = scr_w * 0.5f;
	const float half_h = scr_h * 0.5f;

	// 1. Draw the 3D perspective wireframe box going inward into the display
	if (m_show_box) {
		const float back_scale = FOCAL / (CAM_DIST - Z_FAR);
		const int16_t bx0 = static_cast<int16_t>(lroundf(half_w - half_w * back_scale));
		const int16_t by0 = static_cast<int16_t>(lroundf(half_h - half_h * back_scale));
		const int16_t bx1 = static_cast<int16_t>(lroundf(half_w + (half_w - 1.0f) * back_scale));
		const int16_t by1 = static_cast<int16_t>(lroundf(half_h + (half_h - 1.0f) * back_scale));

		const int16_t fx0 = 0, fy0 = 0;
		const int16_t fx1 = m_matrix.width()  - 1;
		const int16_t fy1 = m_matrix.height() - 1;

		const uint16_t box_back_col = color565(35, 55, 85);
		const uint16_t box_edge_col = color565(50, 80, 120);

		// Back wall rectangle
		m_matrix.drawRect(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1, box_back_col);
		// 4 receding corner edges from viewing plane to back wall
		m_matrix.drawLine(fx0, fy0, bx0, by0, box_edge_col);
		m_matrix.drawLine(fx1, fy0, bx1, by0, box_edge_col);
		m_matrix.drawLine(fx1, fy1, bx1, by1, box_edge_col);
		m_matrix.drawLine(fx0, fy1, bx0, by1, box_edge_col);
	}

	// 2. Advance 3-axis rotation angles
	m_rx += m_drx;
	m_ry += m_dry;
	m_rz += m_drz;
	if (m_rx > TWO_PI_F) m_rx -= TWO_PI_F;
	if (m_ry > TWO_PI_F) m_ry -= TWO_PI_F;
	if (m_rz > TWO_PI_F) m_rz -= TWO_PI_F;

	const float sx = sinf(m_rx), cx = cosf(m_rx);
	const float sy = sinf(m_ry), cy = cosf(m_ry);
	const float sz = sinf(m_rz), cz = cosf(m_rz);

	const MeshData mesh = buildMesh(m_shape);
	const size_t nv = mesh.verts.size();

	std::vector<Vec3> v3d(nv);
	float min_x =  1000.0f, max_x = -1000.0f;
	float min_y =  1000.0f, max_y = -1000.0f;
	float min_z =  1000.0f, max_z = -1000.0f;

	for (size_t i = 0; i < nv; ++i) {
		const float x = mesh.verts[i].x * m_size;
		const float y = mesh.verts[i].y * m_size;
		const float z = mesh.verts[i].z * m_size;

		// Rotate X
		const float y1 = y * cx - z * sx;
		const float z1 = y * sx + z * cx;
		// Rotate Y
		const float x2 =  x * cy + z1 * sy;
		const float z2 = -x * sy + z1 * cy;
		// Rotate Z
		const float x3 = x2 * cz - y1 * sz;
		const float y3 = x2 * sz + y1 * cz;

		v3d[i] = { x3, y3, z2 };
		if (x3 < min_x) min_x = x3;
		if (x3 > max_x) max_x = x3;
		if (y3 < min_y) min_y = y3;
		if (y3 > max_y) max_y = y3;
		if (z2 < min_z) min_z = z2;
		if (z2 > max_z) max_z = z2;
	}

	// 3. Move in 3D and bounce against all 6 inner walls of the box
	m_cx += m_vx;
	m_cy += m_vy;
	m_cz += m_vz;

	if (m_cx + min_x < -half_w) {
		m_cx = -half_w - min_x;
		m_vx = std::abs(m_vx);
	} else if (m_cx + max_x > half_w - 1.0f) {
		m_cx = (half_w - 1.0f) - max_x;
		m_vx = -std::abs(m_vx);
	}

	if (m_cy + min_y < -half_h) {
		m_cy = -half_h - min_y;
		m_vy = std::abs(m_vy);
	} else if (m_cy + max_y > half_h - 1.0f) {
		m_cy = (half_h - 1.0f) - max_y;
		m_vy = -std::abs(m_vy);
	}

	if (m_cz + min_z < Z_FAR) {
		m_cz = Z_FAR - min_z;
		m_vz = std::abs(m_vz);
	} else if (m_cz + max_z > Z_NEAR) {
		m_cz = Z_NEAR - max_z;
		m_vz = -std::abs(m_vz);
	}

	// 4. Perspective project all 3D world vertices to screen coordinates
	std::vector<float>   wz(nv);
	std::vector<float>   px_f(nv);
	std::vector<float>   py_f(nv);
	std::vector<int16_t> sx_scr(nv);
	std::vector<int16_t> sy_scr(nv);

	for (size_t i = 0; i < nv; ++i) {
		const float xw = m_cx + v3d[i].x;
		const float yw = m_cy + v3d[i].y;
		const float zw = m_cz + v3d[i].z;
		wz[i] = zw;

		const float inv_z = FOCAL / (CAM_DIST - zw);
		const float sx_p  = half_w + xw * inv_z;
		const float sy_p  = half_h + yw * inv_z;
		px_f[i]   = sx_p;
		py_f[i]   = sy_p;
		sx_scr[i] = static_cast<int16_t>(lroundf(sx_p));
		sy_scr[i] = static_cast<int16_t>(lroundf(sy_p));
	}

	// 5. Cull back-facing triangles, depth-sort back-to-front, and draw with Lambertian shading
	struct DrawTri {
		size_t tri_idx;
		float  avg_z;
		float  shade;
	};

	constexpr float LX = -0.38f;
	constexpr float LY = -0.48f;
	constexpr float LZ =  0.79f;

	std::vector<DrawTri> visible;
	visible.reserve(mesh.tris.size());

	for (size_t ti = 0; ti < mesh.tris.size(); ++ti) {
		const TriDef &tri = mesh.tris[ti];
		const uint8_t i0 = tri.idx[0];
		const uint8_t i1 = tri.idx[1];
		const uint8_t i2 = tri.idx[2];

		const float dx1 = px_f[i1] - px_f[i0];
		const float dy1 = py_f[i1] - py_f[i0];
		const float dx2 = px_f[i2] - px_f[i0];
		const float dy2 = py_f[i2] - py_f[i0];
		if ((dx1 * dy2 - dy1 * dx2) <= 0.0f) {
			continue; // Back-facing
		}

		const float ex1 = v3d[i1].x - v3d[i0].x;
		const float ey1 = v3d[i1].y - v3d[i0].y;
		const float ez1 = v3d[i1].z - v3d[i0].z;
		const float ex2 = v3d[i2].x - v3d[i0].x;
		const float ey2 = v3d[i2].y - v3d[i0].y;
		const float ez2 = v3d[i2].z - v3d[i0].z;

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

		visible.push_back({
			ti,
			(wz[i0] + wz[i1] + wz[i2]) * (1.0f / 3.0f),
			0.30f + 0.70f * ndotl
		});
	}

	std::sort(visible.begin(), visible.end(), [](const DrawTri &a, const DrawTri &b) {
		return a.avg_z < b.avg_z; // Farther (-Z) first, nearer (+Z) last
	});

	for (const DrawTri &dt : visible) {
		const TriDef &tri = mesh.tris[dt.tri_idx];
		const uint8_t i0 = tri.idx[0];
		const uint8_t i1 = tri.idx[1];
		const uint8_t i2 = tri.idx[2];

		const uint8_t r = static_cast<uint8_t>(std::min<int>(static_cast<int>(tri.r * dt.shade), 255));
		const uint8_t g = static_cast<uint8_t>(std::min<int>(static_cast<int>(tri.g * dt.shade), 255));
		const uint8_t b = static_cast<uint8_t>(std::min<int>(static_cast<int>(tri.b * dt.shade), 255));
		const uint16_t color = color565(r, g, b);

		m_matrix.fillTriangle(sx_scr[i0], sy_scr[i0], sx_scr[i1], sy_scr[i1], sx_scr[i2], sy_scr[i2], color);

		if (m_wireframe_edges) {
			const uint16_t edge_col = color565(
				static_cast<uint8_t>(std::min<int>(r + 60, 255)),
				static_cast<uint8_t>(std::min<int>(g + 60, 255)),
				static_cast<uint8_t>(std::min<int>(b + 60, 255))
			);
			m_matrix.drawLine(sx_scr[i0], sy_scr[i0], sx_scr[i1], sy_scr[i1], edge_col);
			m_matrix.drawLine(sx_scr[i1], sy_scr[i1], sx_scr[i2], sy_scr[i2], edge_col);
			m_matrix.drawLine(sx_scr[i2], sy_scr[i2], sx_scr[i0], sy_scr[i0], edge_col);
		}
	}
}
