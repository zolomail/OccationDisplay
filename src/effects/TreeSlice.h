#pragma once

#include "Effect.h"
#include <cstdint>
#include <vector>

// Renders a pan-and-rotate 32px-tall slice of a procedural 3D Christmas tree
// with rotating 3D pine branches, bushy needle clusters, baubles, trunk, and star.
class TreeSlice : public Effect
{
public:
	TreeSlice(
		Adafruit_GFX &matrix,
		float pan_speed = 0.012f,
		float rot_speed = 0.035f,
		int16_t tree_height = 180
	);
	~TreeSlice() override = default;

	void render() override;

private:
	struct Branch {
		float   y_base;
		float   droop;
		float   cos_theta;
		float   sin_theta;
		float   len_frac;
		float   tuft_r;
		uint8_t seed;
	};

	struct VisibleBranch {
		const Branch *br;
		float depth;
		float sin_p;
	};

	struct Bauble {
		float    y;
		float    cos_theta;
		float    sin_theta;
		float    r_frac;
		uint8_t  radius;
		uint16_t color;
	};

	float    m_pan_speed;
	float    m_rot_speed;
	int16_t  m_tree_height;
	float    m_pan_phase = 0.0f;
	float    m_rot_angle = 0.0f;

	std::vector<Branch>        m_branches;
	std::vector<VisibleBranch> m_vis;
	std::vector<Bauble>        m_baubles;
	std::vector<uint16_t>      m_fb;

	void initTree();
	float coneRadiusAt(float tree_y, float body_top, float body_h, float max_r) const;
	void drawStarRow(int fb_y, float local_y, float star_h, int cx, float cos_rot);
	void drawTrunkRow(int fb_y, float trunk_r, int cx);
	void drawBranch(const VisibleBranch &vb, float pan_y, float body_top, float body_h, float max_r, int cx, int w, int h);
	void drawBauble(int bx, int by, int r, uint16_t base_col, int w, int h);
};
