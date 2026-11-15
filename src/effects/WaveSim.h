#pragma once

#include <vector>
#include "Effect.h"

class WaveSim : public Effect
{
public:
	// drop_interval: frames between random raindrops (0 = disable random drops, 1 = every frame, etc.)
	// enable_boat:   enables a moving sine/cosine "water skier" that carves continuous wakes
	WaveSim(Adafruit_GFX &matrix, uint16_t drop_interval = 5, bool enable_boat = true);
	void render() override;

private:
	void addDrop(int16_t x, int16_t y, int16_t strength);

	uint16_t             m_width;
	uint16_t             m_height;
	uint16_t             m_stride;
	uint16_t             m_drop_interval;
	uint16_t             m_frame_count = 0;
	bool                 m_enable_boat;
	float                m_boat_phase  = 0.0f;
	uint32_t             m_rng_state;
	std::vector<int16_t> m_buf_a;
	std::vector<int16_t> m_buf_b;
	bool                 m_flip        = false;
	uint16_t             m_palette[256];
};
