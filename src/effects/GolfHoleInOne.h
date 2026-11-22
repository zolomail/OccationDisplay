#pragma once

#include "Effect.h"
#include "FlyingPng.h"
#include <memory>

class GolfHoleInOne : public Effect
{
public:
	explicit GolfHoleInOne(Adafruit_GFX& matrix);
	void render() override;

private:
	std::unique_ptr<FlyingPng> m_bg;
	uint16_t                   m_tick = 0;
};
