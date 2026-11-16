#pragma once

#include "Effect.h"

class Cube3D : public Effect
{
public:
	// size: half-side length of the cube in pixels (default 8.5 -> ~17..24px bounding box)
	// wireframe_edges: if true, draws a subtle highlight along the visible face edges
	Cube3D(Adafruit_GFX &matrix, float size = 8.5f, bool wireframe_edges = false);
	void render() override;

private:
	float m_size;
	bool  m_wireframe_edges;

	// Center position and velocity (pixels/frame)
	float m_cx;
	float m_cy;
	float m_vx = 1.15f;
	float m_vy = 0.65f;

	// 3-axis Euler rotation angles and angular velocities (radians/frame)
	float m_rx  = 0.2f;
	float m_ry  = 0.5f;
	float m_rz  = 0.1f;
	float m_drx = 0.043f;
	float m_dry = 0.061f;
	float m_drz = 0.031f;
};
