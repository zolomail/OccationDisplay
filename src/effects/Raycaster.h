#pragma once

#include "Effect.h"
#include <cstdint>
#include <vector>

// Fast Wolfenstein/Doom-style 2.5D DDA raycaster with autonomous maze navigation.
class Raycaster : public Effect
{
public:
	Raycaster(Adafruit_GFX &matrix, float move_speed = 0.045f);
	~Raycaster() override = default;

	void render() override;

private:
	float m_move_speed;
	float m_pos_x = 1.5f;
	float m_pos_y = 1.5f;
	float m_dir_x = 1.0f;
	float m_dir_y = 0.0f;
	float m_plane_x = 0.0f;
	float m_plane_y = 1.15f;
	float m_turn_vel = 0.0f;
	float m_walk_phase = 0.0f;
	int8_t m_turn_bias = 1;
	uint16_t m_bias_timer = 0;

	std::vector<uint16_t> m_fb;

	void updatePlayer();
	float castDistance(float dir_x, float dir_y) const;
	void rotatePlayer(float angle);
};
