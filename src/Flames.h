#pragma once

#include <vector>
#include "Effect.h"

class Flames : public Effect
{
public:
	// max_decay controls flame height (2 = tall full-screen flames, 3 = ~28px DOOM flames, 5 = low flames)
	Flames(Adafruit_GFX &matrix, uint8_t max_decay = 3);
	void render() override;

private:
	uint16_t             m_width;
	uint16_t             m_height;
	uint8_t              m_max_decay;
	uint32_t             m_rng_state;
	std::vector<uint8_t> m_heat;
};
