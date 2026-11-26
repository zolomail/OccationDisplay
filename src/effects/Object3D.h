#pragma once

#include "Effect.h"
#include <cstdint>

enum class MeshShape : uint8_t {
	Cube = 0,
	Pyramid,
	Octahedron,
	Diamond,
	Icosahedron,
	Torus
};

class Object3D : public Effect
{
public:
	// size: half-extent of the 3D object in world units/pixels (default 8.5)
	// wireframe_edges: if true, draws a subtle highlight along visible triangle edges
	// shape: 3D mesh to render
	// show_box: if true, draws a 3D perspective wireframe box going into the display
	Object3D(
		Adafruit_GFX &matrix,
		float size = 8.5f,
		bool wireframe_edges = false,
		MeshShape shape = MeshShape::Cube,
		bool show_box = true
	);
	void render() override;

private:
	float     m_size;
	bool      m_wireframe_edges;
	MeshShape m_shape;
	bool      m_show_box;

	// Center position and velocity in 3D world space (0,0,0 = center of front viewing plane)
	float m_cx = 0.0f;
	float m_cy = 0.0f;
	float m_cz = -20.0f;
	float m_vx = 1.15f;
	float m_vy = 0.65f;
	float m_vz = 0.85f;

	// 3-axis Euler rotation angles and angular velocities (radians/frame)
	float m_rx  = 0.2f;
	float m_ry  = 0.5f;
	float m_rz  = 0.1f;
	float m_drx = 0.043f;
	float m_dry = 0.061f;
	float m_drz = 0.031f;
};
